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
 * @file cpp_vs_java_jit.h
 * @brief Episode 13 C++20 vs. Java 21 (HotSpot C2 JIT, Cache-Line Physics &
 * GC Tail-Latency) forensic definitions and benchmark primitives:
 * 1. Battle 1 (Allocation & Polymorphic Dispatch):
 *    - `OrderResult`: Heap/register result object (`notional`, `fee_ticks`).
 *    - `Event`: Polymorphic base class with cross-TU virtual methods
 *      `ProcessHeapAlloc()` and `ProcessNoAlloc()`.
 *    - `MakeEventByType()` & `ConstructEventInPlace()`: Separate-TU factories.
 *    - `FlatEvent`: Contiguous value struct with `ProcessInline()`.
 * 2. Battle 2 (Cache-Line Physics across 4,000,000 Active 32-Byte Orders):
 *    - `Order`: `alignas(32)` 32-byte market order struct (2 per 64B cache line
 *      with 0-byte object header vs. 52 bytes in Java `ArrayList<Order>`).
 *    - `OrderBookSoA`: Dense Structure-of-Arrays for SIMD vectorization.
 * 3. Battle 3 (Tail-Latency Histogram p50..p99.99 & Rare-Event AOT Execution):
 *    - `RunBattle3SlabTailLatencyAndDeoptCheck()`: Pre-faulted (`MAP_POPULATE`)
 *      slab pool mutating 50,000 batches of 256 messages across 2,097,152 live
 *      orders, plus rare `MarketHaltEvent` Ahead-of-Time execution check.
 */

