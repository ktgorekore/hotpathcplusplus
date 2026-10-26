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
 * @brief Hot Path C++ — Episode 09 Benchmark: Why 'Lock-Free' Atomics Collapse
 *        Multi-Core Servers (MESI, RFO Storms & How Envoy, Linux & `tcmalloc`
 *        Fix Them).
 *
 * Production-Realistic C++20 Benchmarks:
 * 1. BM_AtomicLoad_SharedScaling: 1 to 16 threads reading in MESI Shared (S)
 *    state with zero Request For Ownership (RFO) traffic.
 * 2. BM_AtomicRMW_TopologyPingPong: 2-core `fetch_add` RFO bounce inside the
 *    same 8-core CCD (shared 32 MiB L3) vs across the Infinity Fabric (GMI3)
 *    between CCD 0 and CCD 1.
 * 3. BM_AtomicRMW_FetchAdd vs BM_TraceId_BatchedLease64 vs
 *    BM_TelemetryCounter_ShardedAlignas64: Global `fetch_add(1)` RFO storm vs
 *    Batched ID Leasing (`fetch_add(64)`) vs `alignas(64)` Per-Core Sharded
 *    Counters (Envoy / Linux `percpu_counter` / `folly::ThreadCachedInt`).
 * 4. BM_CASLoop_Naive vs BM_CASLoop_ReadBeforeCAS vs
 *    BM_BufferPool_ThreadMagazine32: Contended `compare_exchange_weak` RFO
 *    storm vs `_mm_pause()` + relaxed load backoff vs `tcmalloc`/`mimalloc`
 *    Per-Thread Batch Magazine.
 * 5. BM_SplitLock_CacheLineBoundary: Aligned L1D Cache Lock (offsets 0 & 56) vs
 *    `#pragma pack(1)` Byte Offset 60 Hardware Split Lock.
 */

#include "benchmark/benchmark.h"

#include <immintrin.h>
#include <pthread.h>
#include <sched.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

namespace hotpath::episode09 {
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

/**
 * @struct CacheLineAtomic
 * @brief Isolated 64-byte cache line wrapper to prevent false sharing.
 */
struct alignas(64) CacheLineAtomic {
  std::atomic<std::uint64_t> value{0};
  std::uint8_t padding[64 - sizeof(std::atomic<std::uint64_t>)]{};
};

/**
 * @class ShardedCounter
 * @brief 64-byte cache-aligned per-core telemetry counter (Envoy / Linux
 * `percpu_counter`).
 */
template <std::size_t NumShards = 64>
class ShardedCounter {
 public:
  void Increment(std::size_t worker_id, std::uint64_t delta = 1) noexcept {
    shards_[worker_id & (NumShards - 1)].value.fetch_add(
        delta, std::memory_order_relaxed);
  }

 private:
  std::array<CacheLineAtomic, NumShards> shards_{};
};

CacheLineAtomic g_shared_line;
ShardedCounter<64> g_sharded_counter;
CacheLineAtomic g_global_trace_id;

/**
 * @brief 1. BM_AtomicLoad_SharedScaling
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
    ->Arg(1)  // Intra-CCD (Shared 32MB L3)
    ->Arg(8)  // Cross-CCD (Infinity Fabric GMI3 hop)
    ->UseRealTime();

/**
 * @brief 3A. Contended `fetch_add(1)` RMW scaling from 1 to 16 physical cores.
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
 * @brief 3B. Production Fix 1B: Batched Trace ID Lease (`fetch_add(64)`).
 * @param state Google Benchmark runner state.
 */
void BM_TraceId_BatchedLease64(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  std::uint64_t cur = 0;
  std::uint64_t batch_end = 0;
  for (auto _ : state) {
    for (int i = 0; i < 1024; ++i) {
      if (cur == batch_end) [[unlikely]] {
        cur = g_global_trace_id.value.fetch_add(64, std::memory_order_relaxed);
        batch_end = cur + 64;
      }
      std::uint64_t id = cur++;
      benchmark::DoNotOptimize(id);
    }
  }
  state.SetItemsProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_TraceId_BatchedLease64)->Threads(16)->UseRealTime();

/**
 * @brief 3C. Production Fix 1A: 64-Byte Cache-Aligned Per-Core Sharded Counter.
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

/**
 * @struct SharedOrderBookLedger
 * @brief Shared buffer pool / order-book state protected by CAS loop.
 */
struct alignas(64) SharedOrderBookLedger {
  std::uint64_t sequence = 0;
  std::uint64_t total_fill_qty = 0;
  std::uint64_t rolling_notional = 0;
  std::array<std::uint64_t, 5> venue_buckets{};

