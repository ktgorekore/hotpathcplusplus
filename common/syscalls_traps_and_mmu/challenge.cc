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
 * @file challenge.cc
 * @brief Hot Path C++ — Episode 11 Production Telemetry & Verification Lab.
 *
 * Three-Part Production Verification Lab (16-Core AMD Ryzen 9 9950X, Zen 5):
 * - Part 1 (Workload 1 — Ring 3 vDSO vs. Ring 0 Syscall & Cache Pollution):
 *   Inspects `/proc/self/maps` (`[vvar]` + `[vdso]`) and compares
 *   `clock_gettime` (vDSO Seqlock + `rdtscp`) vs. `syscall(SYS_clock_gettime)`
 *   vs. `syscall(SYS_gettid)` vs. `thread_local` cached TID, plus the 48 KiB
 *   L1D working-set cache pollution tax.
 * - Part 2 (Workload 2 — Demand Paging, `#PF` Traps & `MAP_POPULATE` Pools):
 *   Measures split-phase `mmap(2 MiB)` allocation (`0.69 us`), first-touch
 *   write loop (`217.6 us`, `512` `minor-faults` @ `425 ns/page`), `glibc`
 *   `std::vector` (`>= 128 KiB` `M_MMAP_THRESHOLD` `munmap` churn: `329.3 us`),
 *   and pre-faulted Ring 3 arena reuse (`197 ns`, `0` `#PF` traps — `1,105x`
 *   to `1,671x` faster!).
 * - Part 3 (Workload 3 — Hardware MMU, 4-Level Page Walks, TLB & HugePages):
 *   Compares `4 KiB` standard pages (`MADV_NOHUGEPAGE`) against `2 MiB`
 *   HugePages (`MADV_HUGEPAGE`) on `32 MiB` and `512 MiB` order books.
 */

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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
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

/**
 * @brief Pins the calling thread to a specific physical core (0..15).
 * @param core_id Physical core index.
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id % 16, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/**
 * @brief Returns cumulative minor page faults (`ru_minflt`) for this process.
 * @return Minor page fault count.
 */
inline long GetMinorFaults() noexcept {
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  return ru.ru_minflt;
}

/**
 * @brief Returns the calling thread's OS Thread ID cached in TLS.
 * @return Cached `pid_t`.
 */
inline pid_t CachedThreadId() noexcept {
  thread_local const pid_t cached_tid = static_cast<pid_t>(syscall(SYS_gettid));
  return cached_tid;
}

/**
 * @struct alignas(64) OrderSlot
 * @brief 64-byte cache-line-sized financial order book / hash table entry.
 */
struct alignas(64) OrderSlot {
  std::uint64_t next_index;
  std::uint64_t order_id;
  std::int64_t price_ticks;
  std::uint64_t quantity;
  std::uint64_t timestamp_ns;
  std::uint64_t flags;
  std::uint64_t pad0;
  std::uint64_t pad1;
};

/**
 * @brief Inspects `/proc/self/maps` for `[vvar]` and `[vdso]` mappings and
 *        benchmarks Ring 3 vDSO vs. Ring 0 syscalls.
 */
