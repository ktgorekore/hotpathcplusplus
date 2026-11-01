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
 * @file benchmark.cc
 * @brief Hot Path C++ — Episode 11 Benchmark Suite: Every Pointer in Your C++
 *        Program Is a Lie (Syscalls, vDSO Seqlocks, #PF Page Fault Traps,
 *        Hardware MMU 4-Level Page Walks, dTLB Thrashing & 2 MiB HugePages).
 *
 * Benchmarks (AMD Ryzen 9 9950X 16-Core Zen 5):
 * 1. Workload 1 — Ring 3 vDSO vs. Ring 0 Syscalls & Cache Pollution:
 *    - BM_Telemetry_ClockGettime_vDSO (`clock_gettime` via `[vvar]` Seqlock)
 *    - BM_Telemetry_ClockGettime_RawSyscall (`syscall(SYS_clock_gettime)`)
 *    - BM_Telemetry_Gettid_RawSyscall (`syscall(SYS_gettid)`)
 *    - BM_Telemetry_Gettid_CachedTLS (`thread_local` cached TID)
 *    - BM_Syscall_WorkingSet48KiB_Ring3Only vs. WithRing0Syscall (48 KiB L1D
 *      working set with/without Ring 0 syscall transition)
 * 2. Workload 2 — Demand Paging, First-Touch `#PF` Traps & `MAP_POPULATE`:
 *    - BM_DemandPaging_Mmap2MiB_AllocCallOnly (`mmap(2 MiB)` VMA IOU creation)
 *    - BM_DemandPaging_FirstTouch_512Pages_Trap (`2 MiB` lazy `mmap`, 512 `#PF`
 *      traps into Ring 0)
 *    - BM_DemandPaging_GlibcVector2MiB_PerRequestChurn (`std::vector` >= 128
 * KiB `M_MMAP_THRESHOLD` `mmap`/`munmap` churn trap)
 *    - BM_DemandPaging_PrefaultedPool_512Pages_Resident (`MAP_POPULATE` +
 *      `mlock` reusable Ring 3 arena — 0 `#PF` traps)
 * 3. Workload 3 — Hardware MMU, 4-Level Page Walks, TLB & 2 MiB HugePages:
 *    - BM_OrderBook_32MiB_4KiBPages vs. BM_OrderBook_32MiB_2MiBHugePages
 *    - BM_OrderBook_512MiB_4KiBPages vs. BM_OrderBook_512MiB_2MiBHugePages
 */

#include "benchmark/benchmark.h"

#include <immintrin.h>
#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#ifndef MADV_COLLAPSE
#define MADV_COLLAPSE 25
#endif

namespace hotpath::episode11 {
namespace {

constexpr std::size_t kPageSize4KiB = 4096ULL;
constexpr std::size_t kHugePageSize2MiB = 2ULL << 20;
constexpr std::size_t kPagesPer2MiB = kHugePageSize2MiB / kPageSize4KiB;  // 512
constexpr std::size_t kOrderBook32MiBBytes = 32ULL << 20;
constexpr std::size_t kOrderBook512MiBBytes = 512ULL << 20;

/**
 * @brief Pins the calling thread to physical Core 0 for deterministic PMU and
 *        TLB measurements.
 * @param core_id Physical core index (0..15).
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id % 16, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/**
 * @brief Returns the calling thread's OS Thread ID cached in Thread-Local
 *        Storage (TLS), avoiding repeated Ring 0 `SYS_gettid` traps.
 * @return Cached `pid_t` thread identifier.
 */
inline pid_t CachedThreadId() noexcept {
  thread_local const pid_t cached_tid = static_cast<pid_t>(syscall(SYS_gettid));
  return cached_tid;
}

// =============================================================================
// WORKLOAD 1: TELEMETRY & LOGGING (RING 3 vDSO vs. RING 0 SYSCALL)
// =============================================================================

/**
 * @brief Benchmarks `std::chrono::steady_clock::now()` / `clock_gettime`,
 *        which executes entirely in Ring 3 via `linux-vdso.so.1` and `[vvar]`
 *        Seqlock + `rdtscp`.
 * @param state Google Benchmark state.
 */
void BM_Telemetry_ClockGettime_vDSO(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    benchmark::DoNotOptimize(ts.tv_sec);
    benchmark::DoNotOptimize(ts.tv_nsec);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Telemetry_ClockGettime_vDSO)->UseRealTime();

/**
 * @brief Benchmarks forcing `clock_gettime(CLOCK_MONOTONIC)` through a real
 *        Ring 0 `syscall` instruction (simulating direct syscall or cloud VM
 *        `hpet`/`acpi_pm` clocksource fallback).
 * @param state Google Benchmark state.
 */
void BM_Telemetry_ClockGettime_RawSyscall(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    struct timespec ts;
    syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    benchmark::DoNotOptimize(ts.tv_sec);
    benchmark::DoNotOptimize(ts.tv_nsec);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Telemetry_ClockGettime_RawSyscall)->UseRealTime();

/**
 * @brief Benchmarks uncached `syscall(SYS_gettid)` per log frame, crossing
 *        from Ring 3 (`CPL=3`) into Ring 0 (`CPL=0`) on every invocation.
 * @param state Google Benchmark state.
 */
void BM_Telemetry_Gettid_RawSyscall(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    benchmark::DoNotOptimize(tid);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Telemetry_Gettid_RawSyscall)->UseRealTime();

/**
 * @brief Benchmarks reading the thread ID from a `thread_local` variable
 *        initialized once per thread (`0` Ring 0 syscalls on the hot path).
 * @param state Google Benchmark state.
 */
void BM_Telemetry_Gettid_CachedTLS(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    pid_t tid = CachedThreadId();
    benchmark::DoNotOptimize(tid);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Telemetry_Gettid_CachedTLS)->UseRealTime();

/**
 * @brief Simulates a hot 48 KiB userspace working set (matching Zen 5's 48 KiB
 *        L1D cache capacity = 768 cache lines) to measure L1D and BTB state
 *        pollution when interleaved with a Ring 0 syscall.
 */
struct alignas(64) HotWorkingSet48KiB {
  std::array<std::uint64_t, 6144> words{};

