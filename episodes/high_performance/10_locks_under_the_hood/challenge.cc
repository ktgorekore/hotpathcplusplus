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
 * @brief Hot Path C++ — Episode 10 Hands-On Lock Engineering Challenge.
 */

#include <sched.h>

#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "common/locks_under_the_hood/locks_under_the_hood.h"

namespace hotpath::episode10 {
namespace {

using hotpath::locks_under_the_hood::Cpp20AtomicWaitLock;
using hotpath::locks_under_the_hood::NaiveTASLock;
using hotpath::locks_under_the_hood::PinThreadToCCD0;
using hotpath::locks_under_the_hood::ProtectedState;
using hotpath::locks_under_the_hood::TTASPauseLock;

constexpr std::size_t kCores16 = 16;
constexpr std::size_t kOpsPerThread16 = 20'000;
constexpr std::size_t kOversubscribedThreads32 = 32;
constexpr std::size_t kOpsPerThread32 = 2'000;

struct LockBenchmarkResult {
  std::uint64_t final_seq = 0;
  bool invariant_ok = false;
  double duration_ms = 0.0;
  double ns_per_op = 0.0;
};

template <typename LockType>
LockBenchmarkResult RunLockTrial(std::size_t num_threads,
                                 std::size_t ops_per_thread,
                                 bool simulate_preemption) {
  LockType lock;
  ProtectedState shared;
  std::barrier start_barrier(static_cast<std::ptrdiff_t>(num_threads));
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  const auto t0 = std::chrono::high_resolution_clock::now();

  for (std::size_t tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      PinThreadToCCD0(static_cast<int>(tid));
      start_barrier.arrive_and_wait();

      for (std::size_t i = 0; i < ops_per_thread; ++i) {
        lock.lock();
        shared.ApplyTransfer(1);
        if (simulate_preemption && (shared.sequence & 63ULL) == 0ULL) {
          sched_yield();
        }
        lock.unlock();
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  const auto t1 = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double, std::milli> elapsed_ms = t1 - t0;
  const std::size_t total_ops = num_threads * ops_per_thread;
  const double ns_per_op =
      (elapsed_ms.count() * 1e6) / static_cast<double>(total_ops);
  const bool ok = (shared.sequence == total_ops) &&
                  (shared.balance_a + shared.balance_b == 2'000'000ULL);

  return {shared.sequence, ok, elapsed_ms.count(), ns_per_op};
}

}  // namespace
}  // namespace hotpath::episode10

int main() {
  using namespace hotpath::episode10;

  std::cout
      << "==================================================================\n"
      << "🔥 Hot Path C++ — Episode 10 Lock Engineering Challenge 🔥\n"
      << "   TAS RFO Storms, TTAS + x86 PAUSE & C++20 Adaptive Futex Locks\n"
      << "=================================================================="
         "\n\n";

  std::cout << "[Part 1] 16-Core Contended Lock Handoff ("
            << (kCores16 * kOpsPerThread16) << " critical sections)...\n";
  const auto tas_16 =
      RunLockTrial<NaiveTASLock>(kCores16, kOpsPerThread16, false);
  const auto ttas_16 =
      RunLockTrial<TTASPauseLock>(kCores16, kOpsPerThread16, false);
  const double speedup_16 = tas_16.ns_per_op / ttas_16.ns_per_op;

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "  • NaiveTASLock  (lock xchg RFO Storm)       : "
            << tas_16.ns_per_op << " ns/op (" << tas_16.duration_ms << " ms)\n";
  std::cout << "  • TTASPauseLock (Shared S + _mm_pause)      : "
            << ttas_16.ns_per_op << " ns/op (" << ttas_16.duration_ms
            << " ms)\n";
  std::cout << "  • 16-Core TTAS + PAUSE Speedup              : " << speedup_16
            << "x Faster! ✅\n\n";

  std::cout
      << "[Part 2] 32-Thread Oversubscription Cliff on 16 Physical Cores ("
      << (kOversubscribedThreads32 * kOpsPerThread32)
      << " preempted handoffs)...\n";
  const auto ttas_32 = RunLockTrial<TTASPauseLock>(kOversubscribedThreads32,
                                                   kOpsPerThread32, true);
  const auto futex_32 = RunLockTrial<Cpp20AtomicWaitLock>(
      kOversubscribedThreads32, kOpsPerThread32, true);
  const double cliff_speedup = ttas_32.ns_per_op / futex_32.ns_per_op;

  const bool all_valid = tas_16.invariant_ok && ttas_16.invariant_ok &&
                         ttas_32.invariant_ok && futex_32.invariant_ok;

  std::cout << "  • Pure Spinlock (TTASPauseLock @ 32T Cliff) : "
            << ttas_32.ns_per_op << " ns/op (" << ttas_32.duration_ms
            << " ms — Preempted Lock Holder!)\n";
  std::cout << "  • AdaptiveLock  (C++20 atomic::wait / futex): "
            << futex_32.ns_per_op << " ns/op (" << futex_32.duration_ms
            << " ms — Yields CPU to Holder!)\n";
  std::cout << "  • Oversubscription Survival Speedup         : "
            << cliff_speedup << "x Faster!\n";
  std::cout
      << "------------------------------------------------------------------\n";
  std::cout << "  • Invariant & Sequence Verification         : "
            << (all_valid ? "PASSED ✅" : "FAILED ❌") << "\n";
  std::cout
      << "==================================================================\n";

  return all_valid ? 0 : 1;
}
