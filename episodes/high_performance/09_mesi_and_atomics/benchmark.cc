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
 * @brief Episode 09 benchmark wrapper delegating to common/mesi_coherency.
 */

#include "benchmark/benchmark.h"

#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "common/mesi_coherency/mesi_coherency.h"

namespace hotpath::episode09 {
namespace {

using hotpath::mesi_coherency::CacheLineAlignedAtomic;
using hotpath::mesi_coherency::NaiveCasSpinlock;
using hotpath::mesi_coherency::ReadBeforeCasSpinlock;
using hotpath::mesi_coherency::SharedOrderBookLedger;

inline void PinThreadToCore(int core_id) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

CacheLineAlignedAtomic g_shared_line;

void BM_AtomicLoad_SharedScaling(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  if (state.thread_index() == 0) {
    g_shared_line.value.store(42, std::memory_order_relaxed);
  }
  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      std::uint64_t v = g_shared_line.value.load(std::memory_order_relaxed);
      benchmark::DoNotOptimize(v);
    }
  }
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_AtomicLoad_SharedScaling)->ThreadRange(1, 16)->UseRealTime();

void BM_AtomicRMW_TopologyPingPong(benchmark::State& state) {
  const int partner_core = static_cast<int>(state.range(0));
  PinThreadToCore(state.thread_index() == 0 ? 0 : partner_core);

  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      std::uint64_t prev =
          g_shared_line.value.fetch_add(1, std::memory_order_relaxed);
      benchmark::DoNotOptimize(prev);
    }
  }
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_AtomicRMW_TopologyPingPong)
    ->Threads(2)
    ->Arg(1)
    ->Arg(8)
    ->UseRealTime();

void BM_AtomicRMW_FetchAdd(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      std::uint64_t prev =
          g_shared_line.value.fetch_add(1, std::memory_order_relaxed);
      benchmark::DoNotOptimize(prev);
    }
  }
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_AtomicRMW_FetchAdd)->Threads(1)->Threads(16)->UseRealTime();

NaiveCasSpinlock g_naive_lock;
ReadBeforeCasSpinlock g_ttas_lock;
SharedOrderBookLedger g_shared_ledger;

void BM_CASLoop_Naive(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  for (auto _ : state) {
    g_naive_lock.Lock();
    g_shared_ledger.RecordExecution(10, 100);
    g_naive_lock.Unlock();
  }
  benchmark::DoNotOptimize(g_shared_ledger);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CASLoop_Naive)->Threads(16)->UseRealTime();

void BM_CASLoop_ReadBeforeCAS(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  for (auto _ : state) {
    g_ttas_lock.Lock();
    g_shared_ledger.RecordExecution(10, 100);
    g_ttas_lock.Unlock();
  }
  benchmark::DoNotOptimize(g_shared_ledger);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CASLoop_ReadBeforeCAS)->Threads(16)->UseRealTime();

alignas(128) std::uint8_t g_raw_arena[256];

void BM_SplitLock_CacheLineBoundary(benchmark::State& state) {
  PinThreadToCore(0);
  const std::size_t byte_offset = static_cast<std::size_t>(state.range(0));
  auto* target_atomic =
      new (g_raw_arena + byte_offset) std::atomic<std::uint64_t>{0};

  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      std::uint64_t prev =
          target_atomic->fetch_add(1, std::memory_order_relaxed);
      benchmark::DoNotOptimize(prev);
    }
  }
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_SplitLock_CacheLineBoundary)
    ->Arg(0)
    ->Arg(56)
    ->Arg(60)
    ->UseRealTime();

}  // namespace
}  // namespace hotpath::episode09
