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
 * @brief Hot Path C++ — Episode 10 Production Synchronization Retrofit Lab:
 *        - Part 1 (Workload A: 99%-Read / 1%-Write Routing Table Cache @ 16T):
 *          `std::shared_mutex` vs Exclusive `std::mutex` vs `SeqlockRouteCache`
 *        - Part 2 (Workload B: 16-Core Contended Mutations & Sharded Hash Map):
 *          `NaiveTASLock` vs `TTASPauseLock` vs `StdMutex` vs
 * `ShardedHashMap<64>`
 *        - Part 3 (Surviving the 32-Thread Oversubscription Preemption Cliff):
 *          `TTASPauseLock` vs `Cpp20AtomicWaitLock` (`SYS_futex`) &
 *          `ShardedHashMap<64>`
 */

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

#include "common/locks_under_the_hood/locks_under_the_hood.h"

namespace hotpath::locks_under_the_hood {
namespace {

constexpr std::size_t kCores16 = 16;
constexpr std::size_t kRouteOpsPerThread16 = 50'000;
constexpr std::size_t kOpsPerThread16 = 20'000;
constexpr std::size_t kOversubscribedThreads32 = 32;

struct RouteCacheResult {
  bool tear_free_ok = false;
  double duration_ms = 0.0;
  double ns_per_op = 0.0;
};

template <typename CacheType>
RouteCacheResult RunRouteCacheTrial(std::size_t num_threads,
                                    std::size_t ops_per_thread) {
  CacheType cache;
  std::barrier start_barrier(static_cast<std::ptrdiff_t>(num_threads));
  std::atomic<bool> torn_detected{false};
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  const auto t0 = std::chrono::high_resolution_clock::now();

  for (std::size_t tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      PinThreadToCore(static_cast<int>(tid));
      start_barrier.arrive_and_wait();

      for (std::size_t i = 1; i <= ops_per_thread; ++i) {
        if ((i % 100ULL) == 0ULL) {
          const std::uint64_t ip = 0x0A000001ULL + (i & 0xFFULL);
          const std::uint64_t tms = 250ULL + (tid & 7ULL);
          const std::uint64_t w = 100ULL + i;
          RouteEntry updated{ip, tms, w, ip ^ tms ^ w};
          cache.Write(updated);
        } else {
          RouteEntry snap = cache.Read();
          if ((snap.upstream_ip ^ snap.timeout_ms ^ snap.weight) !=
              snap.checksum) {
            torn_detected.store(true, std::memory_order_relaxed);
          }
        }
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

  return {!torn_detected.load(std::memory_order_relaxed), elapsed_ms.count(),
          ns_per_op};
}

struct LockBenchmarkResult {
  std::uint64_t final_seq = 0;
  bool invariant_ok = false;
  double duration_ms = 0.0;
  double ns_per_op = 0.0;
};

template <typename LockType>
LockBenchmarkResult RunLockTrial(std::size_t num_threads,
                                 std::size_t ops_per_thread) {
  LockType lock;
  ProtectedState shared;
  std::barrier start_barrier(static_cast<std::ptrdiff_t>(num_threads));
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  const auto t0 = std::chrono::high_resolution_clock::now();

  for (std::size_t tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      PinThreadToCore(static_cast<int>(tid));
      start_barrier.arrive_and_wait();

      for (std::size_t i = 0; i < ops_per_thread; ++i) {
        lock.lock();
        shared.ApplyTransfer(1);
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

LockBenchmarkResult RunShardedMapTrial(std::size_t num_threads,
                                       std::size_t ops_per_thread) {
  ShardedHashMap<64> map;
  std::barrier start_barrier(static_cast<std::ptrdiff_t>(num_threads));
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  const auto t0 = std::chrono::high_resolution_clock::now();

  for (std::size_t tid = 0; tid < num_threads; ++tid) {
    threads.emplace_back([&, tid]() {
      PinThreadToCore(static_cast<int>(tid));
      start_barrier.arrive_and_wait();

      std::uint64_t key = (static_cast<std::uint64_t>(tid) * 1315423911ULL) ^
                          0x9E3779B97F4A7C15ULL;
      for (std::size_t i = 0; i < ops_per_thread; ++i) {
        key = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ULL;
        map.Upsert(key, 1);
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
  const std::uint64_t total_seq = map.TotalUpdates();

  return {total_seq, total_seq == total_ops, elapsed_ms.count(), ns_per_op};
}

}  // namespace
}  // namespace hotpath::locks_under_the_hood

int main() {
  using namespace hotpath::locks_under_the_hood;

  std::cout
      << "==================================================================\n"
      << "🔥 Hot Path C++ — Episode 10 Production Lock Retrofit Challenge 🔥\n"
      << "   Seqlocks, TTAS + PAUSE, 3-State Futex & 64-Shard Hash Tables\n"
      << "=================================================================="
         "\n\n";

  std::cout << std::fixed << std::setprecision(2);

  std::cout << "[Part 1] Workload A: 99%-Read / 1%-Write Routing Cache ("
            << (kCores16 * kRouteOpsPerThread16) << " ops @ 16T)...\n";
  const auto rw_shared =
      RunRouteCacheTrial<SharedMutexRouteCache>(kCores16, kRouteOpsPerThread16);
  const auto rw_mutex =
      RunRouteCacheTrial<MutexRouteCache>(kCores16, kRouteOpsPerThread16);
  const auto rw_seqlock =
      RunRouteCacheTrial<SeqlockRouteCache>(kCores16, kRouteOpsPerThread16);

  std::cout << "  • std::shared_mutex (lock_shared RFO Storm) : "
            << rw_shared.ns_per_op << " ns/op (" << rw_shared.duration_ms
            << " ms)\n";
  std::cout << "  • Exclusive std::mutex                      : "
            << rw_mutex.ns_per_op << " ns/op (" << rw_mutex.duration_ms
            << " ms)\n";
  std::cout << "  • SeqlockRouteCache (100% Shared [S] Reads) : "
            << rw_seqlock.ns_per_op << " ns/op (" << rw_seqlock.duration_ms
            << " ms) -> " << (rw_shared.ns_per_op / rw_seqlock.ns_per_op)
            << "x Faster! ✅\n\n";

  std::cout << "[Part 2] Workload B: 16-Core Contended Mutations vs 64-Shard "
               "Map ("
            << (kCores16 * kOpsPerThread16) << " critical sections)...\n";
  const auto tas_16 = RunLockTrial<NaiveTASLock>(kCores16, kOpsPerThread16);
  const auto ttas_16 = RunLockTrial<TTASPauseLock>(kCores16, kOpsPerThread16);
  const auto sharded_16 = RunShardedMapTrial(kCores16, kOpsPerThread16);

  std::cout << "  • NaiveTASLock      (lock xchg RFO Storm)   : "
            << tas_16.ns_per_op << " ns/op (" << tas_16.duration_ms << " ms)\n";
  std::cout << "  • TTASPauseLock     (Shared S + _mm_pause)  : "
            << ttas_16.ns_per_op << " ns/op (" << ttas_16.duration_ms
            << " ms)\n";
  std::cout << "  • ShardedHashMap<64> (alignas(64) std::mutex): "
            << sharded_16.ns_per_op << " ns/op (" << sharded_16.duration_ms
            << " ms) -> " << (tas_16.ns_per_op / sharded_16.ns_per_op)
            << "x Faster! ✅\n\n";

  std::cout << "[Part 3] 32-Thread Oversubscription on 16 Physical Cores ("
            << (kOversubscribedThreads32 * kOpsPerThread16)
            << " contended handoffs)...\n";
  const auto ttas_32 =
      RunLockTrial<TTASPauseLock>(kOversubscribedThreads32, kOpsPerThread16);
  const auto futex_32 = RunLockTrial<AdaptiveFutexMutex>(
      kOversubscribedThreads32, kOpsPerThread16);
  const auto sharded_32 =
      RunShardedMapTrial(kOversubscribedThreads32, kOpsPerThread16);
  const double futex_speedup = ttas_32.ns_per_op / futex_32.ns_per_op;
  const double sharded_speedup = ttas_32.ns_per_op / sharded_32.ns_per_op;

  const bool all_valid = rw_shared.tear_free_ok && rw_mutex.tear_free_ok &&
                         rw_seqlock.tear_free_ok && tas_16.invariant_ok &&
                         ttas_16.invariant_ok && sharded_16.invariant_ok &&
                         ttas_32.invariant_ok && futex_32.invariant_ok &&
                         sharded_32.invariant_ok;

  std::cout << "  • Pure Spinlock (TTASPauseLock @ 32T)       : "
            << ttas_32.ns_per_op << " ns/op (" << ttas_32.duration_ms
            << " ms)\n";
  std::cout << "  • AdaptiveFutex (3-State SYS_futex @ 32T)   : "
            << futex_32.ns_per_op << " ns/op (" << futex_32.duration_ms
            << " ms) -> " << futex_speedup << "x Faster!\n";
  std::cout << "  • ShardedHashMap<64> (alignas(64) @ 32T)    : "
            << sharded_32.ns_per_op << " ns/op (" << sharded_32.duration_ms
            << " ms) -> " << sharded_speedup << "x Faster! ✅\n";
  std::cout
      << "------------------------------------------------------------------\n";
  std::cout << "  • Invariant & Tear-Free Verification        : "
            << (all_valid ? "PASSED ✅" : "FAILED ❌") << "\n";
  std::cout
      << "==================================================================\n";

  return all_valid ? 0 : 1;
}
