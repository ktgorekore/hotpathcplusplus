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
 * @file locks_under_the_hood.h
 * @brief Episode 10 production synchronization primitives:
 * 1. Workload A (99%-Read / 1%-Write Routing Cache):
 *    - `SharedMutexRouteCache` (`std::shared_mutex` reader-counter RFO storm)
 *    - `MutexRouteCache` (Exclusive `std::mutex`)
 *    - `SeqlockRouteCache` (Linux kernel `seqlock.h`-style lock-free Seqlock
 *      keeping all readers in MESI `Shared [S]` state).
 * 2. Workload B (Multi-Writer Concurrent Key-Value Hash Table):
 *    - `NaiveTASLock` (`test_and_set` / `lock xchg`), `TTASNoPauseLock`,
 *      `TTASPauseLock` (`Shared [S]` + `_mm_pause()`)
 *    - `RawFutexMutex` (Linux 3-state `SYS_futex`), `AdaptiveFutexMutex`,
 *      `Cpp20AtomicWaitLock`, `StdMutexWrapper`
 *    - `ShardedHashMap<64>` (`alignas(64)` 64-shard `std::mutex` concurrent
 *      hash table modeled after RocksDB `ShardedLRUCache`).
 */

#ifndef HOTPATH_COMMON_LOCKS_UNDER_THE_HOOD_LOCKS_UNDER_THE_HOOD_H_
#define HOTPATH_COMMON_LOCKS_UNDER_THE_HOOD_LOCKS_UNDER_THE_HOOD_H_

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
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>

namespace hotpath::locks_under_the_hood {

/**
 * @brief Hardware L1D cache-line size in bytes on x86-64 processors.
 */
inline constexpr std::size_t kCacheLineBytes = 64;

/**
 * @brief Ensures glibc's `__libc_single_threaded` flag is cleared so
 * `pthread_mutex_lock` always executes its real `lock cmpxchg` atomic
 * instruction even in 1-thread benchmarks.
 */
inline void EnsureMultiThreadedRuntime() {
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
 * @brief 32-byte trivially copyable routing table snapshot (Workload A).
 * Must satisfy `std::is_trivially_copyable_v<RouteEntry>` for safe `Seqlock`
 * optimistic reads.
 */
struct RouteEntry {
  std::uint64_t upstream_ip = 0x0A000001ULL;
  std::uint64_t timeout_ms = 250ULL;
  std::uint64_t weight = 100ULL;
  std::uint64_t checksum = 0x0A000001ULL ^ 250ULL ^ 100ULL;
};

static_assert(std::is_trivially_copyable_v<RouteEntry>,
              "Seqlock payload must be TriviallyCopyable (no heap pointers)!");

/**
 * @class SharedMutexRouteCache
 * @brief Textbook `std::shared_mutex` reader-writer cache.
 * Under 16-core read-heavy workloads, `std::shared_lock` suffers a severe MESI
 * RFO storm because `lock_shared()` atomically increments/decrements an
 * internal reader reference counter on every read.
 */
class alignas(kCacheLineBytes) SharedMutexRouteCache {
 public:
  void Write(const RouteEntry& next) noexcept {
    std::unique_lock<std::shared_mutex> wl(rw_mtx_);
    entry_ = next;
  }

  [[nodiscard]] RouteEntry Read() const noexcept {
    std::shared_lock<std::shared_mutex> rl(rw_mtx_);
    return entry_;
  }

 private:
  mutable std::shared_mutex rw_mtx_;
  RouteEntry entry_{};
};

/**
 * @class MutexRouteCache
 * @brief Exclusive `std::mutex` routing cache for comparison against
 * `std::shared_mutex`.
 */
class alignas(kCacheLineBytes) MutexRouteCache {
 public:
  void Write(const RouteEntry& next) noexcept {
    std::lock_guard<std::mutex> lg(mtx_);
    entry_ = next;
  }

  [[nodiscard]] RouteEntry Read() const noexcept {
    std::lock_guard<std::mutex> lg(mtx_);
    return entry_;
  }

 private:
  mutable std::mutex mtx_;
  RouteEntry entry_{};
};

/**
 * @class SeqlockRouteCache
 * @brief Linux-kernel-style Sequence Lock (`Seqlock`) for read-heavy caches.
 *
 * Writers increment `seq_` to an odd number before updating `entry_` and to the
 * next even number on completion. Readers verify `seq_` is even and unchanged
 * (`s0 == s1`), executing zero atomic writes and keeping all reader cores in
 * the MESI `Shared (S)` state.
 */
class alignas(kCacheLineBytes) SeqlockRouteCache {
 public:
  void Write(const RouteEntry& next) noexcept {
    const std::uint64_t s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_release);  // Odd = writer active
    std::atomic_signal_fence(std::memory_order_acq_rel);
    entry_ = next;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    seq_.store(s + 2, std::memory_order_release);  // Even = consistent
  }

  [[nodiscard]] RouteEntry Read() const noexcept {
    RouteEntry copy;
    std::uint64_t s0 = 0;
    std::uint64_t s1 = 0;
    do {
      s0 = seq_.load(std::memory_order_acquire);
      if ((s0 & 1ULL) != 0ULL) {
        _mm_pause();
        continue;
      }
      std::atomic_signal_fence(std::memory_order_acq_rel);
      copy = entry_;
      std::atomic_thread_fence(std::memory_order_acquire);
      s1 = seq_.load(std::memory_order_relaxed);
    } while (s0 != s1);
    return copy;
  }

 private:
  alignas(kCacheLineBytes) std::atomic<std::uint64_t> seq_{0};
  RouteEntry entry_{};
};

/**
 * @class ShardedHashMap
 * @brief Production 64-shard cache-aligned concurrent key-value map (modeled
 * after RocksDB `ShardedLRUCache` and `ConcurrentHashMap`).
 *
 * Each shard sits on its own `alignas(64)` cache line with a dedicated
 * `std::mutex` (`futex`), so >95% of operations across 16 threads hit the
 * uncontended Ring-3 `0 -> 1 -> 0` `futex` fast path with zero kernel syscalls
 * and zero OS preemption cliffs.
 *
 * @tparam NumShards Number of cache-aligned shards (must be a power of two).
 */
template <std::size_t NumShards = 64>
class ShardedHashMap {
  static_assert((NumShards & (NumShards - 1)) == 0,
                "NumShards must be a power of two.");

