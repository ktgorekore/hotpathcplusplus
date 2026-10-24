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
 * @file locks_under_the_hood_benchmark.cc
 * @brief Google Benchmark suite for Episode 10 (`common/locks_under_the_hood`).
 */

#include <linux/futex.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>

#include "benchmark/benchmark.h"
#include "common/locks_under_the_hood/locks_under_the_hood.h"

namespace hotpath::locks_under_the_hood {
namespace {

template <typename LockType>
void BM_Lock_Uncontended(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(0);
  LockType lock;
  ProtectedState shared;

  for (auto _ : state) {
    lock.lock();
    shared.UpdateInvariants(1);
    benchmark::ClobberMemory();
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
struct alignas(128) ContendedArena {
  LockType lock;
  ProtectedState shared;
};

template <typename LockType>
ContendedArena<LockType> g_arena;

inline void PerformLocalThreadWork(std::uint64_t seed) noexcept {
  std::uint64_t acc = seed ^ 0x9E3779B97F4A7C15ULL;
  for (int i = 0; i < 28; ++i) {
    acc = (acc ^ (acc >> 30)) * 0xBF58476D1CE4E5B9ULL;
    benchmark::DoNotOptimize(acc);
  }
}

template <typename LockType>
void BM_Lock_Contended(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCCD0(state.thread_index());
  auto& arena = g_arena<LockType>;
  std::uint64_t local_seq = static_cast<std::uint64_t>(state.thread_index());

  for (auto _ : state) {
    arena.lock.lock();
    arena.shared.UpdateInvariants(1);
    benchmark::ClobberMemory();
    arena.lock.unlock();
    PerformLocalThreadWork(++local_seq);
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

template <typename LockType>
void BM_Spinlock_OversubscriptionCliff(benchmark::State& state) {
  EnsureMultiThreadedRuntime();
  PinThreadToCore(state.thread_index() % 16);
  auto& arena = g_arena<LockType>;

  for (auto _ : state) {
    arena.lock.lock();
    arena.shared.UpdateInvariants(1);
    if ((arena.shared.sequence & 31ULL) == 0ULL) {
      sched_yield();
    }
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

}  // namespace
}  // namespace hotpath::locks_under_the_hood