#ifndef HOTPATH_COMMON_CPP_VS_JAVA_JIT_CPP_VS_JAVA_JIT_H_
#define HOTPATH_COMMON_CPP_VS_JAVA_JIT_CPP_VS_JAVA_JIT_H_

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace hotpath::cpp_vs_java_jit {

/** @brief Number of events in the Battle 1 pre-allocated ring buffer (4,096).
 */
inline constexpr std::size_t kBattle1BatchSize = 4096;

/** @brief Number of active 32-byte orders in Battle 2 (4,000,000 = 128 MB). */
inline constexpr std::size_t kBattle2OrderCount = 4'000'000;

/** @brief Active buy-side order flag in Battle 2 & 3 (`flags == 1U`). */
inline constexpr std::uint32_t kOrderFlagActiveBuy = 1U;

/**
 * @brief Prevents the compiler from optimizing away a benchmark value or loop.
 * @tparam T Value type.
 * @param value Value to preserve across optimization passes.
 */
template <typename T>
inline void DoNotOptimize(const T& value) noexcept {
  asm volatile("" : : "r,m"(value) : "memory");
}

/**
 * @brief Pins the calling thread to a specific physical CPU core ID.
 * @param core_id Logical CPU core index (default 0).
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// ============================================================================
// BATTLE 1: ALLOCATION & POLYMORPHIC DISPATCH (C++ vs. Java C2 JIT)
// ============================================================================

/**
 * @brief Result object allocated per event in the naive OOP pipeline.
 */
struct OrderResult {
  std::int64_t notional;
  std::int64_t fee_ticks;
};

/**
 * @brief Base polymorphic event (mirrors Java `Event` interface / abstract
 * base class).
 *
 * <p><b>Thread Safety:</b> Immutable after construction (`Wait-Free` reads).
 */
struct Event {
  std::int64_t price;
  std::int64_t qty;
  std::uint32_t symbol_id;
  std::uint8_t type_tag;

  /**
   * @brief Constructs a base `Event`.
   * @param p Order price in ticks.
   * @param q Order quantity in lots.
   * @param s Instrument symbol identifier.
   * @param t Event subclass tag (`0..4`).
   */
  Event(std::int64_t p, std::int64_t q, std::uint32_t s,
        std::uint8_t t) noexcept
      : price(p), qty(q), symbol_id(s), type_tag(t) {}

  virtual ~Event() = default;

  /**
   * @brief Virtual dispatch across translation unit boundary that allocates a
   * heap `OrderResult` (`std::make_unique<OrderResult>`).
   * @return Newly allocated `OrderResult`.
   */
  [[nodiscard]] virtual std::unique_ptr<OrderResult> ProcessHeapAlloc()
      const = 0;

  /**
   * @brief Virtual dispatch across translation unit boundary without heap
   * allocation (isolates pure indirect vtable call cost).
   * @return Net notional after fee deduction.
   */
  [[nodiscard]] virtual std::int64_t ProcessNoAlloc() const noexcept = 0;
};

/**
 * @brief Cross-TU factory function defined in `cpp_vs_java_jit_oop_tu.cc`
 * (opaque to `benchmark.cc` without Link-Time Optimization).
 *
 * @param type_tag Subclass discriminator (`0` = LimitOrder, `1` = CancelOrder,
 * `2` = ModifyOrder, `3` = PeggedOrder, `4` = MarketHalt).
 * @param price Order price in integer ticks.
 * @param qty Order quantity in lots.
 * @param symbol_id Instrument symbol identifier.
 * @return Heap-allocated polymorphic `std::unique_ptr<Event>`.
 */
[[nodiscard]] std::unique_ptr<Event> MakeEventByType(std::uint8_t type_tag,
                                                     std::int64_t price,
                                                     std::int64_t qty,
                                                     std::uint32_t symbol_id);

/**
 * @brief Constructs a polymorphic `Event` subclass in-place inside `arena_slot`
 * using placement `new` across a translation unit boundary.
 *
 * @param arena_slot Aligned storage buffer of at least `sizeof(Event)` bytes.
 * @param type_tag Subclass discriminator (`0..4`).
 * @param price Order price in integer ticks.
 * @param qty Order quantity in lots.
 * @param symbol_id Instrument symbol identifier.
 */
void ConstructEventInPlace(void* arena_slot, std::uint8_t type_tag,
                           std::int64_t price, std::int64_t qty,
                           std::uint32_t symbol_id);

/**
 * @brief Flat value struct for idiomatic Hot-Path C++20 (zero vptr, zero heap
 * allocation, contiguous array packing).
 */
struct FlatEvent {
  std::int64_t price;
  std::int64_t qty;
  std::uint32_t symbol_id;
  std::uint8_t type_tag;

  /**
   * @brief Processes the event inline by value with zero heap allocation and
   * zero indirect vtable branches.
   * @return Net notional score (identical math to `Event::ProcessNoAlloc()`).
   */
  [[nodiscard]] [[gnu::always_inline]] inline std::int64_t ProcessInline()
      const noexcept {
    switch (type_tag) {
      case 0:  // LimitOrderEvent (99.9% hot in monomorphic profile)
        return (price * qty) - ((price * qty) >> 10);
      case 1:  // CancelOrderEvent
        return -(price * qty);
      case 2:  // ModifyOrderEvent
        return (price * (qty + 1)) - ((price * qty) >> 9);
      case 3:  // PeggedOrderEvent
        return (price + 2) * qty;
      default:  // MarketHaltEvent (rare emergency packet)
        return (price * qty * 3) ^ static_cast<std::int64_t>(symbol_id);
    }
  }
};

// ============================================================================
// BATTLE 2: CACHE-LINE PHYSICS (4,000,000 Active 32-Byte Orders)
// ============================================================================

/**
 * @brief 32-Byte Market Order for Battle 2 (Cache-Line Physics).
 *
 * <p>Packs 2 headerless 32-byte `Order` structs into every 64-byte L1D cache
 * line, compared to 52 bytes per order (`12B` object header + `32B` fields +
 * `4B` alignment padding + `4B` Compressed OOP reference) in Java's
 * `ArrayList<Order>`.
 */
struct alignas(32) Order {
  std::uint64_t order_id;   // 8B
  std::int64_t price;       // 8B
  std::int64_t qty;         // 8B
  std::uint32_t symbol_id;  // 4B
  std::uint32_t flags;      // 4B (1 = active buy, 2 = active sell, 0 = cancel)
};
static_assert(sizeof(Order) == 32,
              "Order must be exactly 32 bytes (2 per 64B cache line)");

/**
 * @brief Structure-of-Arrays (SoA) order book aligned for AVX2 / AVX-512 SIMD
 * vectorization.
 */
struct OrderBookSoA {
  std::vector<std::int64_t> prices;
  std::vector<std::int64_t> qtys;
  std::vector<std::int64_t> masks;  // -1LL if active buy, 0LL otherwise

  /**
   * @brief Populates `n` deterministic orders in columnar arrays.
   * @param n Number of orders.
   */
  explicit OrderBookSoA(std::size_t n) : prices(n), qtys(n), masks(n) {
    for (std::size_t i = 0; i < n; ++i) {
      prices[i] = static_cast<std::int64_t>(1000 + (i & 1023));
      qtys[i] = static_cast<std::int64_t>(1 + (i & 31));
      masks[i] = -1LL;
    }
  }
};

// ============================================================================
// BATTLE 3: TAIL-LATENCY HISTOGRAM & RARE-EVENT SPIKE (C++ SLAB POOL)
// ============================================================================

/**
 * @brief Summary of Battle 3 C++20 pre-faulted slab pool tail-latency
 * percentiles and rare `MarketHaltEvent` execution latency.
 */
struct Battle3TailMetrics {
  double p50_us;
  double p99_us;
  double p99_9_us;
  double p99_99_us;
  double max_us;
  double rare_event_ns;
  std::uint64_t checksum;
};

/**
 * @brief Executes the Battle 3 sustained state-mutation benchmark on a
 * pre-faulted (`MAP_POPULATE`) C++20 slab arena (`2,097,152` active 32-byte
 * orders = `64 MiB`, `50,000` batches x `256` messages = `12.8M` mutations) and
 * measures the Ahead-of-Time `-O3` latency of a rare `MarketHaltEvent`.
 *
 * @param print_report If true, prints the formatted telemetry table to stdout.
 * @return Populated `Battle3TailMetrics`.
 */
inline Battle3TailMetrics RunBattle3SlabTailLatencyAndDeoptCheck(
    bool print_report = true) {
  PinThreadToCore(0);
  constexpr std::size_t kSlabCapacity = 2'097'152;  // 2M * 32B = 64 MiB ring
  constexpr std::size_t kNumBatches = 50'000;
  constexpr std::size_t kBatchMsgs = 256;  // 12.8M total messages mutated

  const std::size_t bytes = kSlabCapacity * sizeof(Order);
  void* raw = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  Order* slab = static_cast<Order*>(raw);
  std::memset(slab, 0, bytes);

  std::vector<std::uint64_t> batch_latencies_ns;
  batch_latencies_ns.reserve(kNumBatches);

  std::uint64_t rolling_checksum = 0;
  std::size_t cursor = 0;

  for (std::size_t b = 0; b < kNumBatches; ++b) {
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t m = 0; m < kBatchMsgs; ++m) {
      const std::size_t slot = (cursor++) & (kSlabCapacity - 1);
      slab[slot].order_id = cursor;
      slab[slot].price = 1000 + static_cast<std::int64_t>(m & 63);
      slab[slot].qty = 10 + static_cast<std::int64_t>(m & 15);
      slab[slot].symbol_id = static_cast<std::uint32_t>(b & 255);
      slab[slot].flags = kOrderFlagActiveBuy;

      // Read a neighbor order to simulate order-book matching lookup
      const std::size_t match_slot = (slot + 1024) & (kSlabCapacity - 1);
      rolling_checksum +=
          static_cast<std::uint64_t>(slab[match_slot].price * slab[slot].qty);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const std::uint64_t ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    batch_latencies_ns.push_back(ns);
  }

  ::munmap(raw, bytes);
  DoNotOptimize(rolling_checksum);

  std::sort(batch_latencies_ns.begin(), batch_latencies_ns.end());
  auto pct = [&](double p) -> double {
    const std::size_t i = static_cast<std::size_t>(
        p * static_cast<double>(batch_latencies_ns.size() - 1));
    return static_cast<double>(batch_latencies_ns[i]) / 1000.0;
  };

  // Rare Event ("MarketHaltEvent") Latency Check after 100,000 Monomorphic
  // Events
  const FlatEvent warm_ev{105, 20, 42, 0};
  std::int64_t acc = 0;
  for (int i = 0; i < 100'000; ++i) {
    acc += warm_ev.ProcessInline();
  }
  DoNotOptimize(acc);

  const FlatEvent rare_halt_ev{105, 20, 42, 4};  // Type 4 = MarketHaltEvent
  const auto r0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000; ++i) {
    acc += rare_halt_ev.ProcessInline();
    DoNotOptimize(acc);
  }
  const auto r1 = std::chrono::steady_clock::now();
  const double rare_event_ns =
      static_cast<double>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(r1 - r0)
              .count()) /
      1000.0;

  const Battle3TailMetrics metrics{
      .p50_us = pct(0.50),
      .p99_us = pct(0.99),
      .p99_9_us = pct(0.999),
      .p99_99_us = pct(0.9999),
      .max_us = pct(1.00),
      .rare_event_ns = rare_event_ns,
      .checksum = rolling_checksum ^ static_cast<std::uint64_t>(acc),
  };

  if (print_report) {
    std::printf(
        "\n===================================================================="
        "\n");
    std::printf(
        "C++20 BATTLE 3 TELEMETRY: PRE-FAULTED SLAB POOL (256 Msgs / Batch)\n");
    std::printf(
        "===================================================================="
        "\n");
    std::printf("  p50.00 Latency : %8.3f us\n", metrics.p50_us);
    std::printf("  p99.00 Latency : %8.3f us\n", metrics.p99_us);
    std::printf("  p99.90 Latency : %8.3f us\n", metrics.p99_9_us);
    std::printf("  p99.99 Latency : %8.3f us\n", metrics.p99_99_us);
    std::printf("  Max    Latency : %8.3f us\n", metrics.max_us);
    std::printf(
        "  Rare MarketHaltEvent Latency (AOT -O3, 0 Deopt) : %.2f ns/op\n",
        metrics.rare_event_ns);
    std::printf(
        "====================================================================\n"
        "\n");
  }
  return metrics;
}

}  // namespace hotpath::cpp_vs_java_jit

#endif  // HOTPATH_COMMON_CPP_VS_JAVA_JIT_CPP_VS_JAVA_JIT_H_
