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
// Hot Path C++ — Episode 11 Interactive Forensic Lab & Self-Verifying Challenge
// File: episodes/11_syscalls_traps_and_mmu/challenge.cc
// ============================================================================

#include <malloc.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace hotpath::episode11 {
namespace {

using SteadyClock = std::chrono::steady_clock;

/**
 * @brief Prevents the compiler from optimizing away a benchmark value or loop.
 *
 * @tparam T Value type.
 * @param value Value to preserve across optimization passes.
 */
template <typename T>
inline void DoNotOptimize(T const& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

/**
 * @brief Pins the calling thread to CPU Core 0 for deterministic cache and TLB
 * residency across runs.
 *
 * @param core_id Logical CPU core index.
 */
inline void PinThreadToCore(int core_id = 0) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  ::sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

/**
 * @brief Returns the calling thread's cumulative minor page fault count
 * (`ru_minflt`) via `getrusage(RUSAGE_THREAD)`.
 *
 * @return Thread minor page fault count.
 */
inline long GetMinorFaults() noexcept {
  struct rusage u {};
  ::getrusage(RUSAGE_THREAD, &u);
  return u.ru_minflt;
}

/**
 * @brief Returns the calling thread's TID cached in Ring 3 `thread_local`
 * storage (`%fs` segment offset).
 *
 * @return Thread ID (`pid_t`).
 */
inline pid_t CachedTid() noexcept {
  thread_local const pid_t kTid = static_cast<pid_t>(::syscall(SYS_gettid));
  return kTid;
}

/**
 * @brief Reads the first line of a Linux `/sys` or `/proc` virtual file.
 *
 * @param path Null-terminated filesystem path.
 * @return First line string, or `"unknown"` if unreadable.
 */
std::string ReadSysfsFirstLine(const char* path) {
  std::ifstream f(path);
  std::string line = "unknown";
  if (f.is_open()) {
    std::getline(f, line);
  }
  return line;
}

/**
 * @brief 64-byte cache-line-aligned order book lookup node.
 */
struct alignas(64) LookupNode {
  uint64_t key;
  int64_t value;
  uint8_t pad[48];
};
static_assert(sizeof(LookupNode) == 64, "LookupNode must be 64 bytes");

/**
 * @brief Runs an isolated TLB random lookup test over `table_bytes` using
 * either 4 KiB pages (`MADV_NOHUGEPAGE`) or 2 MiB HugePages (`MADV_HUGEPAGE` +
 * `MADV_COLLAPSE`).
 *
 * @param table_bytes Arena footprint in bytes (e.g. 32 MiB or 512 MiB).
 * @param num_lookups Total number of random probes to execute.
 * @param hugepages True for 2 MiB HugePages, false for 4 KiB standard pages.
 * @return Mean latency per lookup in nanoseconds.
 */
double RunTlbLookupTest(size_t table_bytes, size_t num_lookups,
                        bool hugepages) {
  const size_t raw_len = table_bytes + (2ULL << 20);
  void* raw = ::mmap(nullptr, raw_len, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  const uintptr_t raw_a = reinterpret_cast<uintptr_t>(raw);
  const uintptr_t aligned_a = (raw_a + (2ULL << 20) - 1) & ~((2ULL << 20) - 1);
  auto* table = reinterpret_cast<LookupNode*>(aligned_a);

  if (hugepages) {
    ::madvise(table, table_bytes, MADV_HUGEPAGE);
  } else {
    ::madvise(table, table_bytes, MADV_NOHUGEPAGE);
  }

  const size_t num_nodes = table_bytes / sizeof(LookupNode);
  for (size_t i = 0; i < num_nodes; ++i) {
    table[i].key = i;
    table[i].value = static_cast<int64_t>(i * 3 + 1);
  }
#ifdef MADV_COLLAPSE
  if (hugepages) {
    ::madvise(table, table_bytes, MADV_COLLAPSE);
  }
#endif

  constexpr size_t kRing = 1 << 20;
  std::vector<uint32_t> probes(kRing);
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<size_t> dist(0, num_nodes - 1);
  for (size_t i = 0; i < kRing; ++i) {
    probes[i] = static_cast<uint32_t>(dist(rng));
  }

  int64_t sum = 0;
  const auto start = SteadyClock::now();
  for (size_t i = 0; i < num_lookups; ++i) {
    const uint32_t idx =
        probes[(i + static_cast<size_t>(sum & 0xFF)) & (kRing - 1)];
    sum += table[idx].value;
    DoNotOptimize(sum);
  }
  const auto end = SteadyClock::now();
  ::munmap(raw, raw_len);
  return std::chrono::duration<double, std::nano>(end - start).count() /
         static_cast<double>(num_lookups);
}

/**
 * @brief Executes an isolated workload mode for focused `perf stat` PMU
 * profiling when a CLI subcommand is passed.
 *
 * @param mode Subcommand string (`fault_2m`, `pool_2m`, `4k_32m`, `2m_32m`,
 * `4k_512m`, or `2m_512m`).
 * @return Process exit code.
 */
int RunIsolatedPmuMode(const std::string& mode) {
  PinThreadToCore(0);
  ::mallopt(M_MMAP_THRESHOLD, 128 * 1024);
  constexpr size_t kBytes2MiB = 2ULL << 20;
  constexpr size_t kPage4K = 4096;

  if (mode == "fault_2m") {
    const long f0 = GetMinorFaults();
    for (int iter = 0; iter < 500; ++iter) {
      auto* buf = static_cast<uint8_t*>(
          ::mmap(nullptr, kBytes2MiB, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
      ::madvise(buf, kBytes2MiB, MADV_NOHUGEPAGE);
      for (size_t off = 0; off < kBytes2MiB; off += kPage4K) {
        buf[off] = 0x42;
      }
      DoNotOptimize(buf);
      ::munmap(buf, kBytes2MiB);
    }
    const long f1 = GetMinorFaults();
    std::printf(
        "[fault_2m] 500 x 2 MiB fault passes complete: %ld minor faults "
        "(%.1f faults/iter)\n",
        f1 - f0, static_cast<double>(f1 - f0) / 500.0);
    return 0;
  }
  if (mode == "pool_2m") {
    auto* buf = static_cast<uint8_t*>(
        ::mmap(nullptr, kBytes2MiB, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
    ::mlock(buf, kBytes2MiB);
    std::memset(buf, 1, kBytes2MiB);
    const long f0 = GetMinorFaults();
    for (int iter = 0; iter < 500; ++iter) {
      for (size_t off = 0; off < kBytes2MiB; off += kPage4K) {
        buf[off] = 0x99;
      }
      DoNotOptimize(buf);
    }
    const long f1 = GetMinorFaults();
    std::printf(
        "[pool_2m] 500 x 2 MiB pre-faulted passes complete: %ld minor "
        "faults\n",
        f1 - f0);
    ::munlock(buf, kBytes2MiB);
    ::munmap(buf, kBytes2MiB);
    return 0;
  }
  if (mode == "4k_32m") {
    const double ns = RunTlbLookupTest(32ULL << 20, 16000000, false);
    std::printf("[4k_32m] 16M lookups (32 MiB, 4 KiB pages): %.2f ns/lookup\n",
                ns);
    return 0;
  }
  if (mode == "2m_32m") {
    const double ns = RunTlbLookupTest(32ULL << 20, 16000000, true);
    std::printf(
        "[2m_32m] 16M lookups (32 MiB, 2 MiB HugePages): %.2f "
        "ns/lookup\n",
        ns);
    return 0;
  }
  if (mode == "4k_512m") {
    const double ns = RunTlbLookupTest(512ULL << 20, 16000000, false);
    std::printf(
        "[4k_512m] 16M lookups (512 MiB, 4 KiB pages): %.2f ns/lookup\n", ns);
    return 0;
  }
  if (mode == "2m_512m") {
    const double ns = RunTlbLookupTest(512ULL << 20, 16000000, true);
    std::printf(
        "[2m_512m] 16M lookups (512 MiB, 2 MiB HugePages): %.2f ns/lookup\n",
        ns);
    return 0;
  }
  std::fprintf(stderr,
               "Unknown mode '%s'. Valid modes: fault_2m, pool_2m, 4k_32m, "
               "2m_32m, 4k_512m, 2m_512m\n",
               mode.c_str());
  return 1;
}

}  // namespace
}  // namespace hotpath::episode11

int main(int argc, char** argv) {
  using namespace hotpath::episode11;

  if (argc > 1) {
    return RunIsolatedPmuMode(argv[1]);
  }

  PinThreadToCore(0);
  ::mallopt(M_MMAP_THRESHOLD, 128 * 1024);

  std::printf(
      "========================================================================"
      "========\n");
  std::printf(" HOT PATH C++ • EPISODE 11 FORENSIC TELEMETRY RUNNER\n");
  std::printf(
      "========================================================================"
      "========\n");

  const std::string clocksource = ReadSysfsFirstLine(
      "/sys/devices/system/clocksource/clocksource0/current_clocksource");
  const std::string thp_mode =
      ReadSysfsFirstLine("/sys/kernel/mm/transparent_hugepage/enabled");
  std::printf(" [System Info] Active Clocksource : %s\n", clocksource.c_str());
  std::printf(" [System Info] THP Mode           : %s\n\n", thp_mode.c_str());

  // ==========================================================================
  // CASE 1: TELEMETRY, VDSO SEQLOCK VS. SYSCALLS & L1D CACHE EVICTION TAX
  // ==========================================================================
  constexpr size_t kItersCase1 = 2000000;

  auto t0 = SteadyClock::now();
  for (size_t i = 0; i < kItersCase1; ++i) {
    struct timespec ts {};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    DoNotOptimize(ts.tv_nsec);
  }
  auto t1 = SteadyClock::now();
  const double vdso_clock_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() / kItersCase1;

  t0 = SteadyClock::now();
  for (size_t i = 0; i < kItersCase1; ++i) {
    struct timespec ts {};
    ::syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    DoNotOptimize(ts.tv_nsec);
  }
  t1 = SteadyClock::now();
  const double raw_clock_syscall_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() / kItersCase1;

  t0 = SteadyClock::now();
  for (size_t i = 0; i < kItersCase1; ++i) {
    pid_t tid = static_cast<pid_t>(::syscall(SYS_gettid));
    DoNotOptimize(tid);
  }
  t1 = SteadyClock::now();
  const double gettid_tight_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() / kItersCase1;

  t0 = SteadyClock::now();
  for (size_t i = 0; i < kItersCase1 * 10; ++i) {
    pid_t tid = CachedTid();
    DoNotOptimize(tid);
  }
  t1 = SteadyClock::now();
  const double gettid_tls_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() /
      (kItersCase1 * 10);

  constexpr size_t kItersL1Pollute = 400000;
  alignas(64) std::vector<uint64_t> ws(48 * 1024 / sizeof(uint64_t), 1ULL);
  uint64_t acc = 0;

  t0 = SteadyClock::now();
  for (size_t iter = 0; iter < kItersL1Pollute; ++iter) {
    for (size_t i = 0; i < ws.size(); i += 8) acc += ws[i]++;
    acc += static_cast<uint64_t>(CachedTid());
    DoNotOptimize(acc);
  }
  t1 = SteadyClock::now();
  const double ws_with_tls_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() /
      kItersL1Pollute;

  t0 = SteadyClock::now();
  for (size_t iter = 0; iter < kItersL1Pollute; ++iter) {
    for (size_t i = 0; i < ws.size(); i += 8) acc += ws[i]++;
    acc += static_cast<uint64_t>(::syscall(SYS_gettid));
    DoNotOptimize(acc);
  }
  t1 = SteadyClock::now();
  const double ws_with_syscall_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() /
      kItersL1Pollute;
  const double effective_interleaved_syscall_ns =
      ws_with_syscall_ns - ws_with_tls_ns;

  std::printf(
      "--- CASE FILE 1: TELEMETRY, VDSO SEQLOCK & L1D CACHE EVICTION ---\n");
  std::printf("  steady_clock::now() (vDSO Ring 3)       : %7.2f ns\n",
              vdso_clock_ns);
  std::printf(
      "  syscall(SYS_clock_gettime) (Ring 0)     : %7.2f ns (%.2fx slower)\n",
      raw_clock_syscall_ns, raw_clock_syscall_ns / vdso_clock_ns);
  std::printf("  syscall(SYS_gettid) (Tight Loop)        : %7.2f ns\n",
              gettid_tight_ns);
  std::printf(
      "  syscall(SYS_gettid) (In 48 KiB App Loop): %7.2f ns (%.2fx worse due "
      "to L1/BTB eviction!)\n",
      effective_interleaved_syscall_ns,
      effective_interleaved_syscall_ns / gettid_tight_ns);
  std::printf(
      "  thread_local CachedTid() (Ring 3 TLS)   : %7.2f ns (%.0fx faster than "
      "tight loop!)\n\n",
      gettid_tls_ns, gettid_tight_ns / gettid_tls_ns);

  // ==========================================================================
  // CASE 2: DEMAND PAGING, 512 #PF TRAPS & GLIBC 128 KiB VECTOR CHURN
  // ==========================================================================
  constexpr size_t kBytes2MiB = 2ULL << 20;
  constexpr size_t kPage4K = 4096;
  constexpr size_t kItersCase2 = 1000;

  double sum_alloc_ns = 0.0;
  double sum_first_write_ns = 0.0;
  double sum_second_write_ns = 0.0;
  long total_first_write_faults = 0;

  for (size_t iter = 0; iter < kItersCase2; ++iter) {
    const auto a0 = SteadyClock::now();
    auto* buf = static_cast<uint8_t*>(
        ::mmap(nullptr, kBytes2MiB, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    DoNotOptimize(buf);
    const auto a1 = SteadyClock::now();
    ::madvise(buf, kBytes2MiB, MADV_NOHUGEPAGE);

    const long f0 = GetMinorFaults();
    const auto w0 = SteadyClock::now();
    for (size_t off = 0; off < kBytes2MiB; off += kPage4K) {
      buf[off] = 0x42;
    }
    DoNotOptimize(buf);
    asm volatile("" ::: "memory");
    const auto w1 = SteadyClock::now();
    const long f1 = GetMinorFaults();

    const auto s0 = SteadyClock::now();
    for (size_t off = 0; off < kBytes2MiB; off += kPage4K) {
      buf[off] = 0x99;
    }
    DoNotOptimize(buf);
    asm volatile("" ::: "memory");
    const auto s1 = SteadyClock::now();

    sum_alloc_ns += std::chrono::duration<double, std::nano>(a1 - a0).count();
    sum_first_write_ns +=
        std::chrono::duration<double, std::nano>(w1 - w0).count();
    sum_second_write_ns +=
        std::chrono::duration<double, std::nano>(s1 - s0).count();
    total_first_write_faults += (f1 - f0);

    ::munmap(buf, kBytes2MiB);
  }

  const double alloc_us = (sum_alloc_ns / kItersCase2) / 1000.0;
  const double first_write_us = (sum_first_write_ns / kItersCase2) / 1000.0;
  const double per_page_fault_ns = (sum_first_write_ns / kItersCase2) / 512.0;
  const double second_write_ns = (sum_second_write_ns / kItersCase2);

  // Warm up 64 KiB heap bin once so we measure steady-state Ring 3 reuse.
  {
    std::vector<uint8_t> warmup64k(64 * 1024, 1);
    DoNotOptimize(warmup64k.data());
  }
  const long vec64k_f0 = GetMinorFaults();
  for (size_t iter = 0; iter < 500; ++iter) {
    std::vector<uint8_t> v64k(64 * 1024, 1);
    DoNotOptimize(v64k.data());
  }
  const long vec64k_faults = (GetMinorFaults() - vec64k_f0) / 500;

  const long vec2m_f0 = GetMinorFaults();
  for (size_t iter = 0; iter < 500; ++iter) {
    std::vector<uint8_t> v2m(kBytes2MiB, 1);
    DoNotOptimize(v2m.data());
  }
  const long vec2m_faults = (GetMinorFaults() - vec2m_f0) / 500;

  std::printf(
      "--- CASE FILE 2: DEMAND PAGING, #PF TRAPS & 128 KiB VECTOR CLIFF ---\n");
  std::printf(
      "  mmap(2 MiB) Virtual Allocation Only     : %7.2f us (0 Physical RAM)\n",
      alloc_us);
  std::printf(
      "  1st Write Pass (512 x 4 KiB Pages)      : %7.2f us (%ld #PFs/call, "
      "%.1f ns/page!)\n",
      first_write_us, total_first_write_faults / kItersCase2,
      per_page_fault_ns);
  std::printf(
      "  2nd Write Pass / MAP_POPULATE Pool      : %7.1f ns (%.1fx faster!)\n",
      second_write_ns, (first_write_us * 1000.0) / second_write_ns);
  std::printf(
      "  Temp std::vector(64 KiB < 128 KiB)      : %4ld minor faults/call "
      "(Recycled in Ring 3)\n",
      vec64k_faults);
  std::printf(
      "  Temp std::vector(2 MiB >= 128 KiB)      : %4ld minor faults/call "
      "(munmap() churn trap!)\n\n",
      vec2m_faults);

  // ==========================================================================
  // CASE 3: MMU, 12 MiB L2 TLB REACH VS. 2 MiB HUGEPAGES (512 MiB TABLE)
  // ==========================================================================
  constexpr size_t kTableBytes = 512ULL << 20;
  constexpr size_t kNumLookups = 8000000;

  const double lookup_4k_ns =
      RunTlbLookupTest(kTableBytes, kNumLookups, /*hugepages=*/false);
  const double lookup_2m_ns =
      RunTlbLookupTest(kTableBytes, kNumLookups, /*hugepages=*/true);

  std::printf(
      "--- CASE FILE 3: 512 MiB ORDER BOOK (4 KiB PAGES VS. 2 MiB HUGEPAGES) "
      "---\n");
  std::printf("  Standard 4 KiB Pages (131,072 Pages)    : %7.2f ns / lookup\n",
              lookup_4k_ns);
  std::printf(
      "  2 MiB HugePages (256 Pages, MADV_HUGE)  : %7.2f ns / lookup (%.2fx "
      "faster!)\n",
      lookup_2m_ns, lookup_4k_ns / lookup_2m_ns);
  std::printf(
      "========================================================================"
      "========\n");
  return 0;
}
