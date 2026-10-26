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
 *
 * Three-Part Production Verification Lab:
 * - Part 1 (Workload A: 99%-Read / 1%-Write Routing Cache @ 16T):
 *   Compares `std::shared_mutex` (`lock_shared()` RFO storm) against exclusive
 *   `std::mutex` and a tear-free `SeqlockRouteCache<RouteEntry>`.
 * - Part 2 (Workload B: 16-Core Contended Mutations vs 64-Shard Hash Map):
 *   Compares `NaiveTASLock` against `TTASPauseLock` (`Shared (S)` +
 *   `_mm_pause()`) and `ShardedHashMap<64>` (`alignas(64) std::mutex`).
 * - Part 3 (Surviving the 32-Thread Oversubscription Preemption Cliff):
 *   Compares `TTASPauseLock` against a 3-state C++20 `AdaptiveAtomicWaitLock`
 *   (`std::atomic<uint32_t>::wait` / `notify_one`) when 32 active threads share
 *   16 physical CPU cores.
 */

#include <immintrin.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace hotpath::episode10 {
namespace {

constexpr std::size_t kCores16 = 16;
constexpr std::size_t kRouteOpsPerThread16 = 50'000;
constexpr std::size_t kOpsPerThread16 = 20'000;
constexpr std::size_t kOversubscribedThreads32 = 32;

/**
 * @brief Pins the calling thread to a specific physical CPU core ID (0..15).
 * @param core_id Physical core index.
 */
inline void PinThreadToCore(int core_id) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id % 16, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/**
 * @struct RouteEntry
 * @brief 32-byte trivially copyable service routing table snapshot.
 */
struct RouteEntry {
  std::uint32_t upstream_ipv4 = 0x0A000001u;
  std::uint16_t port = 8080u;
  std::uint16_t weight = 100u;
  std::uint64_t version_epoch = 1ULL;
  std::uint64_t quota_bytes = 1024ULL;
  std::uint64_t flags = 0ULL;
};
static_assert(std::is_trivially_copyable_v<RouteEntry>,
              "Seqlock payload must be trivially copyable!");

class alignas(64) SharedMutexRouteCache {
 public:
  RouteEntry read() const noexcept {
    std::shared_lock<std::shared_mutex> lock(rw_mtx_);
    return entry_;
  }

  void update(const RouteEntry& next) noexcept {
    std::unique_lock<std::shared_mutex> lock(rw_mtx_);
    entry_ = next;
  }

 private:
  mutable std::shared_mutex rw_mtx_;
  RouteEntry entry_{};
};

class alignas(64) MutexRouteCache {
 public:
  RouteEntry read() const noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    return entry_;
  }

  void update(const RouteEntry& next) noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    entry_ = next;
  }

 private:
  mutable std::mutex mtx_;
  RouteEntry entry_{};
};

template <typename T>
class alignas(64) SeqlockRouteCache {
  static_assert(std::is_trivially_copyable_v<T>,
                "Seqlock requires a trivially copyable payload (no heap ptrs)");

 public:
  T read() const noexcept {
    T copy;
    for (;;) {
      const std::uint64_t seq1 = seq_.load(std::memory_order_acquire);
      if ((seq1 & 1ULL) != 0ULL) {
        _mm_pause();
        continue;
      }
      std::atomic_signal_fence(std::memory_order_acq_rel);
      std::memcpy(&copy, &data_, sizeof(T));
      std::atomic_thread_fence(std::memory_order_acquire);
      const std::uint64_t seq2 = seq_.load(std::memory_order_relaxed);
      if (seq1 == seq2) {
        return copy;
      }
    }
  }

  void update(const T& next) noexcept {
    std::lock_guard<std::mutex> guard(writer_mtx_);
    const std::uint64_t seq0 = seq_.load(std::memory_order_relaxed);
    seq_.store(seq0 + 1ULL, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    std::atomic_signal_fence(std::memory_order_acq_rel);
    std::memcpy(&data_, &next, sizeof(T));
    seq_.store(seq0 + 2ULL, std::memory_order_release);
  }

 private:
  std::atomic<std::uint64_t> seq_{0};
  T data_{};
  std::mutex writer_mtx_;
};

/**
 * @struct ProtectedState
 * @brief Multi-variable critical section guarded by the locks under test.
 */
struct alignas(64) ProtectedState {
  std::uint64_t sequence = 0;
  std::uint64_t acct_a = 1'000'000;
  std::uint64_t acct_b = 1'000'000;
  std::array<std::uint64_t, 5> buckets{};

