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
 * @brief Episode 10 lock primitives: Naive TAS spinlock (`lock xchg`),
 * TTAS without `_mm_pause()`, hardware-sympathetic TTAS + `_mm_pause()`,
 * 3-state Linux `futex` (`SYS_futex`), Adaptive Hybrid `futex`, and C++20
 * `std::atomic<uint32_t>::wait` / `notify_one`.
 */

#ifndef HOTPATH_COMMON_LOCKS_UNDER_THE_HOOD_LOCKS_UNDER_THE_HOOD_H_
#define HOTPATH_COMMON_LOCKS_UNDER_THE_HOOD_LOCKS_UNDER_THE_HOOD_H_

#include <immintrin.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

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
 * @brief Pins the calling thread to a specific hardware thread on CCD0 (0..7
 * and 16..23) so both MESI L1D/L3 coherency and SMT sibling ROB pipeline
 * effects are measured deterministically.
 * @param thread_idx Benchmark thread index.
 */
inline void PinThreadToCCD0(int thread_idx) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  const int cpu_id = (thread_idx < 8) ? thread_idx : (thread_idx + 8);
  CPU_SET(cpu_id % 32, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
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
 * @struct ProtectedState
 * @brief Multi-invariant critical section guarded by each lock under test.
 */
struct alignas(kCacheLineBytes) ProtectedState {
  std::uint64_t sequence = 0;
  std::uint64_t balance_a = 1'000'000;
  std::uint64_t balance_b = 1'000'000;
  std::uint64_t checksum = 2'000'000;

  /**
   * @brief Updates all invariants deterministically inside the critical
   * section.
   * @param delta Transfer amount.
   */
  void UpdateInvariants(std::uint64_t delta) noexcept {
    const std::uint64_t seq = ++sequence;
    balance_a += delta ^ (seq & 7ULL);
    balance_b -= delta ^ (seq & 7ULL);
    checksum = (balance_a + balance_b) ^ (seq * 0x9E3779B97F4A7C15ULL);
  }

  /**
   * @brief Applies an exact two-account transfer for invariant verification.
   * @param amount Transfer amount.
   */
  void ApplyTransfer(std::uint64_t amount) noexcept {
    ++sequence;
    balance_a += amount;
    balance_b -= amount;
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

  /**
   * @brief Acquires the spinlock via an unshielded `test_and_set` loop.
   */
  void lock() noexcept {
    while (flag.test_and_set(std::memory_order_acquire)) {
    }
  }

  /**
   * @brief Releases the spinlock with `std::memory_order_release`.
   */
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

  /**
   * @brief Acquires the spinlock, spinning in `Shared (S)` without `pause`.
   */
  void lock() noexcept {
    for (;;) {
      bool expected = false;
      if (locked.compare_exchange_weak(expected, true,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
        return;
      }
      while (locked.load(std::memory_order_relaxed)) {
      }
    }
  }

  /**
   * @brief Releases the spinlock with `std::memory_order_release`.
   */
  void unlock() noexcept { locked.store(false, std::memory_order_release); }
};

/**
 * @struct TTASPauseLock
 * @brief Hardware-sympathetic Test-and-Test-and-Set (`TTAS`) spinlock pairing
 * `Shared (S)` read polling with `_mm_pause()` (`F3 90` / `rep nop`).
 */
struct alignas(kCacheLineBytes) TTASPauseLock {
  std::atomic<bool> locked{false};

  /**
   * @brief Acquires the spinlock, spinning in `Shared (S)` with `_mm_pause()`.
   */
  void lock() noexcept {
    for (;;) {
      bool expected = false;
      if (locked.compare_exchange_weak(expected, true,
                                       std::memory_order_acquire,
                                       std::memory_order_relaxed)) {
        return;
      }
      while (locked.load(std::memory_order_relaxed)) {
        _mm_pause();
      }
    }
  }

  /**
   * @brief Releases the spinlock with `std::memory_order_release`.
   */
  void unlock() noexcept { locked.store(false, std::memory_order_release); }
};

/**
 * @struct StdMutexWrapper
 * @brief Thin wrapper around `std::mutex` (`pthread_mutex_t`) aligned to 64B.
 */
struct alignas(kCacheLineBytes) StdMutexWrapper {
  std::mutex mtx;

  /**
   * @brief Acquires the underlying `std::mutex`.
   */
  void lock() noexcept { mtx.lock(); }

  /**
   * @brief Releases the underlying `std::mutex`.
   */
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

  /**
   * @brief Acquires the 3-state Linux futex.
   */
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

  /**
   * @brief Releases the 3-state Linux futex, waking a sleeper only if state was
   * 2.
   */
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

  /**
   * @brief Acquires the hybrid spin+futex mutex.
   */
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

  /**
   * @brief Releases the hybrid spin+futex mutex.
   */
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

  /**
   * @brief Acquires the C++20 adaptive atomic::wait lock.
   */
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

  /**
   * @brief Releases the C++20 adaptive atomic::wait lock.
   */
  void unlock() noexcept {
    if (state.exchange(0, std::memory_order_release) == 2) {
      state.notify_one();
    }
  }
};

}  // namespace hotpath::locks_under_the_hood

#endif  // HOTPATH_COMMON_LOCKS_UNDER_THE_HOOD_LOCKS_UNDER_THE_HOOD_H_
