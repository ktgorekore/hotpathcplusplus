// Copyright 2026 Hot Path C++ Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file memory_safety.h
 * @brief Episode 14 ("The Physics of Memory Safety: C++20 vs. Rust vs. Java
 * 21") core data structures, generational slab allocator, and non-conflated
 * spatial and temporal safety kernels:
 *
 * 1. Battle 1 — Spatial Memory Safety Physics:
 *    - Regime 1A (Sequential Scan): Compiler induction-variable range check
 *      elimination (`operator[]` vs. hardened `ud2` vs. `std::span` vs.
 *      `std::vector::at()`).
 *    - Regime 1B (Independent Random Batch Gather): Demonstrates how unguarded
 *      indexed gathers (`orders[indices[i]]`) auto-vectorize via AVX-512
 *      `vpgatherqq`, how in-loop traps (`ud2`) inhibit speculative gather
 *      vectorization, and how power-of-two bitmasking (`indices[i] & mask`)
 *      restores 100% spatial safety alongside full SIMD auto-vectorization.
 *    - Regime 1C (Latency-Bound Dependent Probe Chain): Serial data dependency
 *      across order-book lookups where neither unchecked nor checked code can
 *      auto-vectorize, isolating the pure hardware branch-predictor cost of
 *      `cmp + jae ud2` (`0.00%` mispredictions).
 *
 * 2. Battle 2 — Temporal Memory Safety & Same-Size-Class Reuse Physics:
 *    - 32-byte `Order` and 32-byte `AuditEntry` structs sharing the exact same
 *      64-byte allocator size class when stored in pairs, deterministically
 *      demonstrating silent Use-After-Free type confusion vs. AddressSanitizer
 *      shadow-memory traps (`(addr >> 3) + 0x7fff8000`).
 *
 * 3. Battle 3 — Temporal Safety Under High-Churn Order Cancellation:
 *    - Apples-to-apples comparison across:
 *      (a) Unchecked raw slot index / pointer under ABA slot recycling (25%
 *          silent corrupt reads when canceled slots are reused by new orders),
 *      (b) Reference-counted `std::shared_ptr<Order>` / `std::weak_ptr<Order>`
 *          with control-block indirection, and
 *      (c) Contiguous 64-byte aligned `OrderSlabPool` with 64-bit generational
 *          handles (`OrderHandle { uint32_t index; uint32_t generation; }`),
 *          catching 100% of ABA recycled slots in O(1) with L1/L2 locality.
 */

#ifndef HOTPATH_COMMON_MEMORY_SAFETY_MEMORY_SAFETY_H_
#define HOTPATH_COMMON_MEMORY_SAFETY_MEMORY_SAFETY_H_

#include <pthread.h>
#include <sched.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace hotpath::memory_safety {

/** @brief Default power-of-two order-book capacity (`65,536` orders = 2 MiB).
 */
inline constexpr std::size_t kDefaultBookCapacity = 65536;

/** @brief Bitmask for power-of-two `kDefaultBookCapacity` (`0xFFFF`). */
inline constexpr std::uint32_t kDefaultBookMask =
    static_cast<std::uint32_t>(kDefaultBookCapacity - 1);

/** @brief Active buy-side order status flag. */
inline constexpr std::uint32_t kOrderFlagActive = 0x0000'0001U;

/** @brief Canceled order status flag (`0xDEAD'0000`). */
inline constexpr std::uint32_t kOrderFlagCanceled = 0xDEAD'0000U;

/**
 * @brief Prevents the compiler from optimizing away a benchmark accumulator.
 * @tparam T Value type.
 * @param value Value to keep live across optimization passes.
 */
template <typename T>
inline void DoNotOptimize(const T& value) noexcept {
  asm volatile("" : : "r,m"(value) : "memory");
}

/**
 * @brief Clobbers compiler memory assumptions so input buffers are re-read.
 */
inline void ClobberMemory() noexcept { asm volatile("" : : : "memory"); }

/**
 * @brief Pins the calling thread to a specific logical CPU core ID.
 * @param core_id Logical CPU core index (default `0`).
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// ============================================================================
// CORE DATA STRUCTURES (32-BYTE ORDER & 64-BYTE GENERATIONAL SLAB SLOT)
// ============================================================================

/**
 * @brief 32-byte limit-order record (2 orders per 64-byte CPU cache line).
 */
struct alignas(32) Order {
  std::uint64_t order_id;      ///< Monotonic exchange order identifier.
  std::int64_t price_ticks;    ///< Limit price in integer ticks ($0.01/tick).
  std::uint32_t quantity;      ///< Open order quantity in lots.
  std::uint32_t symbol_id;     ///< Numeric instrument identifier.
  std::uint32_t flags;         ///< Status bitmask (`kOrderFlagActive`, etc.).
  std::uint32_t reserved_pad;  ///< Explicit padding to 32 bytes.
};
static_assert(sizeof(Order) == 32, "Order must occupy exactly 32 bytes.");

/**
 * @brief 32-byte audit/telemetry log record sharing the exact same allocator
 * size class as `Order`.
 *
 * When a `std::vector<Order>` of capacity 2 (64 bytes) reallocates to capacity
 * 4 (128 bytes), its freed 64-byte block enters the thread-local allocator
 * cache (`tcache`). Allocating a 2-element `std::vector<AuditEntry>` (64 bytes)
 * deterministically reclaims that exact 64-byte block, placing
 * `sequence_number` at offset `+8` (where `Order::price_ticks` lived).
 */
struct alignas(32) AuditEntry {
  std::uint64_t event_tag;       ///< Overwrites `Order::order_id` (+0).
  std::int64_t sequence_number;  ///< Overwrites `Order::price_ticks` (+8).
  std::uint32_t subsystem_id;    ///< Overwrites `Order::quantity` (+16).
  std::uint32_t severity_code;   ///< Overwrites `Order::symbol_id` (+20).
  std::uint32_t status_bits;     ///< Overwrites `Order::flags` (+24).
  std::uint32_t checksum;        ///< Overwrites `Order::reserved_pad` (+28).
};
static_assert(sizeof(AuditEntry) == sizeof(Order),
              "AuditEntry and Order must share the 32-byte size class.");

/**
 * @brief 64-bit generational handle packing `{index, generation}` into a single
 * 64-bit register (`uint64_t`).
 */
struct OrderHandle {
  std::uint32_t index;       ///< Slot index within `OrderSlabPool`.
  std::uint32_t generation;  ///< Monotonic slot generation counter.

  /**
   * @brief Compares two handles for exact index and generation equality.
   * @param other Handle to compare against.
   * @return True if both `index` and `generation` match.
   */
  constexpr bool operator==(const OrderHandle& other) const noexcept = default;
};
static_assert(sizeof(OrderHandle) == 8,
              "OrderHandle must fit in a single 64-bit general register.");

/**
 * @brief 64-byte cache-line aligned slot inside `OrderSlabPool`.
 */
struct alignas(64) OrderSlabSlot {
  Order order;               ///< 32-byte embedded order payload.
  std::uint32_t generation;  ///< Incremented on every `Release()` (`!= 0`).
  std::uint32_t next_free;   ///< Intrusive free-list index (`UINT32_MAX` end).
  std::uint8_t occupied;     ///< `1` if currently allocated, `0` if free.
  std::uint8_t pad[23];      ///< Pad to 64-byte L1 cache line boundary.
};
static_assert(sizeof(OrderSlabSlot) == 64,
              "OrderSlabSlot must occupy exactly one 64-byte cache line.");

/**
 * @brief Pre-allocated, contiguous generational slab allocator providing O(1)
 * temporal memory safety without heap fragmentation or garbage collection.
 */
class OrderSlabPool {
 public:
  /** @brief Sentinel value marking the end of the intrusive free list. */
  static constexpr std::uint32_t kInvalidIndex = 0xFFFF'FFFFU;

  /**
   * @brief Pre-allocates `capacity` contiguous 64-byte cache-line slots.
   * @param capacity Maximum number of concurrent live orders.
   */
  explicit OrderSlabPool(std::size_t capacity)
      : slots_(capacity), free_head_(0), active_count_(0) {
    for (std::size_t i = 0; i < capacity; ++i) {
      slots_[i].order = Order{0, 0, 0, 0, 0, 0};
      slots_[i].generation = 1U;
      slots_[i].next_free = (i + 1 < capacity)
                                ? static_cast<std::uint32_t>(i + 1)
                                : kInvalidIndex;
      slots_[i].occupied = 0U;
      std::memset(slots_[i].pad, 0, sizeof(slots_[i].pad));
    }
  }

  /**
   * @brief Allocates a slot from the O(1) intrusive free list and returns its
   * generational handle.
   * @param initial Initial order payload.
   * @return Valid `OrderHandle`, or `std::nullopt` if the slab is exhausted.
   */
  [[nodiscard]] std::optional<OrderHandle> Allocate(
      const Order& initial) noexcept {
    if (free_head_ == kInvalidIndex) {
      return std::nullopt;
    }
    const std::uint32_t idx = free_head_;
    OrderSlabSlot& slot = slots_[idx];
    free_head_ = slot.next_free;
    slot.next_free = kInvalidIndex;
    slot.occupied = 1U;
    slot.order = initial;
    ++active_count_;
    return OrderHandle{idx, slot.generation};
  }

  /**
   * @brief Releases a live order slot and bumps its 32-bit generation counter,
   * immediately invalidating all existing `OrderHandle` copies in O(1).
   * @param handle Generational handle to release.
   * @return True if the handle was valid and released; false on double-free or
   * stale handle.
   */
  bool Release(OrderHandle handle) noexcept {
    if (handle.index >= slots_.size()) [[unlikely]] {
      return false;
    }
    OrderSlabSlot& slot = slots_[handle.index];
    if (!slot.occupied || slot.generation != handle.generation) [[unlikely]] {
      return false;
    }
    slot.occupied = 0U;
    slot.order.flags = kOrderFlagCanceled;
    slot.generation =
        (slot.generation + 1U == 0U) ? 1U : (slot.generation + 1U);
    slot.next_free = free_head_;
    free_head_ = handle.index;
    --active_count_;
    return true;
  }

  /**
   * @brief Resolves a generational handle to a const `Order*` with both spatial
   * bounds and temporal generation validation.
   * @param handle Generational handle to validate.
   * @return Pointer to the live `Order`, or `nullptr` if out-of-bounds,
   * canceled, or ABA-recycled.
   */
  [[nodiscard]] const Order* Get(OrderHandle handle) const noexcept {
    if (handle.index >= slots_.size()) [[unlikely]] {
      return nullptr;
    }
    const OrderSlabSlot& slot = slots_[handle.index];
    if (slot.generation != handle.generation || !slot.occupied) [[unlikely]] {
      return nullptr;
    }
    return &slot.order;
  }

  /**
   * @brief Unchecked direct slot access (used ONLY to measure the raw baseline
   * and demonstrate silent ABA corruption when generation checks are omitted).
   * @param index Raw slot index (`0 <= index < Capacity()`).
   * @return Const reference to the underlying `Order` in `slots_[index]`.
   */
  [[nodiscard]] const Order& GetUncheckedSlot(
      std::uint32_t index) const noexcept {
    return slots_[index].order;
  }

  /** @brief Returns total slot capacity of the pool. */
  [[nodiscard]] std::size_t Capacity() const noexcept { return slots_.size(); }

  /** @brief Returns current number of occupied order slots. */
  [[nodiscard]] std::size_t ActiveCount() const noexcept {
    return active_count_;
  }

 private:
  std::vector<OrderSlabSlot> slots_;
  std::uint32_t free_head_;
  std::size_t active_count_;
};

// ============================================================================
// BATTLE 1: SPATIAL MEMORY SAFETY KERNELS (ZERO COMPILER PRAGMA HACKS)
// ============================================================================

/**
 * @brief Deterministic dataset for Battle 1 spatial bounds-check benchmarks.
 */
struct SpatialDataset {
  std::vector<Order> orders;              ///< Contiguous 32B orders.
  std::vector<std::uint32_t> random_idx;  ///< Permuted in-bounds indices.

  /**
   * @brief Builds a deterministic dataset of `count` active orders and a
   * pseudo-random index permutation.
   * @param count Number of orders (must be a power of two for bitmask tests).
   */
  explicit SpatialDataset(std::size_t count = kDefaultBookCapacity)
      : orders(count), random_idx(count) {
    std::uint64_t state = 0xCAFE'BABE'1337'BEEFULL;
    for (std::size_t i = 0; i < count; ++i) {
      orders[i] = Order{
          .order_id = static_cast<std::uint64_t>(i + 1),
          .price_ticks = static_cast<std::int64_t>(10000 + (i & 1023)),
          .quantity = static_cast<std::uint32_t>(1 + (i & 63)),
          .symbol_id = static_cast<std::uint32_t>(i & 15),
          .flags = kOrderFlagActive,
          .reserved_pad = 0U,
      };
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      random_idx[i] = static_cast<std::uint32_t>(state % count);
    }
  }
};

/**
 * @brief Regime 1A.1: Sequential scan using unchecked raw pointer indexing.
 * @param orders Pointer to contiguous `Order` array.
 * @param count Number of elements.
 * @return Total notional value (`sum(price_ticks * quantity)`).
 */
inline std::int64_t ScanSequentialUnchecked(const Order* orders,
                                            std::size_t count) noexcept {
  std::int64_t total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    total +=
        orders[i].price_ticks * static_cast<std::int64_t>(orders[i].quantity);
  }
  return total;
}