  void ApplyTransfer(std::uint64_t amount) noexcept {
    const std::uint64_t seq = ++sequence;
    acct_a += amount;
    acct_b -= amount;
    for (std::size_t idx = 0; idx < buckets.size(); ++idx) {
      buckets[idx] = (buckets[idx] ^ (seq + idx)) * 6364136223846793005ULL +
                     1442695040888963407ULL;
    }
  }
};

struct alignas(64) NaiveTASLock {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

  void lock() noexcept {
    while (flag.test_and_set(std::memory_order_acquire)) {
    }
  }

  void unlock() noexcept { flag.clear(std::memory_order_release); }
};

struct alignas(64) TTASPauseLock {
  std::atomic<bool> locked{false};

  void lock() noexcept {
    for (;;) {
      if (!locked.exchange(true, std::memory_order_acquire)) {
        return;
      }
      while (locked.load(std::memory_order_relaxed)) {
        _mm_pause();
      }
    }
  }

  void unlock() noexcept { locked.store(false, std::memory_order_release); }
};

struct alignas(64) AdaptiveFutexMutex {
  std::atomic<std::uint32_t> state{0};

  void lock() noexcept {
    std::uint32_t c = 0;
    if (state.compare_exchange_strong(c, 1, std::memory_order_acquire,
                                      std::memory_order_relaxed)) {
      return;
    }
    for (int spin = 0; spin < 40; ++spin) {
      if (state.load(std::memory_order_relaxed) == 0) {
        c = 0;
        if (state.compare_exchange_weak(c, 1, std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
          return;
        }
      }
      _mm_pause();
    }
    if (c != 2) {
      c = state.exchange(2, std::memory_order_acquire);
    }
    while (c != 0) {
      syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(&state),
              FUTEX_WAIT_PRIVATE, 2, nullptr, nullptr, 0);
      c = state.exchange(2, std::memory_order_acquire);
    }
  }

  void unlock() noexcept {
    if (state.exchange(0, std::memory_order_release) == 2) {
      syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(&state),
              FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
    }
  }
};

template <std::size_t NumShards = 64>
class ShardedHashMap {
  static_assert((NumShards & (NumShards - 1)) == 0,
                "NumShards must be a power of two");

 public:
  struct alignas(64) Shard {
    std::mutex mtx;
    ProtectedState state;
  };

  void Update(std::uint64_t key_hash, std::uint64_t delta) noexcept {
    Shard& shard = shards_[key_hash & (NumShards - 1)];
    std::lock_guard<std::mutex> lock(shard.mtx);
    shard.state.ApplyTransfer(delta);
  }

  std::uint64_t TotalSequence() const noexcept {
    std::uint64_t total = 0;
    for (const auto& shard : shards_) {
      total += shard.state.sequence;
    }
    return total;
  }

 private:
  std::array<Shard, NumShards> shards_{};
};

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
          const std::uint64_t ver = (static_cast<std::uint64_t>(tid) << 32) | i;
          RouteEntry updated{0x0A000001u, 8080u, 1u, ver, ver * 1024ULL, 0ULL};
          cache.update(updated);
        } else {
          RouteEntry snap = cache.read();
          if (snap.quota_bytes != snap.version_epoch * 1024ULL) {
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
                  (shared.acct_a + shared.acct_b == 2'000'000ULL);

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
        map.Update(key, 1);
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
  const std::uint64_t total_seq = map.TotalSequence();

  return {total_seq, total_seq == total_ops, elapsed_ms.count(), ns_per_op};
}

}  // namespace
}  // namespace hotpath::episode10

int main() {
  using namespace hotpath::episode10;

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
  const auto rw_seqlock = RunRouteCacheTrial<SeqlockRouteCache<RouteEntry>>(
      kCores16, kRouteOpsPerThread16);

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
