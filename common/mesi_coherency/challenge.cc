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
 * @brief Hot Path C++ — Episode 09 Production Retrofit Verification Suite.
 *
 * Verifies the 3 production-realistic `HttpRouter` fixes on 16 CPU cores:
 * 1. Telemetry Counter & Request Trace IDs (`fetch_add(1)` RFO storm vs
 *    `fetch_add(64)` Batch Leasing vs `alignas(64)` `ShardedCounter<64>`).
 * 2. Lock-Free Buffer Pool Freelist (`compare_exchange_weak` RFO storm vs
 *    `_mm_pause()` Backoff vs `tcmalloc`-style Batch-32 Thread Magazine).
 * 3. Zero-Copy RPC Wire Header Parsing (`#pragma pack(1)` Byte Offset 60 Split
 *    Lock vs 8-Byte Aligned L1D Cache Lock).
 */

#include <immintrin.h>
#include <pthread.h>
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

#include "common/mesi_coherency/mesi_coherency.h"

namespace hotpath::mesi_coherency {
namespace {

constexpr int kHwThreads = 16;
constexpr std::int64_t kCounterItersPerThread = 500'000;
constexpr std::int64_t kCasItersPerThread = 50'000;
constexpr std::size_t kSplitLockOps = 500'000;

/**
 * @brief Pins the calling thread to a specific physical CPU core ID.
 * @param core_id Physical core index.
 */
inline void PinThreadToCore(int core_id) noexcept {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id % kHwThreads, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/**
 * @struct BenchResult
 * @brief Captures both effective aggregate ns/op (wall_ns / total_ops) and
 * per-thread latency (wall_ns / iters_per_thread).
 */
struct BenchResult {
  double aggregate_ns_per_op = 0.0;
  double per_thread_ns_per_op = 0.0;
  double duration_ms = 0.0;
};

template <typename Fn>
BenchResult RunParallelBench(int num_threads, std::int64_t iters_per_thread,
                             Fn&& fn) {
  std::barrier sync_barrier(num_threads);
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  const auto t0 = std::chrono::steady_clock::now();
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      PinThreadToCore(t);
      sync_barrier.arrive_and_wait();
      fn(t, iters_per_thread);
    });
  }
  for (auto& th : threads) {
    th.join();
  }
  const auto t1 = std::chrono::steady_clock::now();

  const double total_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count();
  const double total_ops =
      static_cast<double>(num_threads) * static_cast<double>(iters_per_thread);
  return {total_ns / total_ops,
          total_ns / static_cast<double>(iters_per_thread), total_ns / 1e6};
}

}  // namespace
}  // namespace hotpath::mesi_coherency

