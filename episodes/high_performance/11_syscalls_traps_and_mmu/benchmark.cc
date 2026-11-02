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

// ============================================================================
// Hot Path C++ — Episode 11 Production Benchmark Suite (Forensic Edition)
// File: episodes/11_syscalls_traps_and_mmu/benchmark.cc
// ============================================================================

#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

namespace hotpath::episode11 {
namespace {

constexpr size_t kPageSize4KiB = 4096;
constexpr size_t kHugePageSize2MiB = 2ULL << 20;
constexpr size_t kArenaBytes64KiB = 64ULL * 1024;
constexpr size_t kArenaBytes2MiB = 2ULL << 20;  // 512 x 4 KiB pages
constexpr size_t kWorkingSet48KiB = 48ULL * 1024;

/**
 * @brief Pins the calling thread to a fixed hardware CPU core to prevent
 * cross-CCD thread migration during microarchitectural benchmarks.
 *
 * @param core_id Target logical CPU core index.
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  ::sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

/**
 * @brief Queries the calling thread's cumulative minor page fault counter
 * (`ru_minflt`) via `getrusage(RUSAGE_THREAD)`.
 *
 * @return Cumulative minor page faults recorded for the calling thread.
 */
inline long GetThreadMinorFaults() noexcept {
  struct rusage usage {};
  ::getrusage(RUSAGE_THREAD, &usage);
  return usage.ru_minflt;
}

/**
 * @brief Returns the calling thread's kernel Thread ID (TID), cached in
 * Ring 3 thread-local storage (`%fs:0x...`) after a single warm-up syscall.
 *
 * @return Calling thread's `pid_t` TID.
 */
inline pid_t GetCachedTid() noexcept {
  thread_local const pid_t kCachedTid =
      static_cast<pid_t>(::syscall(SYS_gettid));
  return kCachedTid;
}

// ============================================================================
// CASE FILE 1: TELEMETRY, VDSO SEQLOCK VS. RING 0 SYSCALLS & L1D EVICTION TAX
// ============================================================================

/**
 * @brief Benchmarks `clock_gettime(CLOCK_MONOTONIC)` via the Linux `[vdso]`
 * virtual dynamic shared object in Ring 3 (`vvar` seqlock + `rdtsc`).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case1_ClockGettime_vDSO_Ring3(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    struct timespec ts {};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    benchmark::DoNotOptimize(ts.tv_nsec);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Case1_ClockGettime_vDSO_Ring3)->UseRealTime();

/**
 * @brief Benchmarks raw `syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts)`
 * forcing a full Ring 3 -> Ring 0 -> Ring 3 privilege transition.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case1_ClockGettime_RawSyscall_Ring0(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    struct timespec ts {};
    ::syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    benchmark::DoNotOptimize(ts.tv_nsec);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Case1_ClockGettime_RawSyscall_Ring0)->UseRealTime();

/**
 * @brief Benchmarks raw `syscall(SYS_gettid)` in a back-to-back tight loop
 * where kernel instructions and stacks remain warm in L1 caches.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case1_Gettid_RawSyscall_TightLoop(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    pid_t tid = static_cast<pid_t>(::syscall(SYS_gettid));
    benchmark::DoNotOptimize(tid);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Case1_Gettid_RawSyscall_TightLoop)->UseRealTime();

/**
 * @brief Benchmarks `GetCachedTid()` reading the thread ID from Ring 3
 * `thread_local` storage (`%fs` segment offset) with zero kernel traps.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case1_Gettid_ThreadLocal_Cached(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    pid_t tid = GetCachedTid();
    benchmark::DoNotOptimize(tid);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Case1_Gettid_ThreadLocal_Cached)->UseRealTime();

/**
 * @brief Benchmarks touching a 48 KiB L1D-resident working set interleaved
 * with a Ring 3 `thread_local` TID lookup (zero L1D/L1I/BTB kernel pollution).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case1_AppWorkingSet48KiB_WithCachedTid(benchmark::State& state) {
  PinThreadToCore(0);
  alignas(64) std::vector<uint64_t> l1_working_set(
      kWorkingSet48KiB / sizeof(uint64_t), 1ULL);
  uint64_t acc = 0;
  for (auto _ : state) {
    for (size_t i = 0; i < l1_working_set.size(); i += 8) {
      acc += l1_working_set[i]++;
    }
    acc += static_cast<uint64_t>(GetCachedTid());
    benchmark::DoNotOptimize(acc);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Case1_AppWorkingSet48KiB_WithCachedTid)->UseRealTime();

/**
 * @brief Benchmarks touching a 48 KiB L1D-resident working set interleaved
 * with a raw `syscall(SYS_gettid)` trap, measuring the collateral L1D/L1I/BTB
 * eviction penalty inflicted on user-space state.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case1_AppWorkingSet48KiB_WithRawSyscall(benchmark::State& state) {
  PinThreadToCore(0);
  alignas(64) std::vector<uint64_t> l1_working_set(
      kWorkingSet48KiB / sizeof(uint64_t), 1ULL);
  uint64_t acc = 0;
  for (auto _ : state) {
    for (size_t i = 0; i < l1_working_set.size(); i += 8) {
      acc += l1_working_set[i]++;
    }
    acc += static_cast<uint64_t>(::syscall(SYS_gettid));
    benchmark::DoNotOptimize(acc);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Case1_AppWorkingSet48KiB_WithRawSyscall)->UseRealTime();

// ============================================================================
// CASE FILE 2: DEMAND PAGING, #PF TRAPS & THE GLIBC 128 KiB VECTOR CLIFF
// ============================================================================

/**
 * @brief Benchmarks `mmap(2 MiB)` + `munmap(2 MiB)` without touching any
 * bytes, proving `mmap` only allocates a `vm_area_struct` with 0 page faults.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case2_Mmap2MiB_VirtualAllocOnly(benchmark::State& state) {
  PinThreadToCore(0);
  const long faults_before = GetThreadMinorFaults();
  for (auto _ : state) {
    void* ptr = ::mmap(nullptr, kArenaBytes2MiB, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    benchmark::DoNotOptimize(ptr);
    ::munmap(ptr, kArenaBytes2MiB);
  }
  const long faults_delta = GetThreadMinorFaults() - faults_before;
  state.counters["minor_faults/op"] =
      static_cast<double>(faults_delta) / state.iterations();
}
BENCHMARK(BM_Case2_Mmap2MiB_VirtualAllocOnly)->UseRealTime();

/**
 * @brief Benchmarks the first write pass across a freshly `mmap`'d 2 MiB
 * region (512 x 4 KiB pages), triggering 512 synchronous `#PF` (Interrupt 14)
 * minor page faults into the kernel page allocator and 4 KiB page zeroing.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case2_Mmap2MiB_FirstWritePass_512PageFaults(benchmark::State& state) {
  PinThreadToCore(0);
  const long faults_before = GetThreadMinorFaults();
  for (auto _ : state) {
    state.PauseTiming();
    auto* buf = static_cast<uint8_t*>(
        ::mmap(nullptr, kArenaBytes2MiB, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    ::madvise(buf, kArenaBytes2MiB, MADV_NOHUGEPAGE);
    state.ResumeTiming();

    for (size_t offset = 0; offset < kArenaBytes2MiB; offset += kPageSize4KiB) {
      buf[offset] = 0x42;
    }
    benchmark::DoNotOptimize(buf);
    benchmark::ClobberMemory();

    state.PauseTiming();
    ::munmap(buf, kArenaBytes2MiB);
    state.ResumeTiming();
  }
  const long faults_delta = GetThreadMinorFaults() - faults_before;
  state.counters["minor_faults/op"] =
      static_cast<double>(faults_delta) / state.iterations();
  state.SetItemsProcessed(state.iterations() * 512);
}
BENCHMARK(BM_Case2_Mmap2MiB_FirstWritePass_512PageFaults)->UseRealTime();

/**
 * @brief Benchmarks writing to 512 pages of a pre-faulted, `MAP_POPULATE` +
 * `mlock`'d 2 MiB arena with valid PTEs (`P=1, R/W=1`) and zero `#PF` traps.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case2_PrefaultedArena2MiB_SecondWritePass(benchmark::State& state) {
  PinThreadToCore(0);
  auto* buf = static_cast<uint8_t*>(
      ::mmap(nullptr, kArenaBytes2MiB, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
  ::mlock(buf, kArenaBytes2MiB);
  std::memset(buf, 1, kArenaBytes2MiB);

  const long faults_before = GetThreadMinorFaults();
  for (auto _ : state) {
    for (size_t offset = 0; offset < kArenaBytes2MiB; offset += kPageSize4KiB) {
      buf[offset] = 0x42;
    }
    benchmark::DoNotOptimize(buf);
    benchmark::ClobberMemory();
  }
  const long faults_delta = GetThreadMinorFaults() - faults_before;
  state.counters["minor_faults/op"] =
      static_cast<double>(faults_delta) / state.iterations();
  state.SetItemsProcessed(state.iterations() * 512);

  ::munlock(buf, kArenaBytes2MiB);
  ::munmap(buf, kArenaBytes2MiB);
}
BENCHMARK(BM_Case2_PrefaultedArena2MiB_SecondWritePass)->UseRealTime();

/**
 * @brief Benchmarks allocating and destroying a temporary 64 KiB `std::vector`
 * (`< 128 KiB` `M_MMAP_THRESHOLD`) inside the loop, demonstrating that `glibc`
 * recycles the chunk in Ring 3 bins with 0 minor page faults.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case2_TempStdVector64KiB_RecycledHeap(benchmark::State& state) {
  PinThreadToCore(0);
  ::mallopt(M_MMAP_THRESHOLD, 128 * 1024);
  // Warm up the Ring 3 heap bin once before timing.
  {
    std::vector<uint8_t> warmup(kArenaBytes64KiB, 0x7F);
    benchmark::DoNotOptimize(warmup.data());
  }
  const long faults_before = GetThreadMinorFaults();
  for (auto _ : state) {
    std::vector<uint8_t> scratch(kArenaBytes64KiB);
    uint8_t* ptr = scratch.data();
    for (size_t offset = 0; offset < kArenaBytes64KiB;
         offset += kPageSize4KiB) {
      ptr[offset] = 0x7F;
    }
    benchmark::DoNotOptimize(ptr);
    benchmark::ClobberMemory();
  }
  const long faults_delta = GetThreadMinorFaults() - faults_before;
  state.counters["minor_faults/op"] =
      static_cast<double>(faults_delta) / state.iterations();
}
BENCHMARK(BM_Case2_TempStdVector64KiB_RecycledHeap)->UseRealTime();

/**
 * @brief Benchmarks allocating and destroying a temporary 2 MiB `std::vector`
 * (`>= 128 KiB` `M_MMAP_THRESHOLD`) per call, triggering `mmap`/`munmap` churn
 * and 512+ minor page faults on every iteration.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case2_TempStdVector2MiB_PerCallMmapChurn(benchmark::State& state) {
  PinThreadToCore(0);
  // Lock glibc's 128 KiB M_MMAP_THRESHOLD to prevent dynamic adjustment from
  // hiding the default 128 KiB mmap/munmap churn threshold across iterations.
  ::mallopt(M_MMAP_THRESHOLD, 128 * 1024);
  const long faults_before = GetThreadMinorFaults();
  for (auto _ : state) {
    std::vector<uint8_t> scratch(kArenaBytes2MiB);
    uint8_t* ptr = scratch.data();
    for (size_t offset = 0; offset < kArenaBytes2MiB; offset += kPageSize4KiB) {
      ptr[offset] = 0x7F;
    }
    benchmark::DoNotOptimize(ptr);
    benchmark::ClobberMemory();
  }
  const long faults_delta = GetThreadMinorFaults() - faults_before;
  state.counters["minor_faults/op"] =
      static_cast<double>(faults_delta) / state.iterations();
}
BENCHMARK(BM_Case2_TempStdVector2MiB_PerCallMmapChurn)->UseRealTime();

/**
 * @brief Benchmarks reusing a pre-reserved 2 MiB `std::vector` via `.clear()`
 * and `.resize()`, retaining physical pages in Ring 3 with 0 page faults.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case2_ReusedStdVector2MiB_ZeroPageFaults(benchmark::State& state) {
  PinThreadToCore(0);
  std::vector<uint8_t> reusable_scratch(kArenaBytes2MiB, 0x01);
  const long faults_before = GetThreadMinorFaults();
  for (auto _ : state) {
    reusable_scratch.clear();
    reusable_scratch.resize(kArenaBytes2MiB);
    uint8_t* ptr = reusable_scratch.data();
    for (size_t offset = 0; offset < kArenaBytes2MiB; offset += kPageSize4KiB) {
      ptr[offset] = 0x7F;
    }
    benchmark::DoNotOptimize(ptr);
    benchmark::ClobberMemory();
  }
  const long faults_delta = GetThreadMinorFaults() - faults_before;
  state.counters["minor_faults/op"] =
      static_cast<double>(faults_delta) / state.iterations();
}
BENCHMARK(BM_Case2_ReusedStdVector2MiB_ZeroPageFaults)->UseRealTime();

// ============================================================================
// CASE FILE 3: THE MMU, 4-LEVEL PAGE TABLES, 12 MiB TLB REACH & 2 MiB HUGEPAGES
// ============================================================================

/**
 * @brief 64-byte cache-line-aligned order book slot used to benchmark MMU
 * page-table walks and dTLB reach.
 */
struct alignas(64) OrderSlot {
  uint64_t order_id;
  int64_t price_ticks;
  uint64_t quantity;
  uint64_t next_probe_idx;
  uint8_t padding[32];
};
static_assert(sizeof(OrderSlot) == 64, "OrderSlot must be 1 cache line (64B)");

/**
 * @brief Allocates a 2 MiB-aligned `OrderSlot` arena backed by either 4 KiB
 * standard pages (`MADV_NOHUGEPAGE`) or 2 MiB Transparent HugePages
 * (`MADV_HUGEPAGE` + `MADV_COLLAPSE`).
 *
 * @param total_bytes Total arena size in bytes (multiple of 2 MiB).
 * @param use_hugepages True to back the arena with 2 MiB PMD HugePages.
 * @return Pointer to the initialized `OrderSlot` arena, or `nullptr` on error.
 */
OrderSlot* AllocateOrderBookArena(size_t total_bytes, bool use_hugepages) {
  const size_t raw_bytes = total_bytes + kHugePageSize2MiB;
  void* raw = ::mmap(nullptr, raw_bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (raw == MAP_FAILED) return nullptr;

  const uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw);
  const uintptr_t aligned_addr =
      (raw_addr + kHugePageSize2MiB - 1) & ~(kHugePageSize2MiB - 1);

  const size_t lead_slop = aligned_addr - raw_addr;
  if (lead_slop > 0) {
    ::munmap(raw, lead_slop);
  }
  const size_t tail_slop = raw_bytes - lead_slop - total_bytes;
  if (tail_slop > 0) {
    ::munmap(reinterpret_cast<void*>(aligned_addr + total_bytes), tail_slop);
  }

  void* aligned_ptr = reinterpret_cast<void*>(aligned_addr);
  if (use_hugepages) {
    ::madvise(aligned_ptr, total_bytes, MADV_HUGEPAGE);
  } else {
    ::madvise(aligned_ptr, total_bytes, MADV_NOHUGEPAGE);
  }

  auto* slots = static_cast<OrderSlot*>(aligned_ptr);
  const size_t num_slots = total_bytes / sizeof(OrderSlot);
  for (size_t i = 0; i < num_slots; ++i) {
    slots[i].order_id = i;
    slots[i].price_ticks = static_cast<int64_t>(i * 7 + 100);
    slots[i].quantity = 10;
    slots[i].next_probe_idx = i;
  }
#ifdef MADV_COLLAPSE
  if (use_hugepages) {
    ::madvise(aligned_ptr, total_bytes, MADV_COLLAPSE);
  }
#endif
  return slots;
}

/**
 * @brief Executes pseudo-random order book lookups across `arena_bytes` with
 * a data-dependent index (`sum & 0xFF`) that defeats speculative page
 * prefetching while measuring MMU TLB hit vs. page-table walk latency.
 *
 * @param state Google Benchmark state controller.
 * @param arena_bytes Total order book footprint in bytes (32 MiB or 512 MiB).
 * @param use_hugepages True for 2 MiB HugePages, false for 4 KiB pages.
 */
void RunRandomLookupBenchmark(benchmark::State& state, size_t arena_bytes,
                              bool use_hugepages) {
  PinThreadToCore(0);
  OrderSlot* book = AllocateOrderBookArena(arena_bytes, use_hugepages);
  const size_t num_pages_4k = arena_bytes / kPageSize4KiB;

  constexpr size_t kRingSize = 1 << 20;
  std::vector<uint32_t> probe_ring(kRingSize);
  std::mt19937_64 rng(0xC0FFEE11ULL);
  std::uniform_int_distribution<size_t> page_dist(0, num_pages_4k - 1);
  std::uniform_int_distribution<size_t> slot_in_page_dist(
      0, (kPageSize4KiB / sizeof(OrderSlot)) - 1);

  for (size_t i = 0; i < kRingSize; ++i) {
    const size_t page = page_dist(rng);
    const size_t slot_offset = slot_in_page_dist(rng);
    probe_ring[i] = static_cast<uint32_t>(
        page * (kPageSize4KiB / sizeof(OrderSlot)) + slot_offset);
  }

  size_t cursor = 0;
  int64_t sum = 0;
  for (auto _ : state) {
    const uint32_t idx = probe_ring[(cursor + static_cast<size_t>(sum & 0xFF)) &
                                    (kRingSize - 1)];
    sum += book[idx].price_ticks;
    ++cursor;
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations());
  state.counters["arena_MiB"] = static_cast<double>(arena_bytes >> 20);
  state.counters["tlb_entries_needed"] =
      use_hugepages ? static_cast<double>(arena_bytes / kHugePageSize2MiB)
                    : static_cast<double>(arena_bytes / kPageSize4KiB);

  ::munmap(book, arena_bytes);
}

/**
 * @brief Benchmarks random lookups across a 32 MiB L3-resident order book
 * backed by 8,192 x 4 KiB pages (exceeding the 3,072-entry L2 dTLB reach).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case3_L3Resident32MiB_4KiBPages_TLBThrash(benchmark::State& state) {
  RunRandomLookupBenchmark(state, 32ULL << 20, /*use_hugepages=*/false);
}
BENCHMARK(BM_Case3_L3Resident32MiB_4KiBPages_TLBThrash)->UseRealTime();

