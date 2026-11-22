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
 * @file memory_allocators.h
 * @brief Episode 12 ("All Things Memory Allocation — From malloc & new to
 * Pools, Arenas & C++17 std::pmr") production allocator primitives, domain
 * layouts, and forensic helpers:
 *
 * 1. Domain Layouts:
 *    - `OrderNode`: Exact 32-byte intrusive order record (`sizeof == 32`). Two
 *      nodes pack cleanly into one 64-byte CPU cache line inside a custom
 *      arena or slab, whereas `glibc` `ptmalloc2` rounds `32B` + `8B` header
 *      up to a `48B` chunk (`50%` metadata bloat and cache-line straddling).
 *    - `StdVenueFill`, `BrokenPmrVenueFill`, and `PmrVenueFill`: Demonstrate
 *      C++17 `std::pmr` allocator-aware (`allocator_type`) propagation vs.
 *      the silent nested `std::pmr::string` heap-escape trap.
 *
 * 2. Custom Low-Latency Allocator Architectures:
 *    - `BumpArena`: Pre-faulted linear monotonic bump allocator (`2-4`
 *      instructions per allocation, `O(1)` bulk reset / "wink-out").
 *    - `FixedSlabPool<T>`: Zero-header intrusive free-list pool (`union Slot`
 *      repurposes free slot bytes for `next_free` pointers; zero per-object
 *      metadata overhead when allocated).
 *    - `ThreadMagazinePool<T, kBatchSize>`: Producer-consumer scalable pool
 *      using thread-private `kBatchSize`-slot magazines (`1` central batch
 *      handoff per `32` cross-thread allocations/frees).
 *    - `SpscRingQueue<T, Capacity>`: Lock-free bounded SPSC ring for isolating
 *      cross-thread allocator throughput in Battle 1C.
 *    - `CountingMemoryResource`: Diagnostic `std::pmr::memory_resource` wrapper
 *      auditing upstream allocations, deallocations, and total bytes.
 */

#ifndef HOTPATH_COMMON_MEMORY_ALLOCATORS_MEMORY_ALLOCATORS_H_
#define HOTPATH_COMMON_MEMORY_ALLOCATORS_MEMORY_ALLOCATORS_H_

#include <sched.h>
#include <sys/mman.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hotpath::memory_allocators {

/** @brief Hardware L1/L2 cache line size in bytes on x86-64. */
inline constexpr std::size_t kCacheLineBytes = 64;

/** @brief Standard Linux 4 KiB base virtual memory page size. */
inline constexpr std::size_t kPageBytes4KiB = 4096;

/** @brief Standard x86-64 2 MiB HugePage size (`2,097,152` bytes). */
inline constexpr std::size_t kHugePageBytes = 2ULL << 20;

/** @brief Default order count (`65,536` x `32B` = exact `2 MiB` working set).
 */
inline constexpr std::size_t kDefaultOrderCount = 65536;

/** @brief Default `glibc` `ptmalloc2` per-bin `tcache` slot limit (`7`). */
inline constexpr std::size_t kGlibcTcacheFillCount = 7;

/**
 * @brief Prevents the compiler from optimizing away a computed value.
 * @tparam T Value type.
 * @param value Value to keep live across compiler optimization passes.
 */
template <typename T>
inline void DoNotOptimize(const T& value) noexcept {
  asm volatile("" : : "r,m"(value) : "memory");
}

/**
 * @brief Forces the compiler to treat memory as potentially modified.
 */
inline void ClobberMemory() noexcept { asm volatile("" : : : "memory"); }

/**
 * @brief Pins the calling thread to a specific logical CPU core to eliminate
 * cross-CCX/CCD migration noise during latency measurements.
 * @param core_id Zero-based logical CPU core index.
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  ::sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

/**
 * @brief Rounds `value` up to the nearest power-of-two `alignment` multiple.
 * @param value Byte offset or size to align.
 * @param alignment Power-of-two alignment in bytes.
 * @return Aligned byte value.
 */
[[nodiscard]] constexpr std::size_t AlignUp(std::size_t value,
                                            std::size_t alignment) noexcept {
  return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief 32-byte intrusive limit-order node used across all three battles.
 *
 * Two `OrderNode` instances pack back-to-back inside one 64-byte cache line
 * when allocated from `BumpArena` or `FixedSlabPool`. Under `glibc`
 * `ptmalloc2`, allocating `32` bytes adds an `8`-byte chunk header rounded up
 * to `16`-byte alignment (`48`-byte chunk stride), causing every node pair to
 * straddle cache lines.
 */
struct alignas(8) OrderNode {
  std::uint64_t order_id = 0;
  std::int64_t price_ticks = 0;
  std::uint32_t quantity = 0;
  std::uint32_t symbol_id = 0;
  OrderNode* next = nullptr;
};

static_assert(
    sizeof(OrderNode) == 32,
    "OrderNode must remain exactly 32 bytes for 2-per-cacheline pack");

/**
 * @brief Walks an intrusive linked list of `OrderNode` records, accumulating a
 * checksum to force serial pointer-dependent memory loads.
 * @param head Head node of the chain.
 * @return Accumulated checksum across all visited nodes.
 */
[[nodiscard]] inline std::uint64_t ScanOrderChain(
    const OrderNode* head) noexcept {
  std::uint64_t checksum = 0;
  const OrderNode* cur = head;
  while (cur != nullptr) {
    checksum += static_cast<std::uint64_t>(cur->price_ticks) * cur->quantity +
                cur->symbol_id;
    cur = cur->next;
  }
  return checksum;
}

/**
 * @brief Baseline heap-allocating venue fill record using `std::string`.
 */
struct StdVenueFill {
  std::uint64_t order_id = 0;
  std::int64_t price_ticks = 0;
  std::uint32_t quantity = 0;
  std::uint32_t venue_flags = 0;
  std::string venue_exec_id;
};

/**
 * @brief Pitfall struct: holds `std::pmr::string` WITHOUT declaring
 * `allocator_type`, so `std::uses_allocator_v<BrokenPmrVenueFill,
 * std::pmr::polymorphic_allocator<>>` is `false`. Inserting into
 * `std::pmr::vector<BrokenPmrVenueFill>` constructs `venue_exec_id` with the
 * global default resource (`new`/`delete`) whenever length exceeds `15` bytes!
 */
struct BrokenPmrVenueFill {
  std::uint64_t order_id = 0;
  std::int64_t price_ticks = 0;
  std::uint32_t quantity = 0;
  std::uint32_t venue_flags = 0;
  std::pmr::string venue_exec_id;

  /**
   * @brief Constructs a fill whose inner `pmr::string` defaults to global heap.
   * @param id Unique order identifier.
   * @param px Fill price in integer ticks.
   * @param qty Executed share quantity.
   * @param flags Venue status bitflags.
   * @param exec_id Execution identifier string view.
   */
  BrokenPmrVenueFill(std::uint64_t id, std::int64_t px, std::uint32_t qty,
                     std::uint32_t flags, std::string_view exec_id)
      : order_id(id),
        price_ticks(px),
        quantity(qty),
        venue_flags(flags),
        venue_exec_id(exec_id) {}
};

/**
 * @brief Properly allocator-aware (`AA`) fill struct propagating `std::pmr`
 * resources from `std::pmr::vector<PmrVenueFill>` into `venue_exec_id`.
 */
struct PmrVenueFill {
  using allocator_type = std::pmr::polymorphic_allocator<>;

  std::uint64_t order_id = 0;
  std::int64_t price_ticks = 0;
  std::uint32_t quantity = 0;
  std::uint32_t venue_flags = 0;
  std::pmr::string venue_exec_id;

  /**
   * @brief Default allocator-extended constructor.
   * @param alloc Polymorphic allocator propagated to `venue_exec_id`.
   */
  explicit PmrVenueFill(allocator_type alloc = {}) noexcept
      : venue_exec_id(alloc) {}

  /**
   * @brief Value constructor propagating the enclosing container's resource.
   * @param id Unique order identifier.
   * @param px Fill price in integer ticks.
   * @param qty Executed share quantity.
   * @param flags Venue status bitflags.
   * @param exec_id Execution identifier string view.
   * @param alloc Polymorphic allocator propagated to `venue_exec_id`.
   */
  PmrVenueFill(std::uint64_t id, std::int64_t px, std::uint32_t qty,
               std::uint32_t flags, std::string_view exec_id,
               allocator_type alloc = {})
      : order_id(id),
        price_ticks(px),
        quantity(qty),
        venue_flags(flags),
        venue_exec_id(exec_id, alloc) {}

  /**
   * @brief Copy constructor with explicit allocator propagation.
   * @param other Source fill instance to copy.
   * @param alloc Polymorphic allocator propagated to `venue_exec_id`.
   */
  PmrVenueFill(const PmrVenueFill& other, allocator_type alloc)
      : order_id(other.order_id),
        price_ticks(other.price_ticks),
        quantity(other.quantity),
        venue_flags(other.venue_flags),
        venue_exec_id(other.venue_exec_id, alloc) {}

  /**
   * @brief Move constructor with explicit allocator propagation.
   * @param other Source fill instance to move from.
   * @param alloc Polymorphic allocator propagated to `venue_exec_id`.
   */
  PmrVenueFill(PmrVenueFill&& other, allocator_type alloc)
      : order_id(other.order_id),
        price_ticks(other.price_ticks),
        quantity(other.quantity),
        venue_flags(other.venue_flags),
        venue_exec_id(std::move(other.venue_exec_id), alloc) {}
};

/**
 * @brief Allocates a `64B`-aligned virtual region via anonymous `mmap`,
 * optionally advising `2 MiB` Transparent HugePages and pre-faulting every page
 * so zero `#PF` traps occur during hot-path execution.
 * @param bytes Total requested capacity in bytes.
 * @param prefault If `true`, touches one byte per `4 KiB` page upfront.
 * @return Aligned base pointer (must be released with `ReleaseMappedRegion`).
 */
[[nodiscard]] inline std::byte* AllocateMappedRegion(
    std::size_t bytes, bool prefault = true) noexcept {
  const std::size_t rounded =
      AlignUp(std::max(bytes, kPageBytes4KiB), kPageBytes4KiB);
  void* ptr = ::mmap(nullptr, rounded, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return nullptr;
  }
  if (rounded >= kHugePageBytes) {
    ::madvise(ptr, rounded, MADV_HUGEPAGE);
  }
  auto* byte_ptr = static_cast<std::byte*>(ptr);
  if (prefault) {
    for (std::size_t offset = 0; offset < rounded; offset += kPageBytes4KiB) {
      byte_ptr[offset] = std::byte{0};
    }
  }
  return byte_ptr;
}

/**
 * @brief Releases a region allocated by `AllocateMappedRegion`.
 * @param ptr Base address returned by `AllocateMappedRegion`.
 * @param bytes Original byte size passed to `AllocateMappedRegion`.
 */
inline void ReleaseMappedRegion(std::byte* ptr, std::size_t bytes) noexcept {
  if (ptr != nullptr) {
    const std::size_t rounded =
        AlignUp(std::max(bytes, kPageBytes4KiB), kPageBytes4KiB);
    ::munmap(ptr, rounded);
  }
}

/**
 * @brief Pre-faulted monotonic bump allocator for phase-scoped / per-request
 * workloads. Compiles fast-path aligned allocation down to a pointer add and
 * bounds check, and reclaims all allocated memory in `O(1)` via `Reset()`.
 */
class BumpArena {
 public:
  /**
   * @brief Constructs a `BumpArena` backed by a pre-faulted `mmap` slab.
   * @param capacity_bytes Total arena capacity in bytes.
   * @param prefault Pre-touches all `4 KiB` pages at construction time.
   */
  explicit BumpArena(std::size_t capacity_bytes, bool prefault = true) noexcept
      : capacity_(
            AlignUp(std::max(capacity_bytes, kPageBytes4KiB), kPageBytes4KiB)),
        base_(AllocateMappedRegion(capacity_, prefault)),
        cur_(base_),
        end_(base_ != nullptr ? base_ + capacity_ : nullptr) {}

  /** @brief Destroys the arena and unmaps its backing slab. */
  ~BumpArena() { ReleaseMappedRegion(base_, capacity_); }

  BumpArena(const BumpArena&) = delete;
  BumpArena& operator=(const BumpArena&) = delete;

  /**
   * @brief Move constructor transferring arena ownership.
   * @param other Source BumpArena whose mapped slab is transferred.
   */
  BumpArena(BumpArena&& other) noexcept
      : capacity_(other.capacity_),
        base_(other.base_),
        cur_(other.cur_),
        end_(other.end_) {
    other.capacity_ = 0;
    other.base_ = nullptr;
    other.cur_ = nullptr;
    other.end_ = nullptr;
  }

  /**
   * @brief Move assignment operator transferring arena ownership.
   * @param other Source BumpArena whose mapped slab is transferred.
   * @return Reference to this BumpArena instance.
   */
  BumpArena& operator=(BumpArena&& other) noexcept {
    if (this != &other) {
      ReleaseMappedRegion(base_, capacity_);
      capacity_ = other.capacity_;
      base_ = other.base_;
      cur_ = other.cur_;
      end_ = other.end_;
      other.capacity_ = 0;
      other.base_ = nullptr;
      other.cur_ = nullptr;
      other.end_ = nullptr;
    }
    return *this;
  }

  /**
   * @brief Allocates `size` bytes aligned to `alignment` via pointer bump.
   * @param size Requested allocation size in bytes.
   * @param alignment Power-of-two alignment requirement.
   * @return Aligned pointer into the arena, or `nullptr` if exhausted.
   */
  [[nodiscard]] void* AllocateBytes(
      std::size_t size,
      std::size_t alignment = alignof(std::max_align_t)) noexcept {
    const std::uintptr_t current = reinterpret_cast<std::uintptr_t>(cur_);
    const std::uintptr_t mask = alignment - 1;
    const std::uintptr_t aligned = (current + mask) & ~mask;
    const std::uintptr_t next = aligned + size;
    if (next > reinterpret_cast<std::uintptr_t>(end_)) [[unlikely]] {
      return nullptr;
    }
    cur_ = reinterpret_cast<std::byte*>(next);
    return reinterpret_cast<void*>(aligned);
  }

  /**
   * @brief Allocates and constructs an instance of `T` in-place inside the
   * arena.
   * @tparam T Object type to construct.
   * @tparam Args Constructor argument types.
   * @param args Constructor arguments forwarded to `T`.
   * @return Pointer to constructed `T`, or `nullptr` if exhausted.
   */
  template <typename T, typename... Args>
  [[nodiscard]] T* Create(Args&&... args) {
    void* mem = AllocateBytes(sizeof(T), alignof(T));
    if (mem == nullptr) [[unlikely]] {
      return nullptr;
    }
    return ::new (mem) T(std::forward<Args>(args)...);
  }

  /**
   * @brief Reclaims all allocations in `O(1)` by rewinding the bump cursor.
   * Callers are responsible for ensuring trivially destructible or already
   * destructed types ("wink-out" pattern).
   */
  void Reset() noexcept { cur_ = base_; }

  /**
   * @brief Returns number of bytes currently consumed in the arena.
   * @return Consumed byte offset from arena base.
   */
  [[nodiscard]] std::size_t BytesUsed() const noexcept {
    return static_cast<std::size_t>(cur_ - base_);
  }

  /**
   * @brief Returns total mapped capacity in bytes.
   * @return Total byte capacity of the backing slab.
   */
  [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }

 private:
  std::size_t capacity_ = 0;
  std::byte* base_ = nullptr;
  std::byte* cur_ = nullptr;
  std::byte* end_ = nullptr;
};

/**
 * @brief Zero-header intrusive free-list slab pool for fixed-size objects `T`.
 *
 * When a slot is free, its bytes store `Slot* next_free`; when allocated, 100%
 * of the slot bytes hold `T`, eliminating `glibc`'s 16-byte chunk metadata tax
 * and packing `2` 32-byte `OrderNode`s per 64-byte hardware cache line.
 *
 * @tparam T Object type stored in each slab slot (`sizeof(T) >=
 * sizeof(void*)`).
 */
template <typename T>
class FixedSlabPool {
 public:
  /** @brief Intrusive storage union sharing free-list link and `T` storage. */
  union Slot {
    Slot* next_free;
    alignas(T) std::byte storage[sizeof(T)];
  };

  static_assert(sizeof(T) >= sizeof(Slot*),
                "FixedSlabPool requires sizeof(T) >= sizeof(void*)");

  /**
   * @brief Allocates a contiguous pre-faulted slab for `capacity` objects.
   * @param capacity Maximum simultaneous live objects in the pool.
   */
  explicit FixedSlabPool(std::size_t capacity) noexcept
      : capacity_(capacity),
        available_(capacity),
        backing_bytes_(std::max(capacity * sizeof(Slot), kPageBytes4KiB)),
        raw_mem_(AllocateMappedRegion(backing_bytes_, true)),
        slots_(reinterpret_cast<Slot*>(raw_mem_)) {
    Reset();
  }

  /** @brief Unmaps the slab region. */
  ~FixedSlabPool() { ReleaseMappedRegion(raw_mem_, backing_bytes_); }

  FixedSlabPool(const FixedSlabPool&) = delete;
  FixedSlabPool& operator=(const FixedSlabPool&) = delete;

  /**
   * @brief Pops one free slot in `O(1)` and constructs `T` in-place.
   * @tparam Args Constructor argument types.
   * @param args Constructor arguments forwarded to `T`.
   * @return Pointer to live `T`, or `nullptr` if pool is full.
   */
  template <typename... Args>
  [[nodiscard]] T* Allocate(Args&&... args) {
    Slot* slot = free_head_;
    if (slot == nullptr) [[unlikely]] {
      return nullptr;
    }
    free_head_ = slot->next_free;
    --available_;
    return ::new (static_cast<void*>(slot->storage))
        T(std::forward<Args>(args)...);
  }

  /**
   * @brief Destroys `T` and pushes its slot back onto the LIFO free list in
   * `O(1)`.
   * @param ptr Pointer previously returned by `Allocate()`, or `nullptr`.
   */
  void Deallocate(T* ptr) noexcept {
    if (ptr == nullptr) [[unlikely]] {
      return;
    }
    ptr->~T();
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    slot->next_free = free_head_;
    free_head_ = slot;
    ++available_;
  }

  /**
   * @brief Rebuilds the intrusive free list across all `capacity_` slots in
   * contiguous virtual memory order (`O(N)` startup / reset helper).
   */
  void Reset() noexcept {
    if (slots_ == nullptr || capacity_ == 0) {
      free_head_ = nullptr;
      available_ = 0;
      return;
    }
    for (std::size_t i = 0; i + 1 < capacity_; ++i) {
      slots_[i].next_free = &slots_[i + 1];
    }
    slots_[capacity_ - 1].next_free = nullptr;
    free_head_ = &slots_[0];
    available_ = capacity_;
  }

  /**
   * @brief Returns number of free slots currently available.
   * @return Count of unallocated slab slots.
   */
  [[nodiscard]] std::size_t Available() const noexcept { return available_; }

  /**
   * @brief Returns total slot capacity of the slab.
   * @return Maximum number of objects the slab can hold.
   */
  [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }

 private:
  std::size_t capacity_ = 0;
  std::size_t available_ = 0;
  std::size_t backing_bytes_ = 0;
  std::byte* raw_mem_ = nullptr;
  Slot* slots_ = nullptr;
  Slot* free_head_ = nullptr;
};

/**
 * @brief Lock-free bounded Single-Producer Single-Consumer (SPSC) ring queue
 * with cache-line-isolated producer/consumer indices (`alignas(64)`).
 * @tparam T Trivially copyable element type (e.g. `OrderNode*`).
 * @tparam Capacity Power-of-two ring capacity.
 */
template <typename T, std::size_t Capacity>
class SpscRingQueue {
  static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                "SpscRingQueue Capacity must be a power of two >= 2");

 public:
  /**
   * @brief Pushes one item if space is available (producer thread only).
   * @param value Item to enqueue.
   * @return True if enqueued, false if the ring is full.
   */
  [[nodiscard]] bool TryPush(T value) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1) & kMask;
    if (next == cached_tail_) {
      cached_tail_ = tail_.load(std::memory_order_acquire);
      if (next == cached_tail_) {
        return false;
      }
    }
    buffer_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  /**
   * @brief Pops one item if available (consumer thread only).
   * @param out Reference receiving the dequeued item on success.
   * @return True if an item was dequeued, false if the ring is empty.
   */
  [[nodiscard]] bool TryPop(T& out) noexcept {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == cached_head_) {
      cached_head_ = head_.load(std::memory_order_acquire);
      if (tail == cached_head_) {
        return false;
      }
    }
    out = buffer_[tail];
    tail_.store((tail + 1) & kMask, std::memory_order_release);
    return true;
  }

 private:
  static constexpr std::size_t kMask = Capacity - 1;

  alignas(kCacheLineBytes) std::atomic<std::size_t> head_{0};
  std::size_t cached_tail_ = 0;
  alignas(kCacheLineBytes) std::atomic<std::size_t> tail_{0};
  std::size_t cached_head_ = 0;
  alignas(kCacheLineBytes) std::array<T, Capacity> buffer_{};
};