/**
 * @brief Regime 1A.2: Sequential scan with explicit hardened fast-trap bounds
 * check (`__builtin_trap()` / `ud2`, matching `_LIBCPP_HARDENING_MODE_FAST`).
 *
 * Notice that at `-O3`, LLVM/GCC induction-variable range analysis proves
 * `i < count` is invariant and eliminates the `ud2` check from the loop body.
 *
 * @param orders Contiguous `std::vector<Order>`.
 * @return Total notional value.
 */
inline std::int64_t ScanSequentialHardenedTrap(
    const std::vector<Order>& orders) noexcept {
  std::int64_t total = 0;
  const std::size_t n = orders.size();
  const Order* data = orders.data();
  for (std::size_t i = 0; i < n; ++i) {
    if (__builtin_expect(i >= n, 0)) {
      __builtin_trap();
    }
    total += data[i].price_ticks * static_cast<std::int64_t>(data[i].quantity);
  }
  return total;
}

/**
 * @brief Regime 1A.3: Sequential scan using C++20 `std::span<const Order>`.
 * @param orders Span view over contiguous `Order` storage.
 * @return Total notional value.
 */
inline std::int64_t ScanSequentialSpan(std::span<const Order> orders) noexcept {
  std::int64_t total = 0;
  for (std::size_t i = 0; i < orders.size(); ++i) {
    total +=
        orders[i].price_ticks * static_cast<std::int64_t>(orders[i].quantity);
  }
  return total;
}