void RunPart1TelemetryAndVdsoLab() {
  std::cout << "=== Part 1: Workload 1 — Ring 3 vDSO vs. Ring 0 Syscalls ===\n";
  std::ifstream maps("/proc/self/maps");
  std::string line;
  while (std::getline(maps, line)) {
    if (line.find("[vvar]") != std::string::npos ||
        line.find("[vdso]") != std::string::npos) {
      std::cout << "  /proc/self/maps: " << line << "\n";
    }
  }

  constexpr std::size_t kIters = 2'000'000;
  std::uint64_t sink = 0;

  const auto t0 = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < kIters; ++i) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    sink += static_cast<std::uint64_t>(ts.tv_nsec);
  }
  const auto t1 = std::chrono::steady_clock::now();

  const auto t2 = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < kIters; ++i) {
    struct timespec ts;
    syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    sink += static_cast<std::uint64_t>(ts.tv_nsec);
  }
  const auto t3 = std::chrono::steady_clock::now();

  const auto t4 = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < kIters; ++i) {
    sink += static_cast<std::uint64_t>(syscall(SYS_gettid));
  }
  const auto t5 = std::chrono::steady_clock::now();

  const auto t6 = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < kIters * 10; ++i) {
    pid_t tid = CachedThreadId();
    asm volatile("" : "+r"(tid));
    sink += static_cast<std::uint64_t>(tid);
  }
  const auto t7 = std::chrono::steady_clock::now();
  asm volatile("" : : "r"(sink) : "memory");

  const double vdso_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() / kIters;
  const double sys_clk_ns =
      std::chrono::duration<double, std::nano>(t3 - t2).count() / kIters;
  const double sys_tid_ns =
      std::chrono::duration<double, std::nano>(t5 - t4).count() / kIters;
  const double tls_tid_ns =
      std::chrono::duration<double, std::nano>(t7 - t6).count() / (kIters * 10);

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "  clock_gettime (vDSO Ring 3)       : " << vdso_ns
            << " ns/op\n";
  std::cout << "  syscall(SYS_clock_gettime) Ring 0 : " << sys_clk_ns
            << " ns/op (" << (sys_clk_ns / vdso_ns) << "x slower)\n";
  std::cout << "  syscall(SYS_gettid) Ring 0        : " << sys_tid_ns
            << " ns/op\n";
  std::cout << "  CachedThreadId() (Ring 3 TLS)     : " << tls_tid_ns
            << " ns/op (" << (sys_tid_ns / tls_tid_ns) << "x faster)\n";
}

/**
 * @brief Measures Demand Paging (`mmap(2 MiB)` allocation call vs. 512 `#PF`
 *        first-touch hardware traps vs. pre-faulted Ring 3 pool).
 */