/**
 * @brief Thread-scalable multi-producer / multi-consumer slab pool using
 * per-thread `kBatchSize`-slot magazines (`1` central lock per `kBatchSize`
 * remote allocations or frees instead of per-object contention).
 *
 * Designed for asymmetric producer-consumer pipelines where Thread A allocates
 * objects and Thread B frees them across SPSC rings.
 *
 * @tparam T Object type (`sizeof(T) >= sizeof(void*)`).
 * @tparam kBatchSize Slots transferred per central-depot batch refill/drain.
 */
template <typename T, std::size_t kBatchSize = 32>
class ThreadMagazinePool {
 public:
  /** @brief Intrusive slot storage union. */
  union Slot {
    Slot* next_free;
    alignas(T) std::byte storage[sizeof(T)];
  };

  static_assert(sizeof(T) >= sizeof(Slot*),
                "ThreadMagazinePool requires sizeof(T) >= sizeof(void*)");
  static_assert(kBatchSize >= 2, "kBatchSize must be at least 2");

  /** @brief Batch container exchanging `kBatchSize` slots with central depot.
   */
  struct alignas(kCacheLineBytes) Magazine {
    std::array<Slot*, kBatchSize> slots{};
    std::size_t count = 0;
    Magazine* next = nullptr;
  };

