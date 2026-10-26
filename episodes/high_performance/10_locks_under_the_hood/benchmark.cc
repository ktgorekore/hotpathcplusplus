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
 * @brief Hot Path C++ — Episode 10 Benchmark: Stop Fighting Your Locks
 *        (Seqlocks, TTAS + x86 PAUSE, 3-State Futex & 64-Shard Hash Tables).
 *
 * Benchmarks:
 * 1. BM_WorkloadA_RouteCache_16T (99% Read / 1% Write Routing Cache @ 16T):
 *    - SharedMutex_99R1W (`std::shared_mutex::lock_shared()` RFO storm)
 *    - ExclusiveMutex_99R1W (`std::mutex`)
 *    - Seqlock_99R1W (`SeqlockRouteCache<RouteEntry>`, 100% MESI Shared [S])
 * 2. BM_Lock_Uncontended (1 Thread):
 *    - TTASPauseLock, std::mutex, RawFutexMutex, Cpp20AtomicWaitLock, and
 *      BM_RawSyscall_Futex to prove uncontended mutexes execute a single
 *      Ring 3 `lock cmpxchg` with zero kernel syscalls.
 * 3. BM_Lock_Contended_16T (16 Threads on 16 Hardware Threads):
 *    - NaiveTASLock, TTASNoPauseLock, TTASPauseLock, StdMutex,
 *      AdaptiveFutexMutex, Cpp20AtomicWaitLock, and ShardedHashMap_64Shards.
 * 4. BM_Spinlock_OversubscriptionCliff_32T (32 Threads on 16 Physical Cores):
 *    - Demonstrates the catastrophic preemption cliff on pure spinlocks vs
 *      `std::mutex`, `AdaptiveFutexMutex`, and `ShardedHashMap_64Shards`.
 */

#include "benchmark/benchmark.h"

#include <immintrin.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>

namespace hotpath::episode10 {
namespace {

/**
 * @brief Ensures glibc's `__libc_single_threaded` flag is cleared so
 *        `pthread_mutex_lock` always executes its real `lock cmpxchg` atomic
 *        instruction even in 1-thread benchmarks.
 */
void EnsureMultiThreadedRuntime() {
  static const bool initialized = []() {
    std::thread([]() {}).join();
    return true;
  }();
  (void)initialized;
}

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
 * @brief 32-byte trivially copyable service routing table snapshot used in
 *        Workload A (99% Read / 1% Write Routing Cache).
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

/**
 * @class SharedMutexRouteCache
 * @brief Workload A anti-pattern: `std::shared_mutex` where every reader calls
 *        `lock_shared()`, executing atomic RMWs (`lock cmpxchg` / `lock xadd`)
 *        on the internal reader counter and triggering an RFO storm.
 */
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

/**
 * @class MutexRouteCache
 * @brief Workload A comparison: exclusive `std::mutex` guarding `RouteEntry`.
 */
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

/**
 * @class SeqlockRouteCache
 * @brief Workload A production fix: Sequence Lock (`Seqlock`). Readers execute
 *        zero atomic RMW stores and keep the cache line 100% in MESI
 *        `Shared (S)` state across all 16 cores.
 */
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
 * @brief Realistic ~10-nanosecond multi-invariant critical section guarded by
 *        each lock under test.
 */
struct alignas(64) ProtectedState {
  std::uint64_t sequence = 0;
  std::uint64_t balance_a = 1'000'000;
  std::uint64_t balance_b = 1'000'000;
  std::array<std::uint64_t, 5> buckets{};

  void UpdateInvariants(std::uint64_t delta) noexcept {
    const std::uint64_t seq = ++sequence;
    balance_a += delta;
    balance_b -= delta;
    for (std::size_t idx = 0; idx < buckets.size(); ++idx) {
      buckets[idx] = (buckets[idx] ^ (seq + idx)) * 6364136223846793005ULL +
                     1442695040888963407ULL;
    }
  }
};

/**
 * @struct NaiveTASLock
 * @brief Classic 3-line textbook Test-and-Set (`TAS`) spinlock.
 */
struct alignas(64) NaiveTASLock {
  std::atomic_flag flag = ATOMIC_FLAG_INIT;

  void lock() noexcept {
    while (flag.test_and_set(std::memory_order_acquire)) {
    }
  }

  void unlock() noexcept { flag.clear(std::memory_order_release); }
};

/**
 * @struct TTASNoPauseLock
 * @brief Test-and-Test-and-Set spinlock that spins in MESI `Shared (S)` state
 *        but omits `_mm_pause()`, suffering Reorder Buffer (ROB) Memory Order
 *        Machine Clears.
 */
struct alignas(64) TTASNoPauseLock {
  std::atomic<bool> locked{false};

  void lock() noexcept {
    for (;;) {
      if (!locked.load(std::memory_order_relaxed) &&
          !locked.exchange(true, std::memory_order_acquire)) {
        return;
      }
      while (locked.load(std::memory_order_relaxed)) {
      }
    }
  }