  void RecordExecution(std::uint64_t qty, std::uint64_t price_ticks) noexcept {
    const std::uint64_t seq = ++sequence;
    total_fill_qty += qty;
    rolling_notional += qty * price_ticks;
    for (std::size_t idx = 0; idx < venue_buckets.size(); ++idx) {
      venue_buckets[idx] =
          (venue_buckets[idx] ^ (seq + idx)) * 6364136223846793005ULL +
          1442695040888963407ULL;
    }
  }
};

CacheLineAtomic g_spin_lock;
SharedOrderBookLedger g_shared_ledger;

/**
 * @brief 4A. BM_CASLoop_Naive
 * @param state Google Benchmark runner state.
 */
void BM_CASLoop_Naive(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  for (auto _ : state) {
    std::uint64_t expected = 0;
    while (!g_spin_lock.value.compare_exchange_weak(
        expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      expected = 0;
    }
    g_shared_ledger.RecordExecution(10, 100);
    g_spin_lock.value.store(0, std::memory_order_release);
  }
  benchmark::DoNotOptimize(g_shared_ledger);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CASLoop_Naive)->Threads(1)->Threads(16)->UseRealTime();

/**
 * @brief 4B. BM_CASLoop_ReadBeforeCAS (Level 1 Fix: `_mm_pause()` + Shared S
 * load).
 * @param state Google Benchmark runner state.
 */
void BM_CASLoop_ReadBeforeCAS(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  for (auto _ : state) {
    std::uint64_t expected = 0;
    while (!g_spin_lock.value.compare_exchange_weak(
        expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      while (g_spin_lock.value.load(std::memory_order_relaxed) != 0) {
        _mm_pause();
      }
      expected = 0;
    }
    g_shared_ledger.RecordExecution(10, 100);
    g_spin_lock.value.store(0, std::memory_order_release);
  }
  benchmark::DoNotOptimize(g_shared_ledger);
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CASLoop_ReadBeforeCAS)->Threads(16)->UseRealTime();

/**
 * @brief 4C. BM_BufferPool_ThreadMagazine32 (Level 2 Fix: `tcmalloc` /
 * `mimalloc` Per-Thread Batch-32 Magazine).
 * @param state Google Benchmark runner state.
 */
void BM_BufferPool_ThreadMagazine32(benchmark::State& state) {
  PinThreadToCore(state.thread_index());
  std::uint64_t local_mag = 32;
  for (auto _ : state) {
    if (local_mag == 0) [[unlikely]] {
      std::uint64_t expected = 0;
      while (!g_spin_lock.value.compare_exchange_weak(
          expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        while (g_spin_lock.value.load(std::memory_order_relaxed) != 0) {
          _mm_pause();
        }
        expected = 0;
      }
      g_shared_ledger.RecordExecution(320, 100);
      g_spin_lock.value.store(0, std::memory_order_release);
      local_mag = 32;
    }
    --local_mag;
    benchmark::DoNotOptimize(local_mag);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BufferPool_ThreadMagazine32)->Threads(16)->UseRealTime();

alignas(128) std::uint8_t g_raw_arena[256];

/**
 * @brief 5. BM_SplitLock_CacheLineBoundary
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
}  // namespace hotpath::episode09