  /**
   * @brief Per-worker-thread private allocation/free cache holding up to two
   * `kBatchSize` magazines (zero locks on fast path).
   */
  class LocalCache {
   public:
    /**
     * @brief Binds a thread-local cache to its backing `ThreadMagazinePool`.
     * @param pool Central magazine pool supplying refill/drain batches.
     */
    explicit LocalCache(ThreadMagazinePool& pool) noexcept : pool_(pool) {}

    /** @brief Flushes any remaining cached slots back to `pool` on teardown. */
    ~LocalCache() { Flush(); }

    LocalCache(const LocalCache&) = delete;
    LocalCache& operator=(const LocalCache&) = delete;

    /** @brief Flushes locally cached magazines back to the central depot. */
    void Flush() noexcept {
      if (alloc_mag_ != nullptr) {
        pool_.ReturnMagazine(alloc_mag_);
        alloc_mag_ = nullptr;
      }
      if (free_mag_ != nullptr) {
        pool_.ReturnMagazine(free_mag_);
        free_mag_ = nullptr;
      }
    }

   private:
    friend class ThreadMagazinePool;

    ThreadMagazinePool& pool_;
    Magazine* alloc_mag_ = nullptr;
    Magazine* free_mag_ = nullptr;
  };

  /** @brief Reserve empty magazine headers kept in the central depot. */
  static constexpr std::size_t kExtraEmptyMagazines = 128;

