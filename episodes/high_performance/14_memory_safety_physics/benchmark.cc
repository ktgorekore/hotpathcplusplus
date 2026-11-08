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
 * @brief Episode 14 Google Benchmark suite measuring Spatial Bounds Checking
 * (Sequential Scan, Independent Batch Gather, and Dependent Probe Chain) and
 * Temporal Memory Safety (`OrderSlabPool` 64-bit Generational Handle vs.
 * Unchecked Slot Index vs. `std::weak_ptr::lock()`) across `65,536` 32-byte
 * `Order` records.
 */

#include <benchmark/benchmark.h>

#include <cstdint>
#include <span>

#include "common/memory_safety/memory_safety.h"

namespace hotpath::memory_safety {
namespace {

/**
 * @brief Returns a process-wide initialized `SpatialDataset` (`65,536` orders).
 * @return Const reference to the singleton `SpatialDataset`.
 */
const SpatialDataset& GetSpatialDataset() {
  static const SpatialDataset* const kDataset =
      new SpatialDataset(kDefaultBookCapacity);
  return *kDataset;
}

/**
 * @brief Returns a process-wide initialized `TemporalChurnScenario` (`65,536`
 * orders with 25% cancellation and ABA slot recycling).
 * @return Const reference to the singleton `TemporalChurnScenario`.
 */
const TemporalChurnScenario& GetTemporalScenario() {
  static const TemporalChurnScenario* const kScenario =
      new TemporalChurnScenario(kDefaultBookCapacity);
  return *kScenario;
}

// ============================================================================
// BATTLE 1A: SEQUENTIAL SCAN (COMPILE-TIME RANGE CHECK ELIMINATION)
// ============================================================================

/**
 * @brief Measures sequential scan over 65,536 32B orders using raw pointer
 * indexing (`orders[i]`).
 * @param state Google Benchmark state.
 */
void BM_Battle1A_Sequential_UncheckedPtr(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum =
        ScanSequentialUnchecked(ds.orders.data(), ds.orders.size());
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.orders.size()));
}
BENCHMARK(BM_Battle1A_Sequential_UncheckedPtr);

/**
 * @brief Measures sequential scan with explicit hardened trap (`if (i >= n)
 * __builtin_trap()`), eliminated by compiler induction-variable range analysis.
 * @param state Google Benchmark state.
 */
void BM_Battle1A_Sequential_HardenedTrap(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = ScanSequentialHardenedTrap(ds.orders);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.orders.size()));
}
BENCHMARK(BM_Battle1A_Sequential_HardenedTrap);

/**
 * @brief Measures sequential scan via C++20 `std::span<const Order>`.
 * @param state Google Benchmark state.
 */
void BM_Battle1A_Sequential_StdSpan(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  const std::span<const Order> view(ds.orders);
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = ScanSequentialSpan(view);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.orders.size()));
}
BENCHMARK(BM_Battle1A_Sequential_StdSpan);

/**
 * @brief Measures sequential scan via exception-throwing `std::vector::at(i)`.
 * @param state Google Benchmark state.
 */
void BM_Battle1A_Sequential_VectorAt(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = ScanSequentialVectorAt(ds.orders);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.orders.size()));
}
BENCHMARK(BM_Battle1A_Sequential_VectorAt);

// ============================================================================
// BATTLE 1B: INDEPENDENT RANDOM BATCH GATHER (SIMD vs. IN-LOOP TRAP)
// ============================================================================

/**
 * @brief Measures independent random batch gather (`orders[indices[i]]`) with
 * unchecked indexing (auto-vectorizes with AVX-512 `vpgatherqq`).
 * @param state Google Benchmark state.
 */
void BM_Battle1B_BatchGather_Unchecked_AVX512(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = GatherBatchUnchecked(
        ds.orders.data(), ds.random_idx.data(), ds.random_idx.size());
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1B_BatchGather_Unchecked_AVX512);

/**
 * @brief Measures independent random batch gather with hardened fast trap
 * (`if (idx >= n) __builtin_trap()`), which inhibits speculative `vpgatherqq`.
 * @param state Google Benchmark state.
 */
void BM_Battle1B_BatchGather_HardenedTrap_Scalar(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = GatherBatchHardenedTrap(ds.orders, ds.random_idx);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1B_BatchGather_HardenedTrap_Scalar);

/**
 * @brief Measures independent random batch gather using `std::span` under
 * standard library bounds assertions (`-D_GLIBCXX_ASSERTIONS`).
 * @param state Google Benchmark state.
 */