/**
 * @brief Regime 1A.4: Sequential scan using exception-throwing
 * `std::vector::at(i)`.
 * @param orders Contiguous `std::vector<Order>`.
 * @return Total notional value.
 */
inline std::int64_t ScanSequentialVectorAt(const std::vector<Order>& orders) {
  std::int64_t total = 0;
  const std::size_t n = orders.size();
  for (std::size_t i = 0; i < n; ++i) {
    const Order& o = orders.at(i);
    total += o.price_ticks * static_cast<std::int64_t>(o.quantity);
  }
  return total;
}

/**
 * @brief Regime 1B.1: Independent random batch gather using unchecked pointer
 * indexing (`orders[indices[i]]`).
 *
 * Because there is no conditional trap inside the loop body, Clang/GCC at
 * `-O3 -march=native` can auto-vectorize this loop using AVX-512 `vpgatherqq`.
 *
 * @param orders Pointer to contiguous `Order` array.
 * @param indices Pointer to lookup indices.
 * @param count Number of lookups.
 * @return Total notional value across gathered orders.
 */
inline std::int64_t GatherBatchUnchecked(const Order* orders,
                                         const std::uint32_t* indices,
                                         std::size_t count) noexcept {
  std::int64_t total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const std::uint32_t idx = indices[i];
    total += orders[idx].price_ticks *
             static_cast<std::int64_t>(orders[idx].quantity);
  }
  return total;
}