  /**
   * @brief Constructs a `ThreadMagazinePool` rounded up to whole `kBatchSize`
   * magazines.
   * @param min_capacity Minimum number of object slots to pre-allocate.
   */
  explicit ThreadMagazinePool(std::size_t min_capacity) noexcept
      : magazine_count_((min_capacity + kBatchSize - 1) / kBatchSize + 16),
        total_mag_headers_(magazine_count_ + kExtraEmptyMagazines),
        capacity_(magazine_count_ * kBatchSize),
        slot_bytes_(AlignUp(capacity_ * sizeof(Slot), kPageBytes4KiB)),
        mag_bytes_(
            AlignUp(total_mag_headers_ * sizeof(Magazine), kPageBytes4KiB)),
        raw_slots_(AllocateMappedRegion(slot_bytes_, true)),
        raw_mags_(AllocateMappedRegion(mag_bytes_, true)),
        slots_(reinterpret_cast<Slot*>(raw_slots_)),
        magazines_(reinterpret_cast<Magazine*>(raw_mags_)) {
    for (std::size_t m = 0; m < magazine_count_; ++m) {
      Magazine* mag = ::new (&magazines_[m]) Magazine{};
      for (std::size_t i = 0; i < kBatchSize; ++i) {
        mag->slots[i] = &slots_[m * kBatchSize + i];
      }
      mag->count = kBatchSize;
      mag->next = full_head_;
      full_head_ = mag;
    }
    for (std::size_t m = magazine_count_; m < total_mag_headers_; ++m) {
      Magazine* empty_mag = ::new (&magazines_[m]) Magazine{};
      empty_mag->count = 0;
      empty_mag->next = empty_head_;
      empty_head_ = empty_mag;
    }
    available_slots_.store(capacity_, std::memory_order_relaxed);
  }