  void unlock() noexcept { locked.store(false, std::memory_order_release); }
};

/**
 * @struct TTASPauseLock
 * @brief Hardware-sympathetic Test-and-Test-and-Set (`TTAS`) spinlock pairing
 *        `Shared (S)` read polling with `_mm_pause()` (`F3 90` / `rep nop`).
 */
struct alignas(64) TTASPauseLock {
  std::atomic<bool> locked{false};

  void lock() noexcept {
    for (;;) {
      if (!locked.load(std::memory_order_relaxed) &&
          !locked.exchange(true, std::memory_order_acquire)) {
        return;
      }
      while (locked.load(std::memory_order_relaxed)) {
        _mm_pause();
      }
    }
  }

  void unlock() noexcept { locked.store(false, std::memory_order_release); }
};

/**
 * @struct StdMutexWrapper
 * @brief Thin wrapper around `std::mutex` (`pthread_mutex_t`) aligned to 64B.
 */
struct alignas(64) StdMutexWrapper {
  std::mutex mtx;

  void lock() noexcept { mtx.lock(); }
  void unlock() noexcept { mtx.unlock(); }
};

/**
 * @struct RawFutexMutex
 * @brief Explicit 3-State Linux `futex` (`Fast Userspace Mutex`) following
 *        Ulrich Drepper's "Futexes Are Tricky" state machine.
 */
struct alignas(64) RawFutexMutex {
  std::atomic<std::uint32_t> state{0};

  void lock() noexcept {
    std::uint32_t c = 0;
    if (state.compare_exchange_strong(c, 1, std::memory_order_acquire,
                                      std::memory_order_relaxed)) {
      return;
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

/**
 * @struct AdaptiveFutexMutex
 * @brief Hybrid lock combining a bounded 40-iteration `TTAS + _mm_pause()`
 *        userspace spin with 3-state `SYS_futex` kernel parking.
 */
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

/**
 * @struct Cpp20AtomicWaitLock
 * @brief Portable C++20 3-state lock built on `std::atomic<uint32_t>::wait`
 *        and `notify_one()`.
 */
struct alignas(64) Cpp20AtomicWaitLock {
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
      state.wait(2, std::memory_order_relaxed);
      c = state.exchange(2, std::memory_order_acquire);
    }
  }

  void unlock() noexcept {
    if (state.exchange(0, std::memory_order_release) == 2) {
      state.notify_one();
    }
  }
};

/**
 * @class ShardedHashMap
 * @brief Workload B architectural fix: 64-shard `alignas(64)` `std::mutex`
 *        table modeled after RocksDB's `ShardedLRUCache`.
 */
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
    shard.state.UpdateInvariants(delta);
  }

 private:
  std::array<Shard, NumShards> shards_{};
};

SharedMutexRouteCache g_shared_mutex_route_cache;
MutexRouteCache g_mutex_route_cache;
SeqlockRouteCache<RouteEntry> g_seqlock_route_cache;

template <typename CacheType>
void BM_WorkloadA_RouteCache_99R1W(benchmark::State& state, CacheType& cache) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(state.thread_index());
  std::uint64_t iter = static_cast<std::uint64_t>(state.thread_index());

  for (auto _ : state) {
    ++iter;
    if ((iter % 100ULL) == 0ULL) {
      RouteEntry updated{0x0A000001u + static_cast<std::uint32_t>(iter & 0xFFu),
                         static_cast<std::uint16_t>(8080u + (iter & 7u)),
                         1u,
                         iter,
                         iter * 1024ULL,
                         0ULL};
      cache.update(updated);
    } else {
      RouteEntry snap = cache.read();
      benchmark::DoNotOptimize(snap);
    }
  }
  state.SetItemsProcessed(state.iterations());
}

void BM_RouteCache_SharedMutex_16T(benchmark::State& state) {
  BM_WorkloadA_RouteCache_99R1W(state, g_shared_mutex_route_cache);
}
BENCHMARK(BM_RouteCache_SharedMutex_16T)
    ->Name("BM_WorkloadA_RouteCache_16T/SharedMutex_99R1W")
    ->Threads(16)
    ->UseRealTime();

void BM_RouteCache_ExclusiveMutex_16T(benchmark::State& state) {
  BM_WorkloadA_RouteCache_99R1W(state, g_mutex_route_cache);
}
BENCHMARK(BM_RouteCache_ExclusiveMutex_16T)
    ->Name("BM_WorkloadA_RouteCache_16T/ExclusiveMutex_99R1W")
    ->Threads(16)
    ->UseRealTime();

void BM_RouteCache_Seqlock_16T(benchmark::State& state) {
  BM_WorkloadA_RouteCache_99R1W(state, g_seqlock_route_cache);
}
BENCHMARK(BM_RouteCache_Seqlock_16T)
    ->Name("BM_WorkloadA_RouteCache_16T/Seqlock_99R1W")
    ->Threads(16)
    ->UseRealTime();