/**
 * @brief Benchmarks random lookups across a 32 MiB L3-resident order book
 * backed by 16 x 2 MiB HugePages (100% L1 dTLB resident).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case3_L3Resident32MiB_2MiBHugePages_TLBHit(benchmark::State& state) {
  RunRandomLookupBenchmark(state, 32ULL << 20, /*use_hugepages=*/true);
}
BENCHMARK(BM_Case3_L3Resident32MiB_2MiBHugePages_TLBHit)->UseRealTime();

/**
 * @brief Benchmarks random lookups across a 512 MiB DRAM order book backed by
 * 131,072 x 4 KiB pages (94%+ dTLB miss rate, 4-level radix page walks).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case3_OrderBook512MiB_4KiBPages_TLBThrash(benchmark::State& state) {
  RunRandomLookupBenchmark(state, 512ULL << 20, /*use_hugepages=*/false);
}
BENCHMARK(BM_Case3_OrderBook512MiB_4KiBPages_TLBThrash)->UseRealTime();

/**
 * @brief Benchmarks random lookups across a 512 MiB DRAM order book backed by
 * 256 x 2 MiB HugePages (fits inside the 2,048-entry 2M L2 dTLB, 3-level PMD).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Case3_OrderBook512MiB_2MiBHugePages_TLBHit(benchmark::State& state) {
  RunRandomLookupBenchmark(state, 512ULL << 20, /*use_hugepages=*/true);
}
BENCHMARK(BM_Case3_OrderBook512MiB_2MiBHugePages_TLBHit)->UseRealTime();

}  // namespace
}  // namespace hotpath::episode11