int main() {
  using namespace hotpath::mesi_coherency;

  std::cout
      << "===================================================================="
         "\n"
      << " HOT PATH C++ — EPISODE 09 PRODUCTION SERVER RETROFIT (16 Cores)\n"
      << "===================================================================="
         "\n\n";
  std::cout << std::fixed << std::setprecision(2);

  // --- PART 1: HTTP Telemetry Counter & Trace ID Leasing ---
  CacheLineAlignedAtomic global_counter;
  const auto res_1t =
      RunParallelBench(1, kCounterItersPerThread, [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          global_counter.value.fetch_add(1, std::memory_order_relaxed);
        }
      });

  global_counter.value.store(0, std::memory_order_relaxed);
  const auto res_16t_global = RunParallelBench(
      kHwThreads, kCounterItersPerThread, [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          global_counter.value.fetch_add(1, std::memory_order_relaxed);
        }
      });

  BatchedIdGenerator<64> batched_gen;
  const auto res_16t_batched = RunParallelBench(
      kHwThreads, kCounterItersPerThread, [&](int, std::int64_t iters) {
        BatchedIdGenerator<64>::ThreadLease lease{};
        for (std::int64_t i = 0; i < iters; ++i) {
          const std::uint64_t id = batched_gen.NextId(lease);
          asm volatile("" : : "r"(id) : "memory");
        }
      });

  ShardedCounter<64> sharded_counter;
  const auto res_16t_sharded = RunParallelBench(
      kHwThreads, kCounterItersPerThread, [&](int tid, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          sharded_counter.Increment(static_cast<std::size_t>(tid), 1);
        }
      });

  std::cout << "[Part 1] HTTP Telemetry Counter & Trace ID Scaling (16 Cores)\n"
            << "  1. Global fetch_add(1) (1 Thread Local M-state) : "
            << res_1t.per_thread_ns_per_op << " ns/op\n"
            << "  2. Global fetch_add(1) (16 Threads RFO Storm)   : "
            << res_16t_global.per_thread_ns_per_op << " ns/op (per-thread) | "
            << res_16t_global.aggregate_ns_per_op << " ns/op (aggregate)\n"
            << "  3. Batched ID Lease fetch_add(64) (16 Threads)  : "
            << res_16t_batched.per_thread_ns_per_op << " ns/op (per-thread) | "
            << res_16t_batched.aggregate_ns_per_op << " ns/op (aggregate)\n"
            << "  4. alignas(64) Per-Core ShardedCounter (16T)    : "
            << res_16t_sharded.per_thread_ns_per_op << " ns/op (per-thread) | "
            << res_16t_sharded.aggregate_ns_per_op << " ns/op (aggregate)\n\n";

  // --- PART 2: Lock-Free Buffer Pool Freelist vs tcmalloc Magazine ---
  NaiveCasSpinlock naive_cas;
  SharedOrderBookLedger naive_ledger{};
  const auto res_cas_1t =
      RunParallelBench(1, kCasItersPerThread, [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          naive_cas.Lock();
          naive_ledger.RecordExecution(1, 10);
          naive_cas.Unlock();
        }
      });

  naive_ledger = SharedOrderBookLedger{};
  const auto res_cas_16t = RunParallelBench(
      kHwThreads, kCasItersPerThread, [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          naive_cas.Lock();
          naive_ledger.RecordExecution(1, 10);
          naive_cas.Unlock();
        }
      });

  ReadBeforeCasSpinlock pause_cas;
  SharedOrderBookLedger pause_ledger{};
  const auto res_pause_16t = RunParallelBench(
      kHwThreads, kCasItersPerThread, [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          pause_cas.Lock();
          pause_ledger.RecordExecution(1, 10);
          pause_cas.Unlock();
        }
      });

  SharedOrderBookLedger mag_ledger{};
  const auto res_mag_16t = RunParallelBench(
      kHwThreads, kCounterItersPerThread, [&](int, std::int64_t iters) {
        std::uint64_t local_mag = 0;
        for (std::int64_t i = 0; i < iters; ++i) {
          if (local_mag == 0) [[unlikely]] {
            pause_cas.Lock();
            mag_ledger.RecordExecution(32, 10);
            pause_cas.Unlock();
            local_mag = 32;
          }
          --local_mag;
          asm volatile("" : "+r"(local_mag) : : "memory");
        }
      });

  std::cout << "[Part 2] Lock-Free Buffer Pool Freelist vs. tcmalloc Magazine\n"
            << "  1. Uncontended CAS Pool Pop (1 Thread)          : "
            << res_cas_1t.per_thread_ns_per_op << " ns/op\n"
            << "  2. Global CAS Freelist (16T lock cmpxchg Storm) : "
            << res_cas_16t.per_thread_ns_per_op << " ns/op (per-thread) | "
            << res_cas_16t.aggregate_ns_per_op << " ns/op (aggregate)\n"
            << "  3. CAS + _mm_pause() + Load Backoff (16T)       : "
            << res_pause_16t.per_thread_ns_per_op << " ns/op (per-thread) | "
            << res_pause_16t.aggregate_ns_per_op << " ns/op (aggregate)\n"
            << "  4. Per-Thread Magazine (Batch-32 Lease, 16T)    : "
            << res_mag_16t.per_thread_ns_per_op << " ns/op (per-thread) | "
            << res_mag_16t.aggregate_ns_per_op << " ns/op (aggregate)\n\n";

  // --- PART 3: Zero-Copy RPC Wire Header Split Lock ---
  alignas(kCacheLineBytes) PackedSplitLockHeader split_hdr{};
  alignas(kCacheLineBytes) AlignedTelemetryHeader aligned_hdr{};

  const auto split_res = RunParallelBench(
      1, static_cast<std::int64_t>(kSplitLockOps),
      [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          split_hdr.seq_no.fetch_add(1, std::memory_order_relaxed);
        }
      });
  const auto aligned_res = RunParallelBench(
      1, static_cast<std::int64_t>(kSplitLockOps),
      [&](int, std::int64_t iters) {
        for (std::int64_t i = 0; i < iters; ++i) {
          aligned_hdr.seq_no.fetch_add(1, std::memory_order_relaxed);
        }
      });

  std::cout << "[Part 3] Zero-Copy RPC Wire Header Alignment (Split Lock vs "
               "Cache Lock)\n"
            << "  1. #pragma pack(1) Byte Offset 60 (Split Lock)  : "
            << split_res.per_thread_ns_per_op << " ns/op\n"
            << "  2. 8-Byte Aligned Byte Offset 0 (L1D Cache Lock): "
            << aligned_res.per_thread_ns_per_op << " ns/op ("
            << (split_res.per_thread_ns_per_op /
                aligned_res.per_thread_ns_per_op)
            << "x Faster!)\n\n";

  const bool ok =
      (sharded_counter.ReadTotal() ==
       static_cast<std::uint64_t>(kHwThreads * kCounterItersPerThread)) &&
      (batched_gen.GlobalWatermark() ==
       static_cast<std::uint64_t>(kHwThreads * kCounterItersPerThread)) &&
      (split_hdr.seq_no.load() == kSplitLockOps) &&
      (aligned_hdr.seq_no.load() == kSplitLockOps);

  std::cout << "Integrity Check: " << (ok ? "PASSED" : "FAILED") << "\n";
  return ok ? 0 : 1;
}