void BM_Battle1B_BatchGather_StdSpan_Checked(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  const std::span<const Order> orders_span(ds.orders);
  const std::span<const std::uint32_t> idx_span(ds.random_idx);
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = GatherBatchSpan(orders_span, idx_span);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1B_BatchGather_StdSpan_Checked);

/**
 * @brief Measures independent random batch gather using `std::vector::at()`.
 * @param state Google Benchmark state.
 */
void BM_Battle1B_BatchGather_VectorAt_Exception(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = GatherBatchVectorAt(ds.orders, ds.random_idx);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1B_BatchGather_VectorAt_Exception);

/**
 * @brief Measures independent random batch gather using branchless power-of-two
 * bitmasking (`orders[indices[i] & 0xFFFF]`), guaranteeing 100% spatial safety
 * while preserving full AVX-512 `vpgatherqq` auto-vectorization.
 * @param state Google Benchmark state.
 */
void BM_Battle1B_BatchGather_BitmaskSafe_AVX512(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = GatherBatchBitmaskSafe(
        ds.orders.data(), ds.random_idx.data(), ds.random_idx.size());
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1B_BatchGather_BitmaskSafe_AVX512);

// ============================================================================
// BATTLE 1C: LATENCY-BOUND DEPENDENT PROBE CHAIN (SERIAL DATA DEPENDENCY)
// ============================================================================

/**
 * @brief Measures dependent order-book probe chain with unchecked indexing.
 * @param state Google Benchmark state.
 */
void BM_Battle1C_DependentProbe_Unchecked(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = ProbeDependentUnchecked(
        ds.orders.data(), ds.random_idx.data(), ds.random_idx.size());
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1C_DependentProbe_Unchecked);

/**
 * @brief Measures dependent order-book probe chain with hardened fast trap
 * (`cmp + jae ud2`), isolating the pure hardware branch-predictor overhead.
 * @param state Google Benchmark state.
 */
void BM_Battle1C_DependentProbe_HardenedTrap(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = ProbeDependentHardenedTrap(ds.orders, ds.random_idx);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1C_DependentProbe_HardenedTrap);

/**
 * @brief Measures dependent order-book probe chain using `std::vector::at()`.
 * @param state Google Benchmark state.
 */
void BM_Battle1C_DependentProbe_VectorAt(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& ds = GetSpatialDataset();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = ProbeDependentVectorAt(ds.orders, ds.random_idx);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.random_idx.size()));
}
BENCHMARK(BM_Battle1C_DependentProbe_VectorAt);

// ============================================================================
// BATTLE 3: TEMPORAL SAFETY UNDER 25% ORDER CANCELLATION & ABA RECYCLING
// ============================================================================

/**
 * @brief Measures unchecked slot lookup (`pool.GetUncheckedSlot(h.index)`),
 * which suffers 25% silent ABA data corruption on recycled order slots.
 * @param state Google Benchmark state.
 */
void BM_Battle3_Temporal_UncheckedSlot_ABACorrupt(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = GetTemporalScenario();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = RunTemporalUncheckedSlotHotLoop(
        scenario.pool, scenario.original_handles, scenario.lookup_order);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(scenario.lookup_order.size()));
}
BENCHMARK(BM_Battle3_Temporal_UncheckedSlot_ABACorrupt);

/**
 * @brief Measures safe 64-bit generational handle lookup
 * (`pool.Get(OrderHandle)`), catching 100% of ABA recycled slots in O(1).
 * @param state Google Benchmark state.
 */
void BM_Battle3_Temporal_GenerationalHandle_Safe(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = GetTemporalScenario();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = RunTemporalGenerationalHotLoop(
        scenario.pool, scenario.original_handles, scenario.lookup_order);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(scenario.lookup_order.size()));
}
BENCHMARK(BM_Battle3_Temporal_GenerationalHandle_Safe);

/**
 * @brief Measures reference-counted `std::weak_ptr<Order>::lock()` under 25%
 * order cancellation churn.
 * @param state Google Benchmark state.
 */
void BM_Battle3_Temporal_SharedWeakPtr_Lock(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = GetTemporalScenario();
  for (auto _ : state) {
    ClobberMemory();
    std::int64_t sum = RunTemporalWeakPtrHotLoop(scenario.original_weak_handles,
                                                 scenario.lookup_order);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(scenario.lookup_order.size()));
}
BENCHMARK(BM_Battle3_Temporal_SharedWeakPtr_Lock);

}  // namespace
}  // namespace hotpath::memory_safety