/**
 * @brief Regime 1B.2: Independent random batch gather with hardened in-loop
 * trap (`if (idx >= n) __builtin_trap()`).
 *
 * The conditional trap (`cmp + jae ud2`) prevents the compiler from issuing a
 * speculative 8-lane AVX-512 `vpgatherqq` before validating all 8 indices,
 * forcing scalar loop execution.
 *
 * @param orders Contiguous `std::vector<Order>`.
 * @param indices Lookup index vector.
 * @return Total notional value across gathered orders.
 */
inline std::int64_t GatherBatchHardenedTrap(
    const std::vector<Order>& orders,
    const std::vector<std::uint32_t>& indices) noexcept {
  std::int64_t total = 0;
  const std::size_t n = orders.size();
  const Order* data = orders.data();
  const std::size_t m = indices.size();
  for (std::size_t i = 0; i < m; ++i) {
    const std::uint32_t idx = indices[i];
    if (__builtin_expect(idx >= n, 0)) {
      __builtin_trap();
    }
    total +=
        data[idx].price_ticks * static_cast<std::int64_t>(data[idx].quantity);
  }
  return total;
}

/**
 * @brief Regime 1B.3: Independent random batch gather using `std::span` under
 * standard library bounds assertions (`-D_GLIBCXX_ASSERTIONS`).
 * @param orders Span over contiguous `Order` storage.
 * @param indices Span over lookup indices.
 * @return Total notional value across gathered orders.
 */