  /** @brief Unmaps slot and magazine slabs. */
  ~ThreadMagazinePool() {
    ReleaseMappedRegion(raw_mags_, mag_bytes_);
    ReleaseMappedRegion(raw_slots_, slot_bytes_);
  }

  ThreadMagazinePool(const ThreadMagazinePool&) = delete;
  ThreadMagazinePool& operator=(const ThreadMagazinePool&) = delete;

  /**
   * @brief Allocates and constructs `T` using the caller's thread-private
   * `LocalCache` (zero locks until `kBatchSize` slots exhaust).
   * @tparam Args Constructor argument types.
   * @param cache Calling thread's `LocalCache`.
   * @param args Constructor arguments forwarded to `T`.
   * @return Pointer to constructed `T`, or `nullptr` if exhausted.
   */
  template <typename... Args>
  [[nodiscard]] T* Allocate(LocalCache& cache, Args&&... args) {
    if (cache.alloc_mag_ == nullptr || cache.alloc_mag_->count == 0)
        [[unlikely]] {
      if (!RefillAllocMagazine(cache)) [[unlikely]] {
        return nullptr;
      }
    }
    Slot* slot = cache.alloc_mag_->slots[--cache.alloc_mag_->count];
    return ::new (static_cast<void*>(slot->storage))
        T(std::forward<Args>(args)...);
  }

  /**
   * @brief Destroys `T` and returns its slot into the caller's `LocalCache`
   * (zero locks until `kBatchSize` slots accumulate).
   * @param cache Calling thread's `LocalCache`.
   * @param ptr Object pointer previously allocated from this pool.
   */
  void Deallocate(LocalCache& cache, T* ptr) noexcept {
    if (ptr == nullptr) [[unlikely]] {
      return;
    }
    ptr->~T();
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    if (cache.free_mag_ == nullptr || cache.free_mag_->count == kBatchSize)
        [[unlikely]] {
      FlushFreeMagazine(cache);
    }
    cache.free_mag_->slots[cache.free_mag_->count++] = slot;
  }

  /**
   * @brief Returns total slot capacity across all pre-allocated magazines.
   * @return Total number of pre-allocated object slots.
   */
  [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }

  /**
   * @brief Returns number of slots currently sitting in the central depot
   * (note: slots held inside active `LocalCache`s count once flushed).
   * @return Currently available slot count in the central depot.
   */
  [[nodiscard]] std::size_t DepotAvailableSlots() const noexcept {
    return available_slots_.load(std::memory_order_acquire);
  }

 private:
  [[nodiscard]] bool RefillAllocMagazine(LocalCache& cache) noexcept {
    // Fast thread-local shortcut: if free_mag_ has accumulated slots, swap.
    if (cache.free_mag_ != nullptr && cache.free_mag_->count > 0) {
      std::swap(cache.alloc_mag_, cache.free_mag_);
      return true;
    }
    std::lock_guard<std::mutex> lock(depot_mutex_);
    if (full_head_ == nullptr) {
      return false;
    }
    Magazine* empty = cache.alloc_mag_;
    cache.alloc_mag_ = full_head_;
    full_head_ = full_head_->next;
    cache.alloc_mag_->next = nullptr;
    available_slots_.fetch_sub(cache.alloc_mag_->count,
                               std::memory_order_relaxed);
    if (empty != nullptr) {
      empty->next = empty_head_;
      empty_head_ = empty;
    }
    return true;
  }

