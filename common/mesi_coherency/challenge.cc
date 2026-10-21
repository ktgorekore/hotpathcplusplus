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
 * @brief Hot Path C++ — Episode 09 Hands-On Silicon Switchboard Challenge.
 */

#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "common/mesi_coherency/mesi_coherency.h"

namespace hotpath::mesi_coherency {
namespace {

constexpr std::size_t kSplitLockOps = 500'000;
constexpr std::size_t kCasThreads = 16;
constexpr std::size_t kCasOpsPerThread = 25'000;

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
 * @struct SplitLockMetrics
 * @brief Timing and latency metrics for Part 1 Split-Lock verification.
 */
struct SplitLockMetrics {
  std::uint64_t final_seq = 0;
  double duration_ms = 0.0;
  double ns_per_op = 0.0;
};

/**
 * @brief Executes `kSplitLockOps` standard C++20 `fetch_add` increments on a
 *        `std::atomic<std::uint64_t>` counter.
 * @param counter Reference to the target atomic 64-bit integer.
 * @return SplitLockMetrics containing elapsed time and per-op latency.
 */
SplitLockMetrics RunAtomicIncrementLoop(std::atomic<std::uint64_t>& counter) {
  PinThreadToCore(0);
  counter.store(0, std::memory_order_relaxed);

  const auto start = std::chrono::high_resolution_clock::now();
  for (std::size_t i = 0; i < kSplitLockOps; ++i) {
    counter.fetch_add(1, std::memory_order_relaxed);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double, std::milli> elapsed_ms = end - start;
  const double ns_per_op =
      (elapsed_ms.count() * 1e6) / static_cast<double>(kSplitLockOps);

  return {counter.load(std::memory_order_relaxed), elapsed_ms.count(),
          ns_per_op};
}

/**
 * @struct CasStormMetrics
 * @brief Timing and throughput metrics for Part 2 16-Core CAS contention test.
 */
struct CasStormMetrics {
  std::uint64_t total_claimed = 0;
  double duration_ms = 0.0;
  double ns_per_op = 0.0;
};

/**
 * @brief Runs a 16-core contended spinlock loop using `LockType`.
 * @tparam LockType Either `NaiveCasSpinlock` or `ReadBeforeCasSpinlock`.
 * @return CasStormMetrics containing total updates and per-update latency.
 */
template <typename LockType>
CasStormMetrics RunContendedCasChallenge() {
  LockType spinlock;
  SharedOrderBookLedger ledger{};

  std::barrier sync_barrier(kCasThreads);
  std::vector<std::thread> workers;
  workers.reserve(kCasThreads);

  const auto start = std::chrono::high_resolution_clock::now();

  for (std::size_t tid = 0; tid < kCasThreads; ++tid) {
    workers.emplace_back([&, tid]() {
      PinThreadToCore(static_cast<int>(tid % 16));
      sync_barrier.arrive_and_wait();

      for (std::size_t i = 0; i < kCasOpsPerThread; ++i) {
        spinlock.Lock();
        ledger.RecordExecution(10, 100);
        spinlock.Unlock();
      }
    });
  }

  for (auto& w : workers) {
    w.join();
  }

  const auto end = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double, std::milli> elapsed_ms = end - start;
  const std::size_t total_ops = kCasThreads * kCasOpsPerThread;
  const double ns_per_op =
      (elapsed_ms.count() * 1e6) / static_cast<double>(total_ops);

  return {ledger.sequence, elapsed_ms.count(), ns_per_op};
}

}  // namespace
}  // namespace hotpath::mesi_coherency

/**
 * @brief Main entrypoint for the Episode 09 Silicon Switchboard Challenge.
 * @return 0 if all integrity checks pass, 1 otherwise.
 */
int main() {
  using namespace hotpath::mesi_coherency;

  std::cout
      << "==============================================================\n"
      << "🔥 Hot Path C++ — Episode 09 Silicon Switchboard Challenge 🔥\n"
      << "   MESI Coherency, Split-Lock Bus Stalls & Read-Before-CAS\n"
      << "==============================================================\n\n";

  alignas(kCacheLineBytes) PackedSplitLockHeader split_hdr{};
  alignas(kCacheLineBytes) AlignedTelemetryHeader aligned_hdr{};

  std::cout << "[Part 1] Defusing a Split-Lock Telemetry Struct ("
            << kSplitLockOps << " atomic RMWs)...\n";
  const auto split_res = RunAtomicIncrementLoop(split_hdr.seq_no);
  const auto aligned_res = RunAtomicIncrementLoop(aligned_hdr.seq_no);
  const double split_speedup = split_res.ns_per_op / aligned_res.ns_per_op;

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "  • Packed Split-Lock (Offset 60, Straddles 2 Cache Lines) :\n"
            << "      Latency : " << split_res.ns_per_op << " ns/op ("
            << split_res.duration_ms << " ms total — Global Bus Lock!)\n";
  std::cout << "  • Aligned Cache-Lock (Offset 0, Single 64B Cache Line)   :\n"
            << "      Latency : " << aligned_res.ns_per_op << " ns/op ("
            << aligned_res.duration_ms << " ms total — L1D Cache Lock ✅)\n";
  std::cout << "  • Alignment Speedup : " << split_speedup << "x Faster!\n\n";

  std::cout << "[Part 2] Taming a 16-Core CAS Contention Storm ("
            << (kCasThreads * kCasOpsPerThread) << " contended updates)...\n";
  const auto naive_cas = RunContendedCasChallenge<NaiveCasSpinlock>();
  const auto ttas_cas = RunContendedCasChallenge<ReadBeforeCasSpinlock>();
  const double cas_speedup = naive_cas.ns_per_op / ttas_cas.ns_per_op;

  const bool valid =
      (split_res.final_seq == kSplitLockOps) &&
      (aligned_res.final_seq == kSplitLockOps) &&
      (naive_cas.total_claimed == kCasThreads * kCasOpsPerThread) &&
      (ttas_cas.total_claimed == kCasThreads * kCasOpsPerThread);

  std::cout
      << "--------------------------------------------------------------\n";
  std::cout << "Naive CAS Loop (lock cmpxchg RFO Storm) : "
            << naive_cas.ns_per_op << " ns/op (" << naive_cas.duration_ms
            << " ms)\n";
  std::cout << "Read-Before-CAS (Shared S + _mm_pause)  : "
            << ttas_cas.ns_per_op << " ns/op (" << ttas_cas.duration_ms
            << " ms)\n";
  std::cout << "16-Core Contention Speedup              : " << cas_speedup
            << "x Faster!\n";
  std::cout << "Ticket Counter Integrity                : "
            << ttas_cas.total_claimed << " / "
            << (kCasThreads * kCasOpsPerThread) << " ("
            << (valid ? "PASSED ✅" : "FAILED ❌") << ")\n";
  std::cout
      << "--------------------------------------------------------------\n";

  return valid ? 0 : 1;
}