  HotWorkingSet48KiB() noexcept {
    for (std::size_t i = 0; i < words.size(); ++i) {
      words[i] = (i * 0x9E3779B97F4A7C15ULL) ^ 0xBF58476D1CE4E5B9ULL;
    }
  }

  std::uint64_t StepHotWorkingSet(std::uint64_t seed) noexcept {
    std::uint64_t acc = seed;
    for (std::size_t cl = 0; cl < 6144; cl += 8) {
      const std::uint64_t v = words[cl] + acc;
      if ((v & 1ULL) == 0ULL) {
        acc ^= (v >> 3) + 0x517CC1B727220A95ULL;
      } else {
        acc += (v << 2) ^ 0x94D049BB133111EBULL;
      }
      words[cl] = acc;
    }
    return acc;
  }
};

/**
 * @brief Benchmarks the 48 KiB L1D hot userspace working set + vDSO
 *        `clock_gettime` + cached TLS `gettid` (stays 100% in Ring 3).
 * @param state Google Benchmark state.
 */
void BM_Syscall_WorkingSet48KiB_Ring3Only(benchmark::State& state) {
  PinThreadToCore(0);
  HotWorkingSet48KiB ws;
  std::uint64_t seed = 1ULL;
  for (auto _ : state) {
    seed = ws.StepHotWorkingSet(seed);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    pid_t tid = CachedThreadId();
    benchmark::DoNotOptimize(ts.tv_nsec);
    benchmark::DoNotOptimize(tid);
    benchmark::DoNotOptimize(seed);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Syscall_WorkingSet48KiB_Ring3Only)->UseRealTime();

/**
 * @brief Benchmarks the exact same 48 KiB L1D working set when telemetry
 * crosses into Ring 0 via `syscall(SYS_clock_gettime)` + `syscall(SYS_gettid)`
 * + `getrusage(RUSAGE_SELF)`, exposing both direct Ring 0 transition cost and
 * indirect L1D/BTB eviction tax.
 * @param state Google Benchmark state.
 */
void BM_Syscall_WorkingSet48KiB_WithRing0Syscall(benchmark::State& state) {
  PinThreadToCore(0);
  HotWorkingSet48KiB ws;
  std::uint64_t seed = 1ULL;
  for (auto _ : state) {
    seed = ws.StepHotWorkingSet(seed);
    struct timespec ts;
    syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    benchmark::DoNotOptimize(ts.tv_nsec);
    benchmark::DoNotOptimize(tid);
    benchmark::DoNotOptimize(seed);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Syscall_WorkingSet48KiB_WithRing0Syscall)->UseRealTime();

// =============================================================================
// WORKLOAD 2: DEMAND PAGING, #PF HARDWARE TRAPS & MAP_POPULATE POOLS
// =============================================================================

/**
 * @brief Measures the pure `mmap(2 MiB)` allocation call latency (creating the
 *        virtual `vm_area_struct` IOU without touching physical RAM).
 * @param state Google Benchmark state.
 */
void BM_DemandPaging_Mmap2MiB_AllocCallOnly(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    void* ptr = mmap(nullptr, kHugePageSize2MiB, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    benchmark::DoNotOptimize(ptr);
    state.PauseTiming();
    munmap(ptr, kHugePageSize2MiB);
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DemandPaging_Mmap2MiB_AllocCallOnly)->UseRealTime();

/**
 * @brief Measures the first-touch write pass across 512 newly mapped `4 KiB`
 *        pages (`2 MiB`), triggering 512 synchronous `#PF` hardware traps
 *        (Interrupt Vector 14) into Ring 0.
 * @param state Google Benchmark state.
 */
void BM_DemandPaging_FirstTouch_512Pages_Trap(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    state.PauseTiming();
    auto* buf = static_cast<std::uint8_t*>(
        mmap(nullptr, kHugePageSize2MiB, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    madvise(buf, kHugePageSize2MiB, MADV_NOHUGEPAGE);
    state.ResumeTiming();

    for (std::size_t p = 0; p < kPagesPer2MiB; ++p) {
      const std::size_t cl_offset = (p & 63ULL) * 64ULL;
      buf[p * kPageSize4KiB + cl_offset] = static_cast<std::uint8_t>(p + 1);
    }
    benchmark::DoNotOptimize(buf);
    benchmark::ClobberMemory();

    state.PauseTiming();
    munmap(buf, kHugePageSize2MiB);
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kPagesPer2MiB));
}
BENCHMARK(BM_DemandPaging_FirstTouch_512Pages_Trap)->UseRealTime();

/**
 * @brief Measures the full per-request temporary `std::vector<uint8_t>(2 MiB)`
 *        churn cycle when `glibc` `M_MMAP_THRESHOLD` is locked at `128 KiB`:
 *        `vector` allocates via `mmap`, zeros/faults all 512 pages (`#PF` traps
 *        into Ring 0), and calls `munmap` on scope exit!
 * @param state Google Benchmark state.
 */
void BM_DemandPaging_GlibcVector2MiB_PerRequestChurn(benchmark::State& state) {
  PinThreadToCore(0);
  mallopt(M_MMAP_THRESHOLD, 128 * 1024);
  for (auto _ : state) {
    std::vector<std::uint8_t> tmp(kHugePageSize2MiB);
    std::uint8_t* buf = tmp.data();
    for (std::size_t p = 0; p < kPagesPer2MiB; ++p) {
      const std::size_t cl_offset = (p & 63ULL) * 64ULL;
      buf[p * kPageSize4KiB + cl_offset] = 1;
    }
    benchmark::DoNotOptimize(buf);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kPagesPer2MiB));
}
BENCHMARK(BM_DemandPaging_GlibcVector2MiB_PerRequestChurn)->UseRealTime();

/**
 * @brief Measures writing to all 512 `4 KiB` pages (`2 MiB`) inside a
 *        pre-faulted (`MAP_POPULATE` + `mlock` + `MADV_HUGEPAGE`) reusable
 *        Ring 3 arena pool. Zero `#PF` traps occur on the critical path.
 * @param state Google Benchmark state.
 */
void BM_DemandPaging_PrefaultedPool_512Pages_Resident(benchmark::State& state) {
  PinThreadToCore(0);
  void* raw_map = mmap(nullptr, kHugePageSize2MiB * 2, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(raw_map);
  const std::uintptr_t aligned_addr =
      (addr + kHugePageSize2MiB - 1ULL) & ~(kHugePageSize2MiB - 1ULL);
  auto* arena = reinterpret_cast<std::uint8_t*>(aligned_addr);
  madvise(arena, kHugePageSize2MiB, MADV_HUGEPAGE);
  madvise(arena, kHugePageSize2MiB, MADV_COLLAPSE);
  mlock(arena, kHugePageSize2MiB);
  std::memset(arena, 0x42, kHugePageSize2MiB);

  std::uint8_t tag = 1;
  for (auto _ : state) {
    ++tag;
    for (std::size_t p = 0; p < kPagesPer2MiB; ++p) {
      const std::size_t cl_offset = (p & 63ULL) * 64ULL;
      arena[p * kPageSize4KiB + cl_offset] = tag;
    }
    benchmark::DoNotOptimize(arena);
    benchmark::ClobberMemory();
  }

  munlock(arena, kHugePageSize2MiB);
  munmap(raw_map, kHugePageSize2MiB * 2);
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kPagesPer2MiB));
}
BENCHMARK(BM_DemandPaging_PrefaultedPool_512Pages_Resident)->UseRealTime();

// =============================================================================
// WORKLOAD 3: HARDWARE MMU, 4-LEVEL PAGE WALKS, TLB & 2 MiB HUGEPAGES
// =============================================================================

/**
 * @struct alignas(64) OrderSlot
 * @brief 64-byte cache-line-sized financial order book / flat hash table entry.
 */
struct alignas(64) OrderSlot {
  std::uint64_t next_index;
  std::uint64_t order_id;
  std::int64_t price_ticks;
  std::uint64_t quantity;
  std::uint64_t timestamp_ns;
  std::uint64_t venue_flags;
  std::uint64_t reserved0;
  std::uint64_t reserved1;
};
static_assert(sizeof(OrderSlot) == 64, "OrderSlot must be 1 cache line (64B)");

struct MappedOrderBook {
  void* raw_map = nullptr;
  std::size_t raw_bytes = 0;
  OrderSlot* slots = nullptr;
  std::size_t start_index = 0;
};

/**
 * @brief Allocates a 2 MiB-aligned order book arena of `total_bytes`, applies
 *        either `MADV_NOHUGEPAGE` (`4 KiB` pages) or `MADV_HUGEPAGE` +
 *        `MADV_COLLAPSE` (`2 MiB` HugePages), and builds a single-cycle Sattolo
 *        page permutation across the active L3-resident order slots (1 active
 *        64B slot per 4 KiB page).
 * @param total_bytes Arena size in bytes.
 * @param use_hugepages True for `2 MiB` HugePages, false for `4 KiB` pages.
 * @return `MappedOrderBook` handle.
 */
MappedOrderBook AllocateOrderBookArena(std::size_t total_bytes,
                                       bool use_hugepages) {
  const std::size_t raw_bytes = total_bytes + kHugePageSize2MiB;
  void* raw_map = mmap(nullptr, raw_bytes, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (raw_map == MAP_FAILED) {
    std::abort();
  }
  const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(raw_map);
  const std::uintptr_t aligned_addr =
      (addr + kHugePageSize2MiB - 1ULL) & ~(kHugePageSize2MiB - 1ULL);
  auto* table = reinterpret_cast<OrderSlot*>(aligned_addr);

  if (use_hugepages) {
    madvise(table, total_bytes, MADV_HUGEPAGE);
  } else {
    madvise(table, total_bytes, MADV_NOHUGEPAGE);
  }

  // Touch all pages to wire PTEs
  const std::size_t num_slots = total_bytes / sizeof(OrderSlot);
  for (std::size_t i = 0; i < num_slots; i += 64) {
    table[i].order_id = i;
  }
  if (use_hugepages) {
    madvise(table, total_bytes, MADV_COLLAPSE);
  }

  // Build single-cycle Sattolo permutation across all 4 KiB pages
  const std::size_t num_pages = total_bytes / kPageSize4KiB;
  std::vector<std::size_t> page_perm(num_pages);
  for (std::size_t p = 0; p < num_pages; ++p) {
    page_perm[p] = p;
  }
  std::uint64_t seed = 0x9E3779B97F4A7C15ULL;
  for (std::size_t p = num_pages - 1; p > 0; --p) {
    seed ^= seed >> 12;
    seed ^= seed << 25;
    seed ^= seed >> 27;
    const std::size_t j = (seed * 0x2545F4914F6CDD1DULL) % p;
    std::swap(page_perm[p], page_perm[j]);
  }
  for (std::size_t p = 0; p < num_pages; ++p) {
    const std::size_t cur_page = page_perm[p];
    const std::size_t next_page = page_perm[(p + 1) % num_pages];
    const std::size_t cur_idx = cur_page * 64ULL + (cur_page & 63ULL);
    const std::size_t next_idx = next_page * 64ULL + (next_page & 63ULL);
    table[cur_idx].next_index = next_idx;
    table[cur_idx].price_ticks =
        static_cast<std::int64_t>(10000 + (p & 1023ULL));
  }

  // Warm the active order slots into L3 cache
  std::size_t idx = page_perm[0] * 64ULL + (page_perm[0] & 63ULL);
  std::int64_t warm_sink = 0;
  for (std::size_t p = 0; p < num_pages * 2; ++p) {
    warm_sink += table[idx].price_ticks;
    idx = table[idx].next_index;
  }
  benchmark::DoNotOptimize(warm_sink);

  return MappedOrderBook{raw_map, raw_bytes, table, idx};
}

/**
 * @brief Benchmarks random order-book lookups across a `32 MiB` table (`8,192`
 *        `4 KiB` pages) that fits inside Zen 5's L3 cache, but exceeds the
 *        `12 MiB` L2 dTLB reach by 2.67x when mapped with `4 KiB` pages
 *        (`MADV_NOHUGEPAGE`).
 * @param state Google Benchmark state.
 */
void BM_OrderBook_32MiB_4KiBPages(benchmark::State& state) {
  PinThreadToCore(0);
  MappedOrderBook ob = AllocateOrderBookArena(kOrderBook32MiBBytes, false);
  std::size_t idx = ob.start_index;
  std::int64_t sum = 0;

  for (auto _ : state) {
    const OrderSlot& slot = ob.slots[idx];
    sum += slot.price_ticks;
    idx = slot.next_index;
    benchmark::DoNotOptimize(sum);
  }

  munmap(ob.raw_map, ob.raw_bytes);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_32MiB_4KiBPages)->UseRealTime();

/**
 * @brief Benchmarks the exact same `32 MiB` L3-resident order book backed by
 *        `16` `2 MiB` HugePages (`MADV_HUGEPAGE`), fitting in just 16 L1/L2
 *        dTLB slots.
 * @param state Google Benchmark state.
 */
void BM_OrderBook_32MiB_2MiBHugePages(benchmark::State& state) {
  PinThreadToCore(0);
  MappedOrderBook ob = AllocateOrderBookArena(kOrderBook32MiBBytes, true);
  std::size_t idx = ob.start_index;
  std::int64_t sum = 0;

  for (auto _ : state) {
    const OrderSlot& slot = ob.slots[idx];
    sum += slot.price_ticks;
    idx = slot.next_index;
    benchmark::DoNotOptimize(sum);
  }

  munmap(ob.raw_map, ob.raw_bytes);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_32MiB_2MiBHugePages)->UseRealTime();

/**
 * @brief Benchmarks random lookups across a `512 MiB` order book mapped with
 *        `131,072` standard `4 KiB` pages (`MADV_NOHUGEPAGE`), where the
 *        `3,072`-entry L2 dTLB covers only `12 MiB` (`2.34%`) of the table.
 * @param state Google Benchmark state.
 */
void BM_OrderBook_512MiB_4KiBPages(benchmark::State& state) {
  PinThreadToCore(0);
  MappedOrderBook ob = AllocateOrderBookArena(kOrderBook512MiBBytes, false);
  std::size_t idx = ob.start_index;
  std::int64_t sum = 0;

  for (auto _ : state) {
    const OrderSlot& slot = ob.slots[idx];
    sum += slot.price_ticks;
    idx = slot.next_index;
    benchmark::DoNotOptimize(sum);
  }

  munmap(ob.raw_map, ob.raw_bytes);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_512MiB_4KiBPages)->UseRealTime();

/**
 * @brief Benchmarks random lookups across the exact same `512 MiB` order book
 *        advised with `madvise(..., MADV_HUGEPAGE)` (`256` `2 MiB` HugePages),
 *        allowing the entire `512 MiB` arena to fit inside `256` L2 dTLB slots!
 * @param state Google Benchmark state.
 */
void BM_OrderBook_512MiB_2MiBHugePages(benchmark::State& state) {
  PinThreadToCore(0);
  MappedOrderBook ob = AllocateOrderBookArena(kOrderBook512MiBBytes, true);
  std::size_t idx = ob.start_index;
  std::int64_t sum = 0;

  for (auto _ : state) {
    const OrderSlot& slot = ob.slots[idx];
    sum += slot.price_ticks;
    idx = slot.next_index;
    benchmark::DoNotOptimize(sum);
  }

  munmap(ob.raw_map, ob.raw_bytes);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_512MiB_2MiBHugePages)->UseRealTime();

}  // namespace
}  // namespace hotpath::episode11