template <typename LockType>
void BM_Lock_Uncontended(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(0);
  LockType lock;
  ProtectedState shared;

  for (auto _ : state) {
    lock.lock();
    shared.UpdateInvariants(1);
    lock.unlock();
    benchmark::DoNotOptimize(shared);
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Lock_Uncontended, TTASPauseLock)
    ->Name("BM_Lock_Uncontended/TTASPauseLock")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Uncontended, StdMutexWrapper)
    ->Name("BM_Lock_Uncontended/StdMutex")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Uncontended, RawFutexMutex)
    ->Name("BM_Lock_Uncontended/RawFutexMutex")
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Uncontended, Cpp20AtomicWaitLock)
    ->Name("BM_Lock_Uncontended/Cpp20AtomicWaitLock")
    ->UseRealTime();

void BM_RawSyscall_Futex(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(0);
  alignas(64) std::uint32_t futex_word = 0;
  for (auto _ : state) {
    long res = syscall(SYS_futex, &futex_word, FUTEX_WAKE_PRIVATE, 1, nullptr,
                       nullptr, 0);
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RawSyscall_Futex)->UseRealTime();

template <typename LockType>
struct alignas(256) ContendedArena {
  alignas(256) LockType lock;
  alignas(256) ProtectedState shared;
};

template <typename LockType>
ContendedArena<LockType> g_arena;

template <typename LockType>
void BM_Lock_Contended(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(state.thread_index());
  auto& arena = g_arena<LockType>;

  for (auto _ : state) {
    arena.lock.lock();
    arena.shared.UpdateInvariants(1);
    arena.lock.unlock();
  }
  benchmark::DoNotOptimize(arena.shared);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Lock_Contended, NaiveTASLock)
    ->Name("BM_Lock_Contended_16T/NaiveTASLock")
    ->Threads(16)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Contended, TTASNoPauseLock)
    ->Name("BM_Lock_Contended_16T/TTASNoPauseLock")
    ->Threads(16)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Contended, TTASPauseLock)
    ->Name("BM_Lock_Contended_16T/TTASPauseLock")
    ->Threads(16)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Contended, StdMutexWrapper)
    ->Name("BM_Lock_Contended_16T/StdMutex")
    ->Threads(16)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Contended, AdaptiveFutexMutex)
    ->Name("BM_Lock_Contended_16T/AdaptiveFutexMutex")
    ->Threads(16)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Lock_Contended, Cpp20AtomicWaitLock)
    ->Name("BM_Lock_Contended_16T/Cpp20AtomicWaitLock")
    ->Threads(16)
    ->UseRealTime();

ShardedHashMap<64> g_sharded_map_64;

void BM_ShardedHashMap64_16T(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(state.thread_index());
  std::uint64_t key =
      (static_cast<std::uint64_t>(state.thread_index()) * 1315423911ULL) ^
      0x9E3779B97F4A7C15ULL;

  for (auto _ : state) {
    key = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ULL;
    g_sharded_map_64.Update(key, 1);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ShardedHashMap64_16T)
    ->Name("BM_Lock_Contended_16T/ShardedHashMap_64Shards")
    ->Threads(16)
    ->UseRealTime();

template <typename LockType>
void BM_Spinlock_OversubscriptionCliff(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(state.thread_index() % 16);
  auto& arena = g_arena<LockType>;

  for (auto _ : state) {
    arena.lock.lock();
    arena.shared.UpdateInvariants(1);
    arena.lock.unlock();
  }
  benchmark::DoNotOptimize(arena.shared);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Spinlock_OversubscriptionCliff, NaiveTASLock)
    ->Name("BM_Spinlock_OversubscriptionCliff_32T/NaiveTASLock")
    ->Threads(32)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Spinlock_OversubscriptionCliff, TTASPauseLock)
    ->Name("BM_Spinlock_OversubscriptionCliff_32T/TTASPauseLock")
    ->Threads(32)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Spinlock_OversubscriptionCliff, StdMutexWrapper)
    ->Name("BM_Spinlock_OversubscriptionCliff_32T/StdMutex")
    ->Threads(32)
    ->UseRealTime();
BENCHMARK_TEMPLATE(BM_Spinlock_OversubscriptionCliff, AdaptiveFutexMutex)
    ->Name("BM_Spinlock_OversubscriptionCliff_32T/AdaptiveFutexMutex")
    ->Threads(32)
    ->UseRealTime();

void BM_ShardedHashMap64_Oversubscribed_32T(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(state.thread_index() % 16);
  std::uint64_t key =
      (static_cast<std::uint64_t>(state.thread_index()) * 1315423911ULL) ^
      0x9E3779B97F4A7C15ULL;

  for (auto _ : state) {
    key = (key ^ (key >> 30)) * 0xBF58476D1CE4E5B9ULL;
    g_sharded_map_64.Update(key, 1);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ShardedHashMap64_Oversubscribed_32T)
    ->Name("BM_Spinlock_OversubscriptionCliff_32T/ShardedHashMap_64Shards")
    ->Threads(32)
    ->UseRealTime();

}  // namespace
}  // namespace hotpath::episode10