 public:
  struct alignas(kCacheLineBytes) MapShard {
    std::mutex mtx;
    std::array<std::uint64_t, 16> buckets{};
    std::uint64_t updates = 0;
  };

  /**
   * @brief Upserts `key` with `delta` inside the target cache-aligned shard.
   * @param key 64-bit lookup/update key.
   * @param delta Value increment.
   */
  void Upsert(std::uint64_t key, std::uint64_t delta = 1) noexcept {
    const std::uint64_t h = MixHash(key);
    MapShard& shard = shards_[h & (NumShards - 1)];
    std::lock_guard<std::mutex> lg(shard.mtx);
    shard.buckets[(h >> 8) & 15ULL] += delta;
    ++shard.updates;
  }

  /**
   * @brief Sums total completed updates across all shards.
   * @return Total update count.
   */
  [[nodiscard]] std::uint64_t TotalUpdates() noexcept {
    std::uint64_t sum = 0;
    for (auto& shard : shards_) {
      std::lock_guard<std::mutex> lg(shard.mtx);
      sum += shard.updates;
    }
    return sum;
  }

 private:
  static constexpr std::uint64_t MixHash(std::uint64_t x) noexcept {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
  }

  std::array<MapShard, NumShards> shards_{};
};

/**
 * @struct ProtectedState
 * @brief Multi-invariant critical section guarded by each lock under test.
 */
struct alignas(kCacheLineBytes) ProtectedState {
  std::uint64_t sequence = 0;
  std::uint64_t balance_a = 1'000'000;
  std::uint64_t balance_b = 1'000'000;
  std::uint64_t checksum = 2'000'000;
  std::array<std::uint64_t, 5> buckets{};

  /**
   * @brief Updates all invariants deterministically inside the critical
   * section.
   * @param delta Transfer amount.
   */
  void UpdateInvariants(std::uint64_t delta) noexcept {
    const std::uint64_t seq = ++sequence;
    balance_a += delta;
    balance_b -= delta;
    checksum = (balance_a + balance_b) ^ (seq * 0x9E3779B97F4A7C15ULL);
    for (std::size_t idx = 0; idx < buckets.size(); ++idx) {
      buckets[idx] = (buckets[idx] ^ (seq + idx)) * 6364136223846793005ULL +
                     1442695040888963407ULL;
    }
  }

  /**
   * @brief Applies an exact two-account transfer for invariant verification.
   * @param amount Transfer amount.
   */
  void ApplyTransfer(std::uint64_t amount) noexcept {
    const std::uint64_t seq = ++sequence;
    balance_a += amount;
    balance_b -= amount;
    for (std::size_t idx = 0; idx < buckets.size(); ++idx) {
      buckets[idx] = (buckets[idx] ^ (seq + idx)) * 6364136223846793005ULL +
                     1442695040888963407ULL;
    }
  }
};

/**
 * @struct NaiveTASLock
 * @brief Classic 3-line textbook Test-and-Set (`TAS`) spinlock.
 * Issues `xchg` (implicit hardware `LOCK`) on every failed spin, flooding the
 * interconnect with RFO broadcasts and starving `unlock()`.
 */
struct alignas(kCacheLineBytes) NaiveTASLock {
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
 * but omits `_mm_pause()`, suffering Reorder Buffer (ROB) Memory Order Machine
 * Clears and starving the sibling SMT thread.
 */
struct alignas(kCacheLineBytes) TTASNoPauseLock {
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
 * `Shared (S)` read polling with `_mm_pause()` (`F3 90` / `rep nop`).
 */
struct alignas(kCacheLineBytes) TTASPauseLock {
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
struct alignas(kCacheLineBytes) StdMutexWrapper {
  std::mutex mtx;

  void lock() noexcept { mtx.lock(); }
  void unlock() noexcept { mtx.unlock(); }
};

/**
 * @struct RawFutexMutex
 * @brief Explicit 3-State Linux `futex` (`Fast Userspace Mutex`) following
 * Ulrich Drepper's "Futexes Are Tricky" state machine:
 * - State 0: Unlocked
 * - State 1: Locked, zero kernel waiters (Ring 3 fast path!)
 * - State 2: Locked, one or more threads sleeping in `SYS_futex`
 */
struct alignas(kCacheLineBytes) RawFutexMutex {
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
 * userspace spin with 3-state `SYS_futex` kernel parking.
 */
struct alignas(kCacheLineBytes) AdaptiveFutexMutex {
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
 * and `notify_one()`.
 */
struct alignas(kCacheLineBytes) Cpp20AtomicWaitLock {
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

}  // namespace hotpath::locks_under_the_hood

#endif  // HOTPATH_COMMON_LOCKS_UNDER_THE_HOOD_LOCKS_UNDER_THE_HOOD_H_