inline std::int64_t GatherBatchSpan(
    std::span<const Order> orders,
    std::span<const std::uint32_t> indices) noexcept {
  std::int64_t total = 0;
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const std::uint32_t idx = indices[i];
    total += orders[idx].price_ticks *
             static_cast<std::int64_t>(orders[idx].quantity);
  }
  return total;
}

/**
 * @brief Regime 1B.4: Independent random batch gather using `std::vector::at()`
 * (throws `std::out_of_range` on failure).
 * @param orders Contiguous `std::vector<Order>`.
 * @param indices Lookup index vector.
 * @return Total notional value across gathered orders.
 */
inline std::int64_t GatherBatchVectorAt(
    const std::vector<Order>& orders,
    const std::vector<std::uint32_t>& indices) {
  std::int64_t total = 0;
  const std::size_t m = indices.size();
  for (std::size_t i = 0; i < m; ++i) {
    const Order& o = orders.at(indices[i]);
    total += o.price_ticks * static_cast<std::int64_t>(o.quantity);
  }
  return total;
}

/**
 * @brief Regime 1B.5: Branchless power-of-two bitmask spatial safety
 * (`orders[indices[i] & mask]`).
 *
 * By enforcing a power-of-two slab capacity (`2^16 = 65,536`) and masking the
 * untrusted index with `mask = capacity - 1`, out-of-bounds memory access is
 * mathematically impossible with **zero conditional branches**, restoring full
 * AVX-512 `vpgatherqq` auto-vectorization!
 *
 * @param orders Pointer to power-of-two `Order` slab.
 * @param indices Pointer to lookup indices.
 * @param count Number of lookups.
 * @param mask Power-of-two bitmask (`capacity - 1`).
 * @return Total notional value across gathered orders.
 */
inline std::int64_t GatherBatchBitmaskSafe(
    const Order* orders, const std::uint32_t* indices, std::size_t count,
    std::uint32_t mask = kDefaultBookMask) noexcept {
  std::int64_t total = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const std::uint32_t safe_idx = indices[i] & mask;
    total += orders[safe_idx].price_ticks *
             static_cast<std::int64_t>(orders[safe_idx].quantity);
  }
  return total;
}

/**
 * @brief Regime 1C.1: Latency-bound dependent probe chain (Unchecked).
 *
 * Models a realistic order-book hash-table probe or price-level chain where
 * step `i`'s lookup index depends on step `i - 1`'s loaded order state.
 * Because of the loop-carried data dependency, neither unchecked nor checked
 * variants can auto-vectorize, isolating the pure hardware cost of `cmp + jae`.
 *
 * @param orders Pointer to contiguous `Order` array.
 * @param indices Pointer to base probe indices.
 * @param count Number of dependent probes.
 * @param mask Power-of-two book mask (`capacity - 1`) used by the probe step.
 * @return Accumulated notional chain value.
 */
