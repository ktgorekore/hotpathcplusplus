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
 * @file syscalls_traps_and_mmu.h
 * @brief Episode 11 production OS boundary & hardware MMU primitives:
 * 1. Workload 1 (Ring 3 vDSO & TLS vs. Ring 0 System Calls):
 *    - `VdsoNowNs()` (`clock_gettime(CLOCK_MONOTONIC)` via Ring 3 `[vdso]` +
 *      `[vvar]` Seqlock + `rdtscp`, 0 syscalls)
 *    - `RawSyscallNowNs()` (`syscall(SYS_clock_gettime)` Ring 0 trap)
 *    - `RawSyscallGettid()` (`syscall(SYS_gettid)` Ring 0 trap)
 *    - `CachedThreadId()` (`thread_local` Ring 3 `%fs` segment read)
 * 2. Workload 2 (Demand Paging, Interrupt Vector 14 `#PF` Traps & Pools):
 *    - `PrefaultedArenaPool` (`mmap` + `MAP_POPULATE` + `mlock` Ring 3 pool)
 *    - `TouchPages4KiB()` (Cache-set skewed 4 KiB stride page writer)
 *    - `ReadMinorFaults()` (`getrusage` `ru_minflt` counter)
 * 3. Workload 3 (Hardware MMU Page Walks, L2 dTLB Thrashing & HugePages):
 *    - `OrderLevelSlot` (`alignas(64)` 64-byte L1D cache-line price level)
 *    - `OrderBookArena` (`MADV_NOHUGEPAGE` 4 KiB pages vs. `MADV_HUGEPAGE`
 *      2 MiB HugePages with deterministic random page-crossing lookup chains).
 */