  void FlushFreeMagazine(LocalCache& cache) noexcept {
    std::lock_guard<std::mutex> lock(depot_mutex_);
    if (cache.free_mag_ != nullptr) {
      available_slots_.fetch_add(cache.free_mag_->count,
                                 std::memory_order_relaxed);
      cache.free_mag_->next = full_head_;
      full_head_ = cache.free_mag_;
      cache.free_mag_ = nullptr;
    }
    if (empty_head_ != nullptr) {
      cache.free_mag_ = empty_head_;
      empty_head_ = empty_head_->next;
      cache.free_mag_->next = nullptr;
      cache.free_mag_->count = 0;
    }
  }

  void ReturnMagazine(Magazine* mag) noexcept {
    std::lock_guard<std::mutex> lock(depot_mutex_);
    if (mag->count == 0) {
      mag->next = empty_head_;
      empty_head_ = mag;
    } else {
      available_slots_.fetch_add(mag->count, std::memory_order_relaxed);
      mag->next = full_head_;
      full_head_ = mag;
    }
  }

  std::size_t magazine_count_ = 0;
  std::size_t total_mag_headers_ = 0;
  std::size_t capacity_ = 0;
  std::size_t slot_bytes_ = 0;
  std::size_t mag_bytes_ = 0;
  std::byte* raw_slots_ = nullptr;
  std::byte* raw_mags_ = nullptr;
  Slot* slots_ = nullptr;
  Magazine* magazines_ = nullptr;

  alignas(kCacheLineBytes) std::mutex depot_mutex_;
  Magazine* full_head_ = nullptr;
  Magazine* empty_head_ = nullptr;
  alignas(kCacheLineBytes) std::atomic<std::size_t> available_slots_{0};
};

/**
 * @brief Diagnostic `std::pmr::memory_resource` decorator that counts upstream
 * allocations, deallocations, and allocated bytes without changing semantics.
 */
class CountingMemoryResource final : public std::pmr::memory_resource {
 public:
  /**
   * @brief Wraps an `upstream` resource (defaults to
   * `std::pmr::new_delete_resource()`).
   * @param upstream Backing memory resource receiving forwarded calls.
   */
  explicit CountingMemoryResource(std::pmr::memory_resource* upstream =
                                      std::pmr::new_delete_resource()) noexcept
      : upstream_(upstream) {}

  /** @brief Resets all counters to zero. */
  void ResetCounters() noexcept {
    alloc_count_.store(0, std::memory_order_relaxed);
    dealloc_count_.store(0, std::memory_order_relaxed);
    bytes_allocated_.store(0, std::memory_order_relaxed);
  }

  /**
   * @brief Returns total upstream `allocate()` calls observed.
   * @return Cumulative allocation count.
   */
  [[nodiscard]] std::size_t alloc_count() const noexcept {
    return alloc_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns total upstream `deallocate()` calls observed.
   * @return Cumulative deallocation count.
   */
  [[nodiscard]] std::size_t dealloc_count() const noexcept {
    return dealloc_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Returns total upstream bytes allocated.
   * @return Cumulative allocated byte count.
   */
  [[nodiscard]] std::size_t bytes_allocated() const noexcept {
    return bytes_allocated_.load(std::memory_order_relaxed);
  }

 protected:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    alloc_count_.fetch_add(1, std::memory_order_relaxed);
    bytes_allocated_.fetch_add(bytes, std::memory_order_relaxed);
    return upstream_->allocate(bytes, alignment);
  }

  void do_deallocate(void* p, std::size_t bytes,
                     std::size_t alignment) override {
    dealloc_count_.fetch_add(1, std::memory_order_relaxed);
    upstream_->deallocate(p, bytes, alignment);
  }

  bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }

 private:
  std::pmr::memory_resource* upstream_;
  std::atomic<std::size_t> alloc_count_{0};
  std::atomic<std::size_t> dealloc_count_{0};
  std::atomic<std::size_t> bytes_allocated_{0};
};

/**
 * @brief Pointer-stride spatial locality summary across a linked `OrderNode`
 * chain.
 */
struct StrideStats {
  std::size_t node_count = 0;
  double pct_same_cacheline_stride = 0.0;  ///< `|next - cur| <= 64` bytes.
  double pct_same_4k_page = 0.0;           ///< `(next >> 12) == (cur >> 12)`.
  double avg_abs_jump_bytes = 0.0;         ///< Mean `|next - cur|` in bytes.
};

/**
 * @brief Measures spatial pointer stride locality along a linked `OrderNode`
 * list without modifying any nodes.
 * @param head Head of the linked list.
 * @return Populated `StrideStats` summary.
 */
[[nodiscard]] inline StrideStats MeasureChainStride(
    const OrderNode* head) noexcept {
  StrideStats stats;
  if (head == nullptr) {
    return stats;
  }
  std::size_t hops = 0;
  std::size_t cacheline_hits = 0;
  std::size_t page_hits = 0;
  long double total_jump = 0.0L;
  const OrderNode* cur = head;
  while (cur->next != nullptr) {
    const auto a = reinterpret_cast<std::uintptr_t>(cur);
    const auto b = reinterpret_cast<std::uintptr_t>(cur->next);
    const std::uintptr_t diff = (b >= a) ? (b - a) : (a - b);
    if (diff <= kCacheLineBytes) {
      ++cacheline_hits;
    }
    if ((a >> 12) == (b >> 12)) {
      ++page_hits;
    }
    total_jump += static_cast<long double>(diff);
    ++hops;
    cur = cur->next;
  }
  stats.node_count = hops + 1;
  if (hops > 0) {
    stats.pct_same_cacheline_stride =
        100.0 * static_cast<double>(cacheline_hits) / static_cast<double>(hops);
    stats.pct_same_4k_page =
        100.0 * static_cast<double>(page_hits) / static_cast<double>(hops);
    stats.avg_abs_jump_bytes =
        static_cast<double>(total_jump / static_cast<long double>(hops));
  }
  return stats;
}