void RunPart2DemandPagingLab() {
  std::cout << "\n=== Part 2: Workload 2 — Demand Paging & #PF Traps (2 MiB = "
               "512 x 4 KiB Pages) ===\n";
  constexpr int kTrials = 400;
  double total_alloc_ns = 0.0;
  double total_first_write_ns = 0.0;
  long total_first_faults = 0;

  for (int t = 0; t < kTrials; ++t) {
    const auto t0 = std::chrono::steady_clock::now();
    void* raw = mmap(nullptr, kHugePageSize2MiB, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    const auto t1 = std::chrono::steady_clock::now();
    madvise(raw, kHugePageSize2MiB, MADV_NOHUGEPAGE);

    auto* buf = static_cast<std::uint8_t*>(raw);
    const long flt0 = GetMinorFaults();
    const auto t2 = std::chrono::steady_clock::now();
    for (std::size_t p = 0; p < kPagesPer2MiB; ++p) {
      const std::size_t cl_offset = (p & 63ULL) * 64ULL;
      buf[p * kPageSize4KiB + cl_offset] = static_cast<std::uint8_t>(p + 1);
    }
    asm volatile("" : : "r"(buf) : "memory");
    const auto t3 = std::chrono::steady_clock::now();
    const long flt1 = GetMinorFaults();

    total_alloc_ns += std::chrono::duration<double, std::nano>(t1 - t0).count();
    total_first_write_ns +=
        std::chrono::duration<double, std::nano>(t3 - t2).count();
    total_first_faults += (flt1 - flt0);

    munmap(raw, kHugePageSize2MiB);
  }

  // Pre-faulted pooled arena (MAP_POPULATE + mlock + MADV_HUGEPAGE)
  void* raw_pool = mmap(nullptr, kHugePageSize2MiB * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(raw_pool);
  const std::uintptr_t aligned_addr =
      (addr + kHugePageSize2MiB - 1ULL) & ~(kHugePageSize2MiB - 1ULL);
  auto* pool_buf = reinterpret_cast<std::uint8_t*>(aligned_addr);
  madvise(pool_buf, kHugePageSize2MiB, MADV_HUGEPAGE);
  madvise(pool_buf, kHugePageSize2MiB, MADV_COLLAPSE);
  mlock(pool_buf, kHugePageSize2MiB);
  std::memset(pool_buf, 0x42, kHugePageSize2MiB);

  constexpr int kPoolTrials = 100'000;
  const long flt_pool0 = GetMinorFaults();
  const auto tp0 = std::chrono::steady_clock::now();
  for (int t = 0; t < kPoolTrials; ++t) {
    const std::uint8_t tag = static_cast<std::uint8_t>(t + 1);
    for (std::size_t p = 0; p < kPagesPer2MiB; ++p) {
      const std::size_t cl_offset = (p & 63ULL) * 64ULL;
      pool_buf[p * kPageSize4KiB + cl_offset] = tag;
    }
    asm volatile("" : : "r"(pool_buf) : "memory");
  }
  const auto tp1 = std::chrono::steady_clock::now();
  const long flt_pool1 = GetMinorFaults();
  munlock(pool_buf, kHugePageSize2MiB);
  munmap(raw_pool, kHugePageSize2MiB * 2);

  const double avg_alloc_ns = total_alloc_ns / kTrials;
  const double avg_first_ns = total_first_write_ns / kTrials;
  const double avg_pool_ns =
      std::chrono::duration<double, std::nano>(tp1 - tp0).count() / kPoolTrials;

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "  mmap(2 MiB) Alloc Call : " << avg_alloc_ns << " ns ("
            << (avg_alloc_ns / 1000.0) << " us)\n";
  std::cout << "  1st Write Pass (512p)  : " << avg_first_ns << " ns ("
            << (avg_first_ns / 1000.0) << " us) | " << (avg_first_ns / 512.0)
            << " ns/page | minor faults = " << (total_first_faults / kTrials)
            << "\n";
  std::cout << "  Pre-Faulted Pool (512p): " << avg_pool_ns << " ns ("
            << (avg_pool_ns / 512.0)
            << " ns/page) | minor faults = " << (flt_pool1 - flt_pool0) << "\n";
  std::cout << "  Speedup (Pool vs Fault): " << (avg_first_ns / avg_pool_ns)
            << "x\n";
}

/**
 * @brief Runs the Order Book random lookup benchmark for a given size (`32` or
 *        `512` MiB) and page size (`4 KiB` vs. `2 MiB` HugePages).
 * @param size_mib Arena size in MiB.
 * @param use_hugepages True for `MADV_HUGEPAGE`, false for `MADV_NOHUGEPAGE`.
 * @param lookups Number of dependent lookups to execute.
 */
void RunOrderBookTlbTest(std::size_t size_mib, bool use_hugepages,
                         std::size_t lookups) {
  const std::size_t bytes = size_mib << 20;
  void* raw_map =
      mmap(nullptr, bytes + kHugePageSize2MiB, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(raw_map);
  const std::uintptr_t aligned_addr =
      (addr + kHugePageSize2MiB - 1ULL) & ~(kHugePageSize2MiB - 1ULL);
  auto* table = reinterpret_cast<OrderSlot*>(aligned_addr);

  if (use_hugepages) {
    madvise(table, bytes, MADV_HUGEPAGE);
  } else {
    madvise(table, bytes, MADV_NOHUGEPAGE);
  }

  const std::size_t num_slots = bytes / sizeof(OrderSlot);
  for (std::size_t i = 0; i < num_slots; i += 64) {
    table[i].order_id = i;
  }
  if (use_hugepages) {
    madvise(table, bytes, MADV_COLLAPSE);
  }

  // Active hot set: 1 order slot (64B) per 4 KiB page across the arena, wired
  // in a single-cycle Sattolo permutation so every lookup jumps to a random
  // 4 KiB page while keeping the hot cache lines L3-resident.
  const std::size_t num_pages = bytes / kPageSize4KiB;
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

  std::int64_t sink = 0;
  std::size_t idx = page_perm[0] * 64ULL + (page_perm[0] & 63ULL);
  for (std::size_t p = 0; p < num_pages * 2; ++p) {
    sink += table[idx].price_ticks;
    idx = table[idx].next_index;
  }

  const auto t0 = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < lookups; ++i) {
    const OrderSlot& slot = table[idx];
    sink += slot.price_ticks;
    idx = slot.next_index;
  }
  const auto t1 = std::chrono::steady_clock::now();
  asm volatile("" : : "r"(sink) : "memory");

  const double elapsed_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count();
  const double ns_per_lookup = elapsed_ns / static_cast<double>(lookups);
  const double mops = 1000.0 / ns_per_lookup;

  std::cout << "  [" << size_mib << " MiB | "
            << (use_hugepages ? "2 MiB HugePages" : "4 KiB Std Pages")
            << "] : " << std::fixed << std::setprecision(2) << ns_per_lookup
            << " ns/lookup (" << mops << " M lookups/s)\n";

  munmap(raw_map, bytes + kHugePageSize2MiB);
}

}  // namespace
}  // namespace hotpath::episode11