inline std::int64_t ProbeDependentUnchecked(
    const Order* orders, const std::uint32_t* indices, std::size_t count,
    std::uint32_t mask = kDefaultBookMask) noexcept {
  std::int64_t total = 0;
  std::uint32_t prev_mix = 0U;
  for (std::size_t i = 0; i < count; ++i) {
    const std::uint32_t idx = (indices[i] ^ prev_mix) & mask;
    const Order& o = orders[idx];
    const std::int64_t notional =
        o.price_ticks * static_cast<std::int64_t>(o.quantity);
    total += notional;
    prev_mix = static_cast<std::uint32_t>(notional) & 0xFFU;
  }
  return total;
}

/**
 * @brief Regime 1C.2: Latency-bound dependent probe chain with hardened fast
 * trap (`if (idx >= n) __builtin_trap()`).
 * @param orders Contiguous `std::vector<Order>`.
 * @param indices Base probe index vector.
 * @param mask Power-of-two book mask (`capacity - 1`).
 * @return Accumulated notional chain value.
 */
inline std::int64_t ProbeDependentHardenedTrap(
    const std::vector<Order>& orders, const std::vector<std::uint32_t>& indices,
    std::uint32_t mask = kDefaultBookMask) noexcept {
  std::int64_t total = 0;
  std::uint32_t prev_mix = 0U;
  const std::size_t n = orders.size();
  const Order* data = orders.data();
  const std::size_t m = indices.size();
  for (std::size_t i = 0; i < m; ++i) {
    const std::uint32_t idx = (indices[i] ^ prev_mix) & mask;
    if (__builtin_expect(idx >= n, 0)) {
      __builtin_trap();
    }
    const Order& o = data[idx];
    const std::int64_t notional =
        o.price_ticks * static_cast<std::int64_t>(o.quantity);
    total += notional;
    prev_mix = static_cast<std::uint32_t>(notional) & 0xFFU;
  }
  return total;
}

/**
 * @brief Regime 1C.3: Latency-bound dependent probe chain using
 * `std::vector::at()`.
 * @param orders Contiguous `std::vector<Order>`.
 * @param indices Base probe index vector.
 * @param mask Power-of-two book mask (`capacity - 1`).
 * @return Accumulated notional chain value.
 */
inline std::int64_t ProbeDependentVectorAt(
    const std::vector<Order>& orders, const std::vector<std::uint32_t>& indices,
    std::uint32_t mask = kDefaultBookMask) {
  std::int64_t total = 0;
  std::uint32_t prev_mix = 0U;
  const std::size_t m = indices.size();
  for (std::size_t i = 0; i < m; ++i) {
    const std::uint32_t idx = (indices[i] ^ prev_mix) & mask;
    const Order& o = orders.at(idx);
    const std::int64_t notional =
        o.price_ticks * static_cast<std::int64_t>(o.quantity);
    total += notional;
    prev_mix = static_cast<std::uint32_t>(notional) & 0xFFU;
  }
  return total;
}

// ============================================================================
// BATTLE 3: TEMPORAL SAFETY UNDER ORDER CANCELLATION & ABA SLOT RECYCLING
// ============================================================================

/**
 * @brief Forensic result of a temporal lookup pass across `65,536` order
 * references after `25%` (`16,384`) of orders have been canceled and their
 * slots recycled by new incoming orders.
 */
struct TemporalAuditResult {
  std::int64_t total_notional;         ///< Sum of notional across active reads.
  std::size_t valid_active_reads;      ///< Reads matching original live order.
  std::size_t rejected_stale_handles;  ///< Canceled/ABA handles safely caught.
  std::size_t silent_aba_corruptions;  ///< Recycled slots misread as original!
};

/**
 * @brief Pre-constructed order-book churn scenario for Battle 3:
 * - Allocates `count` (`65,536`) orders (`order_id = 1..65536`).
 * - Cancels every 4th order (`25%` = `16,384` orders) and immediately recycles
 *   those freed slots with new incoming orders (`order_id = 1,000,000 + i`,
 *   `price_ticks = 999,999`).
 * - Provides identical state for:
 *   1. Unchecked raw slot index / pointer lookup (`OrderSlabPool`),
 *   2. Safe 64-bit `OrderHandle` generational lookup (`OrderSlabPool`), and
 *   3. Reference-counted `std::shared_ptr<Order>` / `std::weak_ptr<Order>`.
 */