/**
 * @brief Deterministic Battle 2 fixture constructing four memory layouts of
 * identical `OrderNode` chains (`order_count` nodes, default `65,536` = `2 MiB`
 * logical payload):
 * 1. Fresh Startup Heap (`new OrderNode` sequentially before churn),
 * 2. Aged Fragmented Heap (`new OrderNode` after mixed-size churn),
 * 3. Isolated `FixedSlabPool<OrderNode>` (immune to variable-size heap churn),
 * 4. Compacted `BumpArena` / Chained `std::pmr` (`32B` packed stride).
 */
class HeapAgingScenario {
 public:
  /**
   * @brief Builds all four `OrderNode` chains using deterministic LCG churn.
   * @param order_count Number of live orders in each chain.
   * @param churn_cycles Number of mixed-size allocations creating
   * fragmentation.
   */
  explicit HeapAgingScenario(std::size_t order_count = kDefaultOrderCount,
                             std::size_t churn_cycles = 262144)
      : order_count_(order_count),
        slab_pool_(order_count * 2),
        arena_(AlignUp(order_count * sizeof(OrderNode) + kPageBytes4KiB,
                       kPageBytes4KiB),
               true),
        pmr_backing_(AlignUp(order_count * sizeof(OrderNode) * 3 + 262144,
                             kPageBytes4KiB),
                     true),
        pmr_monotonic_(pmr_backing_.AllocateBytes(pmr_backing_.Capacity(),
                                                  kCacheLineBytes),
                       pmr_backing_.Capacity(),
                       std::pmr::null_memory_resource()),
        pmr_pool_(std::pmr::pool_options{.max_blocks_per_chunk = 4096,
                                         .largest_required_pool_block = 64},
                  &pmr_monotonic_) {
    BuildFreshHeapChain();
    SimulateProductionHeapAging(churn_cycles);
    BuildAgedHeapChain();
    BuildIsolatedSlabAndArenaChains();
  }

  /** @brief Releases all heap-allocated `OrderNode`s cleanly. */
  ~HeapAgingScenario() {
    for (OrderNode* node : fresh_nodes_) {
      delete node;
    }
    for (OrderNode* node : aged_nodes_) {
      delete node;
    }
    for (OrderNode* node : transient_survivors_) {
      delete node;
    }
  }

  HeapAgingScenario(const HeapAgingScenario&) = delete;
  HeapAgingScenario& operator=(const HeapAgingScenario&) = delete;

  /**
   * @brief Returns head of the fresh startup heap order chain.
   * @return Const pointer to first `OrderNode` in the fresh heap chain.
   */
  [[nodiscard]] const OrderNode* fresh_head() const noexcept {
    return fresh_head_;
  }

  /**
   * @brief Returns head of the aged fragmented heap order chain.
   * @return Const pointer to first `OrderNode` in the aged heap chain.
   */
  [[nodiscard]] const OrderNode* aged_head() const noexcept {
    return aged_head_;
  }

  /**
   * @brief Returns head of the churned `FixedSlabPool<OrderNode>` chain.
   * @return Const pointer to first `OrderNode` in the slab chain.
   */
  [[nodiscard]] const OrderNode* slab_head() const noexcept {
    return slab_head_;
  }

  /**
   * @brief Returns head of the compacted `BumpArena` chain.
   * @return Const pointer to first `OrderNode` in the bump-arena chain.
   */
  [[nodiscard]] const OrderNode* arena_head() const noexcept {
    return arena_head_;
  }

  /**
   * @brief Returns head of the chained `std::pmr::unsynchronized_pool_resource`
   *        order chain.
   * @return Const pointer to first `OrderNode` in the PMR pool chain.
   */
  [[nodiscard]] const OrderNode* pmr_head() const noexcept { return pmr_head_; }

  /**
   * @brief Returns number of live orders in each benchmark chain.
   * @return Order count per chain.
   */
  [[nodiscard]] std::size_t order_count() const noexcept {
    return order_count_;
  }

 private:
  static std::uint64_t NextRng(std::uint64_t& state) noexcept {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return state;
  }

  void BuildFreshHeapChain() {
    fresh_nodes_.reserve(order_count_);
    OrderNode* prev = nullptr;
    for (std::size_t i = 0; i < order_count_; ++i) {
      auto* node =
          new OrderNode{i + 1, 18500 + static_cast<std::int64_t>(i & 255U),
                        100U + static_cast<std::uint32_t>(i & 63U),
                        static_cast<std::uint32_t>(i & 31U), nullptr};
      if (prev != nullptr) {
        prev->next = node;
      } else {
        fresh_head_ = node;
      }
      prev = node;
      fresh_nodes_.push_back(node);
    }
  }

  void SimulateProductionHeapAging(std::size_t churn_cycles) {
    // Interleave multi-size session/audit buffers and transient OrderNodes
    // across tens of megabytes so glibc/jemalloc bins age organically while
    // long-lived records pin pages in place ("Swiss-cheese" RSS).
    std::uint64_t rng = 0xCAFEBABE12345678ULL;
    constexpr std::size_t kPinSlots = 16384;
    pinned_payloads_.resize(kPinSlots);
    std::vector<OrderNode*> scratch_orders(kPinSlots, nullptr);

    for (std::size_t step = 0; step < churn_cycles; ++step) {
      const std::size_t idx =
          static_cast<std::size_t>(NextRng(rng) >> 32) % kPinSlots;
      const std::size_t size_bucket =
          64U + (static_cast<std::size_t>(NextRng(rng) >> 40) & 7U) * 192U;
      auto buf = std::make_unique<std::byte[]>(size_bucket);
      buf[0] = static_cast<std::byte>(step & 0xFFU);
      pinned_payloads_[idx] = std::move(buf);

      if ((step & 3U) == 0U) {
        delete scratch_orders[idx];
        scratch_orders[idx] = new OrderNode{
            step, 18525, 100U, static_cast<std::uint32_t>(idx), nullptr};
      }
    }
    // Retain half the transient scratch nodes as background noise and free
    // the rest, leaving scattered 48-byte holes across tens of megabytes.
    for (std::size_t i = 0; i < kPinSlots; ++i) {
      if (scratch_orders[i] != nullptr) {
        if ((i & 1U) == 0U) {
          transient_survivors_.push_back(scratch_orders[i]);
        } else {
          delete scratch_orders[i];
        }
      }
    }
  }

