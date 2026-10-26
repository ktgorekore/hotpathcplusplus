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
 * @file mesi_coherency_benchmark.cc
 * @brief Google Benchmark suite for Episode 09: Production-Realistic MESI
 * Cache Coherency, Sharded Telemetry Counters, Batched ID Leasing,
 * `tcmalloc`-style Thread Magazines, and `#pragma pack(1)` Split Locks.
 */

#include <immintrin.h>
#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "benchmark/benchmark.h"
#include "common/mesi_coherency/mesi_coherency.h"

namespace hotpath::mesi_coherency {
namespace {

/**
 * @brief Pins the calling thread to a specific physical CPU core ID.
 * @param core_id Physical core index (0..15 on Ryzen 9 9950X).
 */
inline void PinThreadToCore(int core_id) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id % 16, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

CacheLineAlignedAtomic g_shared_line;
ShardedCounter<64> g_sharded_counter;
BatchedIdGenerator<64> g_batched_id_gen;

/**
 * @brief 1. BM_AtomicLoad_SharedScaling
 *
 * Demonstrates 1 to 16 threads reading a shared variable in the MESI Shared (S)
 * state with zero Request For Ownership (RFO) broadcasts.
 *
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief 2. BM_AtomicRMW_TopologyPingPong
 *
 * Arg(1) = Core 0 & Core 1 (Same L3 CCD0 on Ryzen 9 9950X)
 * Arg(8) = Core 0 (CCD0) & Core 8 (CCD1 across Infinity Fabric)
 *
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief 3A. Contended global `fetch_add(1)` RMW scaling (1T vs 16T RFO storm).
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief 3B. Production Fix 1B: Batched ID Lease (`fetch_add(64)`).
 * @param state Google Benchmark runner state.
 */
void BM_TraceId_BatchedLease64(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  BatchedIdGenerator<64>::ThreadLease lease{};
  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      std::uint64_t id = g_batched_id_gen.NextId(lease);
      benchmark::DoNotOptimize(id);
    }
  }
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_TraceId_BatchedLease64)->Threads(16)->UseRealTime();

/**
 * @brief 3C. Production Fix 1A: 64B-Aligned Per-Core `ShardedCounter<64>`
 * (Envoy / Linux `percpu_counter` / `folly::ThreadCachedInt`).
 * @param state Google Benchmark runner state.
 */
void BM_TelemetryCounter_ShardedAlignas64(benchmark::State& state) {
  const std::size_t tid = static_cast<std::size_t>(state.thread_index());
  PinThreadToCore(state.thread_index());
  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      g_sharded_counter.Increment(tid, 1);
    }
  }
  benchmark::DoNotOptimize(g_sharded_counter);
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_TelemetryCounter_ShardedAlignas64)->Threads(16)->UseRealTime();

NaiveCasSpinlock g_naive_lock;
ReadBeforeCasSpinlock g_ttas_lock;
SharedOrderBookLedger g_shared_ledger;

/**
 * @brief 4A. BM_CASLoop_Naive (16-Thread `compare_exchange_weak` RFO storm).
 * @param state Google Benchmark runner state.
 */
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
BENCHMARK(BM_CASLoop_Naive)->Threads(1)->Threads(16)->UseRealTime();

/**
 * @brief 4B. BM_CASLoop_ReadBeforeCAS (Level 1 Fix: `_mm_pause()` + Shared S
 * load backoff).
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief 4C. BM_BufferPool_ThreadMagazine32 (Level 2 Fix: `tcmalloc` /
 * `mimalloc`-style Per-Thread Magazine transferring batches of 32 buffers).
 * @param state Google Benchmark runner state.
 */
void BM_BufferPool_ThreadMagazine32(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  std::uint64_t local_mag_count = 32;
  for (auto _ : state) {
    if (local_mag_count == 0) [[unlikely]] {
      // Refill 32 buffers in a single central batch CAS
      g_ttas_lock.Lock();
      g_shared_ledger.RecordExecution(320, 100);
      g_ttas_lock.Unlock();
      local_mag_count = 32;
    }
    --local_mag_count;
    benchmark::DoNotOptimize(local_mag_count);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BufferPool_ThreadMagazine32)->Threads(16)->UseRealTime();

alignas(128) std::uint8_t g_raw_arena[256];

/**
 * @brief 5. BM_SplitLock_CacheLineBoundary
 * - Arg(0)  = Byte Offset 0  (Aligned inside 1 cache line -> L1D Cache Lock)
 * - Arg(56) = Byte Offset 56 (Aligned at end of cache line -> L1D Cache Lock)
 * - Arg(60) = Byte Offset 60 (Straddles two 64B cache lines -> Global Bus Lock)
 * @param state Google Benchmark runner state.
 */
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
}  // namespace hotpath::mesi_coherency