struct TemporalChurnScenario {
  OrderSlabPool pool;
  std::vector<OrderHandle> original_handles;
  std::vector<std::uint32_t> lookup_order;
  std::vector<std::shared_ptr<Order>> live_shared_book;
  std::vector<std::weak_ptr<Order>> original_weak_handles;

  /**
   * @brief Constructs and populates the 25% cancellation + ABA recycling
   * scenario.
   * @param count Total order slots (`65,536` default).
   */
  explicit TemporalChurnScenario(std::size_t count = kDefaultBookCapacity)
      : pool(count),
        original_handles(count),
        lookup_order(count),
        live_shared_book(count),
        original_weak_handles(count) {
    for (std::size_t i = 0; i < count; ++i) {
      Order initial{
          .order_id = static_cast<std::uint64_t>(i + 1),
          .price_ticks = static_cast<std::int64_t>(10000 + (i & 1023)),
          .quantity = static_cast<std::uint32_t>(1 + (i & 63)),
          .symbol_id = static_cast<std::uint32_t>(i & 15),
          .flags = kOrderFlagActive,
          .reserved_pad = 0U,
      };
      original_handles[i] = *pool.Allocate(initial);
      live_shared_book[i] = std::make_shared<Order>(initial);
      original_weak_handles[i] = live_shared_book[i];
    }

    // Cancel 25% of orders (every 4th order) and immediately recycle each slot
    // with a newly arriving replacement order (ABA slot reuse).
    for (std::size_t i = 0; i < count; i += 4) {
      pool.Release(original_handles[i]);
      Order replacement{
          .order_id = static_cast<std::uint64_t>(1'000'000 + i),
          .price_ticks = 999'999LL,
          .quantity = 100U,
          .symbol_id = static_cast<std::uint32_t>(i & 15),
          .flags = kOrderFlagActive,
          .reserved_pad = 0U,
      };
      (void)pool.Allocate(replacement);

      // In the shared_ptr/weak_ptr book, replacing the slot drops the old
      // shared_ptr (expiring original_weak_handles[i]) and allocates a new one.
      live_shared_book[i] = std::make_shared<Order>(replacement);
    }

    std::uint64_t state = 0x9E37'79B9'7F4A'7C15ULL;
    for (std::size_t i = 0; i < count; ++i) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      lookup_order[i] = static_cast<std::uint32_t>(state % count);
    }
  }
};

/**
 * @brief Battle 3.1 (Hot Loop): Unchecked raw slot-index lookup checking only
 * `order.flags == kOrderFlagActive`.
 *
 * Performs the exact same arithmetic inside the timed loop as
 * `RunTemporalGenerationalHotLoop()`. Because recycled slots have
 * `flags == kOrderFlagActive`, this unchecked path silently folds the
 * replacement order's `999,999 * 100` notional into the caller's total!
 *
 * @param pool `OrderSlabPool` after 25% ABA slot recycling.
 * @param handles Original handles held by the gateway.
 * @param lookup_order Permuted indices into `handles`.
 * @return Total notional accumulated (corrupted by 25% ABA slot aliasing).
 */
inline std::int64_t RunTemporalUncheckedSlotHotLoop(
    const OrderSlabPool& pool, const std::vector<OrderHandle>& handles,
    const std::vector<std::uint32_t>& lookup_order) noexcept {
  std::int64_t total_notional = 0;
  const std::size_t n = lookup_order.size();
  for (std::size_t i = 0; i < n; ++i) {
    const OrderHandle h = handles[lookup_order[i]];
    const Order& order = pool.GetUncheckedSlot(h.index);
    if (order.flags == kOrderFlagActive) {
      total_notional +=
          order.price_ticks * static_cast<std::int64_t>(order.quantity);
    }
  }
  return total_notional;
}