  void BuildAgedHeapChain() {
    aged_nodes_.reserve(order_count_);
    std::uint64_t rng = 0x9E3779B97F4A7C15ULL;
    std::vector<OrderNode*> temp_allocs;
    temp_allocs.reserve(order_count_ * 2);

    // Interleave allocations and cancellations across the aged heap just like
    // live market order-book arrivals.
    OrderNode* prev = nullptr;
    for (std::size_t i = 0; i < order_count_; ++i) {
      // Allocate burst of mixed-size records & transient nodes, retain 1 live
      // node, and drop transient peers to force non-linear virtual placement.
      auto* live =
          new OrderNode{i + 1, 18500 + static_cast<std::int64_t>(i & 255U),
                        100U + static_cast<std::uint32_t>(i & 63U),
                        static_cast<std::uint32_t>(i & 31U), nullptr};
      if (prev != nullptr) {
        prev->next = live;
      } else {
        aged_head_ = live;
      }
      prev = live;
      aged_nodes_.push_back(live);

      auto* decoy = new OrderNode{i, 1, 1, 1, nullptr};
      temp_allocs.push_back(decoy);
      if (temp_allocs.size() > 1024 && ((NextRng(rng) & 3ULL) != 0ULL)) {
        const std::size_t victim =
            static_cast<std::size_t>(NextRng(rng) >> 32) % temp_allocs.size();
        delete temp_allocs[victim];
        temp_allocs[victim] = temp_allocs.back();
        temp_allocs.pop_back();
      }
    }
    for (OrderNode* decoy : temp_allocs) {
      delete decoy;
    }
  }

  void BuildIsolatedSlabAndArenaChains() {
    // Same churn pattern executed inside FixedSlabPool (isolated 2 MiB slab).
    std::uint64_t rng = 0x9E3779B97F4A7C15ULL;
    std::vector<OrderNode*> slab_decoys;
    slab_decoys.reserve(1024);

    OrderNode* prev_slab = nullptr;
    OrderNode* prev_arena = nullptr;
    OrderNode* prev_pmr = nullptr;
    std::pmr::polymorphic_allocator<OrderNode> pmr_alloc(&pmr_pool_);

    for (std::size_t i = 0; i < order_count_; ++i) {
      OrderNode* s = slab_pool_.Allocate(
          i + 1, 18500 + static_cast<std::int64_t>(i & 255U),
          100U + static_cast<std::uint32_t>(i & 63U),
          static_cast<std::uint32_t>(i & 31U), nullptr);
      if (prev_slab != nullptr) {
        prev_slab->next = s;
      } else {
        slab_head_ = s;
      }
      prev_slab = s;

      OrderNode* d = slab_pool_.Allocate(i, 1LL, 1U, 1U, nullptr);
      slab_decoys.push_back(d);
      if (slab_decoys.size() > 512 && ((NextRng(rng) & 3ULL) != 0ULL)) {
        const std::size_t victim =
            static_cast<std::size_t>(NextRng(rng) >> 32) % slab_decoys.size();
        slab_pool_.Deallocate(slab_decoys[victim]);
        slab_decoys[victim] = slab_decoys.back();
        slab_decoys.pop_back();
      }

      OrderNode* a = arena_.Create<OrderNode>(
          i + 1, 18500 + static_cast<std::int64_t>(i & 255U),
          100U + static_cast<std::uint32_t>(i & 63U),
          static_cast<std::uint32_t>(i & 31U), nullptr);
      if (prev_arena != nullptr) {
        prev_arena->next = a;
      } else {
        arena_head_ = a;
      }
      prev_arena = a;

      OrderNode* p = pmr_alloc.allocate(1);
      pmr_alloc.construct(p, i + 1, 18500 + static_cast<std::int64_t>(i & 255U),
                          100U + static_cast<std::uint32_t>(i & 63U),
                          static_cast<std::uint32_t>(i & 31U), nullptr);
      if (prev_pmr != nullptr) {
        prev_pmr->next = p;
      } else {
        pmr_head_ = p;
      }
      prev_pmr = p;
    }
    for (OrderNode* d : slab_decoys) {
      slab_pool_.Deallocate(d);
    }
  }

  std::size_t order_count_;
  OrderNode* fresh_head_ = nullptr;
  OrderNode* aged_head_ = nullptr;
  OrderNode* slab_head_ = nullptr;
  OrderNode* arena_head_ = nullptr;
  OrderNode* pmr_head_ = nullptr;

  std::vector<OrderNode*> fresh_nodes_;
  std::vector<OrderNode*> aged_nodes_;
  std::vector<OrderNode*> transient_survivors_;
  std::vector<std::unique_ptr<std::byte[]>> pinned_payloads_;

  FixedSlabPool<OrderNode> slab_pool_;
  BumpArena arena_;
  BumpArena pmr_backing_;
  std::pmr::monotonic_buffer_resource pmr_monotonic_;
  std::pmr::unsynchronized_pool_resource pmr_pool_;
};

}  // namespace hotpath::memory_allocators

#endif  // HOTPATH_COMMON_MEMORY_ALLOCATORS_MEMORY_ALLOCATORS_H_