#ifndef HOTPATH_COMMON_SYSCALLS_TRAPS_AND_MMU_SYSCALLS_TRAPS_AND_MMU_H_
#define HOTPATH_COMMON_SYSCALLS_TRAPS_AND_MMU_SYSCALLS_TRAPS_AND_MMU_H_

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace hotpath::syscalls_traps_and_mmu {

/** @brief Standard x86-64 base page size (4 KiB = 4,096 bytes). */
inline constexpr std::size_t kPageSize4KiB = 4096;

/** @brief x86-64 L2 Page-Directory HugePage size (2 MiB = 512 x 4 KiB). */
inline constexpr std::size_t kHugePageSize2MiB = 2ULL * 1024ULL * 1024ULL;

/** @brief Number of 4 KiB pages inside a single 2 MiB batch buffer (512). */
inline constexpr std::size_t kPagesPer2MiB = kHugePageSize2MiB / kPageSize4KiB;

/** @brief L1D working-set size (48 KiB on AMD Zen 5). */
inline constexpr std::size_t kL1DWorkingSetBytes = 48ULL * 1024ULL;

/** @brief 32 MiB Order Book size (fits inside Zen 5's 64 MiB L3 cache). */
inline constexpr std::size_t kOrderBook32MiB = 32ULL * 1024ULL * 1024ULL;

/** @brief 512 MiB Order Book size (131,072 4 KiB pages vs 256 HugePages). */
inline constexpr std::size_t kOrderBook512MiB = 512ULL * 1024ULL * 1024ULL;

/**
 * @brief Pins the calling thread to a specific physical CPU core ID.
 * @param core_id Physical core index.
 */
inline void PinThreadToCore(int core_id) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/**
 * @brief Reads monotonic time in nanoseconds via the Linux Ring 3 vDSO
 *        (`__vdso_clock_gettime` reading `[vvar]` Seqlock + `rdtscp`).
 * @return Monotonic timestamp in nanoseconds.
 */
[[nodiscard]] inline std::uint64_t VdsoNowNs() noexcept {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

/**
 * @brief Reads monotonic time in nanoseconds by forcing a Ring 0 kernel trap
 *        via `syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts)`.
 * @return Monotonic timestamp in nanoseconds.
 */
[[nodiscard]] inline std::uint64_t RawSyscallNowNs() noexcept {
  struct timespec ts;
  syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

/**
 * @brief Queries the OS kernel thread ID by trapping into Ring 0 via
 *        `syscall(SYS_gettid)`.
 * @return Calling thread's kernel `pid_t`.
 */
[[nodiscard]] inline pid_t RawSyscallGettid() noexcept {
  return static_cast<pid_t>(syscall(SYS_gettid));
}

/**
 * @brief Returns the calling thread's OS thread ID (`pid_t`) cached once in
 *        Ring 3 Thread-Local Storage (`%fs` segment base on x86-64).
 * @return Cached OS thread ID.
 */
[[nodiscard]] inline pid_t CachedThreadId() noexcept {
  thread_local const pid_t cached_tid = static_cast<pid_t>(syscall(SYS_gettid));
  return cached_tid;
}

/**
 * @brief Reads the calling process's cumulative minor page fault count
 *        (`ru_minflt`) via `getrusage(RUSAGE_SELF)`.
 * @return Cumulative minor page faults handled by the kernel.
 */
[[nodiscard]] inline long ReadMinorFaults() noexcept {
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_minflt;
}

/**
 * @brief Writes 1 byte into each 4 KiB page of `buf` across `num_pages` pages,
 *        skewing the cache-line offset `(p & 63) * 64` within each page to
 *        prevent 4 KiB set-associativity aliasing in the L1D/L2 caches.
 * @param buf Base pointer to the page-aligned memory region.
 * @param num_pages Number of 4 KiB pages to touch.
 * @param value Byte value to store into each page.
 */
inline void TouchPages4KiB(std::uint8_t* buf, std::size_t num_pages,
                           std::uint8_t value) noexcept {
  for (std::size_t p = 0; p < num_pages; ++p) {
    const std::size_t cl_offset = (p & 63ULL) * 64ULL;
    buf[p * kPageSize4KiB + cl_offset] = value;
  }
}

/**
 * @class PrefaultedArenaPool
 * @brief Pre-faulted (`MAP_POPULATE`), `mlock`-pinned Ring 3 buffer pool that
 *        eliminates runtime Interrupt 14 (`#PF`) traps and `munmap` TLB
 *        shootdowns on the critical path.
 */
class PrefaultedArenaPool {
 public:
  /**
   * @brief Allocates and pre-faults `bytes` of anonymous memory in Ring 3.
   * @param bytes Size of the arena in bytes (rounded up to 4 KiB).
   */
  explicit PrefaultedArenaPool(std::size_t bytes = kHugePageSize2MiB) noexcept
      : size_bytes_((bytes + kPageSize4KiB - 1) & ~(kPageSize4KiB - 1)) {
    void* p = mmap(nullptr, size_bytes_, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (p != MAP_FAILED) {
      arena_ = static_cast<std::uint8_t*>(p);
      mlock(arena_, size_bytes_);
      std::memset(arena_, 1, size_bytes_);
    }
  }

  ~PrefaultedArenaPool() noexcept {
    if (arena_ != nullptr) {
      munmap(arena_, size_bytes_);
    }
  }

  PrefaultedArenaPool(const PrefaultedArenaPool&) = delete;
  PrefaultedArenaPool& operator=(const PrefaultedArenaPool&) = delete;

  /**
   * @brief Acquires the pre-faulted buffer in Ring 3 with zero syscalls.
   * @return Pointer to the resident buffer.
   */
  [[nodiscard]] std::uint8_t* Acquire() noexcept { return arena_; }

  /** @brief Returns the size of the arena in bytes. */
  [[nodiscard]] std::size_t size_bytes() const noexcept { return size_bytes_; }

 private:
  std::size_t size_bytes_ = 0;
  std::uint8_t* arena_ = nullptr;
};

/**
 * @struct OrderLevelSlot
 * @brief 64-byte cache-line-aligned price level node for MMU/TLB benchmarks.
 */
struct alignas(64) OrderLevelSlot {
  std::uint64_t next_slot_index;
  std::uint64_t price_ticks;
  std::uint64_t quantity_lots;
  std::uint64_t order_count;
  std::uint64_t timestamp_ns;
  std::uint64_t instrument_id;
  std::uint64_t venue_tag;
  std::uint64_t checksum;
};

static_assert(sizeof(OrderLevelSlot) == 64,
              "OrderLevelSlot must be exactly one 64-byte cache line.");

/**
 * @class OrderBookArena
 * @brief Allocates a page-aligned order book arena backed by either standard
 *        4 KiB pages (`MADV_NOHUGEPAGE`) or 2 MiB Transparent HugePages
 *        (`MADV_HUGEPAGE`) and initializes a deterministic random lookup table
 *        spanning all 4 KiB pages.
 */
class OrderBookArena {
 public:
  /**
   * @brief Constructs and pre-faults the order book arena.
   * @param total_bytes Total arena size in bytes (multiple of 2 MiB).
   * @param use_hugepages True to advise `MADV_HUGEPAGE` (2 MiB PDEs), false
   *        for `MADV_NOHUGEPAGE` (4 KiB PTEs).
   * @param num_random_indices Number of pre-computed random lookup indices.
   */
  OrderBookArena(std::size_t total_bytes, bool use_hugepages,
                 std::size_t num_random_indices = 1ULL << 20)
      : total_bytes_((total_bytes + kHugePageSize2MiB - 1) &
                     ~(kHugePageSize2MiB - 1)) {
    const std::size_t alloc_len = total_bytes_ + kHugePageSize2MiB;
    void* raw = mmap(nullptr, alloc_len, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    raw_base_ = raw;
    raw_len_ = alloc_len;

    const std::uintptr_t raw_addr = reinterpret_cast<std::uintptr_t>(raw);
    const std::uintptr_t aligned_addr =
        (raw_addr + kHugePageSize2MiB - 1) & ~(kHugePageSize2MiB - 1);
    slots_ = reinterpret_cast<OrderLevelSlot*>(aligned_addr);
    num_slots_ = total_bytes_ / sizeof(OrderLevelSlot);

    madvise(slots_, total_bytes_,
            use_hugepages ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);

    // Touch every slot to fault in all backing 4 KiB or 2 MiB physical frames.
    for (std::size_t i = 0; i < num_slots_; ++i) {
      slots_[i].next_slot_index = i;
      slots_[i].price_ticks = 10'000ULL + (i & 0xFFFFULL);
      slots_[i].quantity_lots = 100ULL + (i & 0xFFULL);
      slots_[i].order_count = 1ULL + (i & 0x7ULL);
      slots_[i].timestamp_ns = i * 10ULL;
      slots_[i].instrument_id = static_cast<std::uint64_t>(i & 0x3FFULL);
      slots_[i].venue_tag = 0x4E415344ULL;
      slots_[i].checksum = slots_[i].price_ticks ^ slots_[i].quantity_lots;
    }
#ifdef MADV_COLLAPSE
    if (use_hugepages) {
      madvise(slots_, total_bytes_, MADV_COLLAPSE);
    }
#endif

    // Build a deterministic random index sequence touching all 4 KiB pages
    // while skewing the cache-line offset `(p & 63)` so hot slots stay
    // L3-cache resident (isolating MMU dTLB misses from DRAM row-buffer
    // misses).
    constexpr std::size_t kSlotsPer4KiBPage =
        kPageSize4KiB / sizeof(OrderLevelSlot);
    const std::size_t num_pages = total_bytes_ / kPageSize4KiB;

    random_indices_.resize(num_random_indices);
    std::mt19937_64 rng(0xC0FFEE11ULL);
    std::uniform_int_distribution<std::size_t> page_dist(0, num_pages - 1);
    for (std::size_t i = 0; i < num_random_indices; ++i) {
      const std::size_t p = page_dist(rng);
      const std::size_t cl_in_page = p & (kSlotsPer4KiBPage - 1);
      random_indices_[i] = p * kSlotsPer4KiBPage + cl_in_page;
    }
  }

  ~OrderBookArena() noexcept {
    if (raw_base_ != nullptr && raw_base_ != MAP_FAILED) {
      munmap(raw_base_, raw_len_);
    }
  }

  OrderBookArena(const OrderBookArena&) = delete;
  OrderBookArena& operator=(const OrderBookArena&) = delete;

  /** @brief Returns a const pointer to the aligned `OrderLevelSlot` array. */
  [[nodiscard]] const OrderLevelSlot* slots() const noexcept { return slots_; }

  /** @brief Returns the pre-computed random lookup index vector. */
  [[nodiscard]] const std::vector<std::size_t>& random_indices()
      const noexcept {
    return random_indices_;
  }

  /** @brief Returns total number of 64-byte slots in the arena. */
  [[nodiscard]] std::size_t num_slots() const noexcept { return num_slots_; }

 private:
  void* raw_base_ = nullptr;
  std::size_t raw_len_ = 0;
  std::size_t total_bytes_ = 0;
  OrderLevelSlot* slots_ = nullptr;
  std::size_t num_slots_ = 0;
  std::vector<std::size_t> random_indices_;
};

}  // namespace hotpath::syscalls_traps_and_mmu

#endif  // HOTPATH_COMMON_SYSCALLS_TRAPS_AND_MMU_SYSCALLS_TRAPS_AND_MMU_H_