/**
 * @brief Battle 3.2 (Hot Loop): Safe 64-bit generational handle lookup via
 * `OrderSlabPool::Get(OrderHandle)`.
 *
 * Validates both `h.index < capacity` and `slot.generation == h.generation`,
 * rejecting 100% of ABA-recycled slots in O(1) while scanning contiguous
 * 64-byte cache lines.
 *
 * @param pool `OrderSlabPool` after 25% ABA slot recycling.
 * @param handles Original handles held by the gateway.
 * @param lookup_order Permuted indices into `handles`.
 * @return Exact uncorrupted notional across the 75% surviving orders.
 */
inline std::int64_t RunTemporalGenerationalHotLoop(
    const OrderSlabPool& pool, const std::vector<OrderHandle>& handles,
    const std::vector<std::uint32_t>& lookup_order) noexcept {
  std::int64_t total_notional = 0;
  const std::size_t n = lookup_order.size();
  for (std::size_t i = 0; i < n; ++i) {
    const OrderHandle h = handles[lookup_order[i]];
    const Order* order = pool.Get(h);
    if (order != nullptr) {
      total_notional +=
          order->price_ticks * static_cast<std::int64_t>(order->quantity);
    }
  }
  return total_notional;
}

/**
 * @brief Battle 3.3 (Hot Loop): Reference-counted temporal safety via
 * `std::weak_ptr<Order>::lock()`.
 *
 * Prevents Use-After-Free and ABA corruption via heap control blocks, but pays
 * for atomic reference-count updates (`shared_use_count` CAS) and pointer
 * chasing across non-contiguous heap allocations.
 *
 * @param weak_handles Original `std::weak_ptr<Order>` handles.
 * @param lookup_order Permuted indices into `weak_handles`.
 * @return Exact uncorrupted notional across the 75% surviving orders.
 */
inline std::int64_t RunTemporalWeakPtrHotLoop(
    const std::vector<std::weak_ptr<Order>>& weak_handles,
    const std::vector<std::uint32_t>& lookup_order) noexcept {
  std::int64_t total_notional = 0;
  const std::size_t n = lookup_order.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (std::shared_ptr<Order> locked = weak_handles[lookup_order[i]].lock()) {
      total_notional +=
          locked->price_ticks * static_cast<std::int64_t>(locked->quantity);
    }
  }
  return total_notional;
}

/**
 * @brief Performs a full forensic audit comparing Unchecked Slot Lookup vs.
 * Safe 64-Bit Generational Handle Lookup across all `count` handles.
 * @param scenario Populated `TemporalChurnScenario`.
 * @param use_generational_check True to audit `pool.Get(h)`; false to audit
 * raw `pool.GetUncheckedSlot(h.index)`.
 * @return Detailed `TemporalAuditResult` including `silent_aba_corruptions`.
 */
[[nodiscard]] inline TemporalAuditResult AuditTemporalLookups(
    const TemporalChurnScenario& scenario,
    bool use_generational_check) noexcept {
  TemporalAuditResult result{0, 0, 0, 0};
  const std::size_t n = scenario.original_handles.size();
  for (std::size_t i = 0; i < n; ++i) {
    const OrderHandle h = scenario.original_handles[i];
    const std::uint64_t expected_order_id = static_cast<std::uint64_t>(i + 1);
    if (use_generational_check) {
      const Order* order = scenario.pool.Get(h);
      if (order == nullptr) {
        ++result.rejected_stale_handles;
        continue;
      }
      if (order->order_id != expected_order_id) {
        ++result.silent_aba_corruptions;
      } else {
        ++result.valid_active_reads;
      }
      result.total_notional +=
          order->price_ticks * static_cast<std::int64_t>(order->quantity);
    } else {
      const Order& order = scenario.pool.GetUncheckedSlot(h.index);
      if (order.flags != kOrderFlagActive) {
        ++result.rejected_stale_handles;
        continue;
      }
      if (order.order_id != expected_order_id) {
        ++result.silent_aba_corruptions;
      } else {
        ++result.valid_active_reads;
      }
      result.total_notional +=
          order.price_ticks * static_cast<std::int64_t>(order.quantity);
    }
  }
  return result;
}

}  // namespace hotpath::memory_safety

namespace hotpath::ep14 {
using namespace ::hotpath::memory_safety;
}  // namespace hotpath::ep14

#endif  // HOTPATH_COMMON_MEMORY_SAFETY_MEMORY_SAFETY_H_