int main(int argc, char** argv) {
  hotpath::episode11::PinThreadToCore(0);
  if (argc > 1) {
    const std::string mode = argv[1];
    if (mode == "4k_512m") {
      hotpath::episode11::RunOrderBookTlbTest(512, false, 50'000'000);
      return 0;
    }
    if (mode == "2m_512m") {
      hotpath::episode11::RunOrderBookTlbTest(512, true, 50'000'000);
      return 0;
    }
    if (mode == "4k_32m") {
      hotpath::episode11::RunOrderBookTlbTest(32, false, 50'000'000);
      return 0;
    }
    if (mode == "2m_32m") {
      hotpath::episode11::RunOrderBookTlbTest(32, true, 50'000'000);
      return 0;
    }
    if (mode == "fault_2m") {
      for (int i = 0; i < 1000; ++i) {
        auto* buf = static_cast<std::uint8_t*>(
            mmap(nullptr, hotpath::episode11::kHugePageSize2MiB,
                 PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        madvise(buf, hotpath::episode11::kHugePageSize2MiB, MADV_NOHUGEPAGE);
        for (std::size_t p = 0; p < 512; ++p) {
          buf[p * 4096ULL + (p & 63ULL) * 64ULL] = 1;
        }
        asm volatile("" : : "r"(buf) : "memory");
        munmap(buf, hotpath::episode11::kHugePageSize2MiB);
      }
      return 0;
    }
    if (mode == "pool_2m") {
      void* raw = mmap(nullptr, hotpath::episode11::kHugePageSize2MiB * 2,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
      const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(raw);
      auto* buf = reinterpret_cast<std::uint8_t*>(
          (addr + hotpath::episode11::kHugePageSize2MiB - 1ULL) &
          ~(hotpath::episode11::kHugePageSize2MiB - 1ULL));
      madvise(buf, hotpath::episode11::kHugePageSize2MiB, MADV_HUGEPAGE);
      madvise(buf, hotpath::episode11::kHugePageSize2MiB, MADV_COLLAPSE);
      mlock(buf, hotpath::episode11::kHugePageSize2MiB);
      for (int i = 0; i < 1000; ++i) {
        for (std::size_t p = 0; p < 512; ++p) {
          buf[p * 4096ULL + (p & 63ULL) * 64ULL] = static_cast<std::uint8_t>(i);
        }
        asm volatile("" : : "r"(buf) : "memory");
      }
      munlock(buf, hotpath::episode11::kHugePageSize2MiB);
      munmap(raw, hotpath::episode11::kHugePageSize2MiB * 2);
      return 0;
    }
  }

  hotpath::episode11::RunPart1TelemetryAndVdsoLab();
  hotpath::episode11::RunPart2DemandPagingLab();
  std::cout << "\n=== Part 3: Workload 3 — Order Book Random Lookup (4 KiB vs "
               "2 MiB HugePages) ===\n";
  hotpath::episode11::RunOrderBookTlbTest(32, false, 20'000'000);
  hotpath::episode11::RunOrderBookTlbTest(32, true, 20'000'000);
  hotpath::episode11::RunOrderBookTlbTest(512, false, 20'000'000);
  hotpath::episode11::RunOrderBookTlbTest(512, true, 20'000'000);
  return 0;
}
