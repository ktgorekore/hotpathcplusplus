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
 * @file measurement_benchmark.cc
 * @brief Empirical Microbenchmarking & Silicon Measurement Suite.
 *
 * Demonstrates the 4 classic microbenchmarking pitfalls on modern x86-64
 * architectures:
 * 1. Dead Code Elimination: Compiler deleting loops under the ISO "as-if" rule.
 * 2. Branch Predictor Omniscience: TAGE branch predictor learning synthetic
 * patterns vs entropy.
 * 3. Memory Hierarchy Illusion: L1 cache residency (1ns) vs DRAM latency
 * (65ns).
 * 4. Timer Overhead: std::chrono vs serialized RDTSC instruction latency.
 */

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstdint>
#include <numeric>
#include <vector>

#include "common/measurement/entropy_generator.h"
#include "common/measurement/measurement_harness.h"

namespace cognitas::trading {

// ============================================================================
// PART 1: THE COMPILER TRAP — DEAD CODE ELIMINATION
// ============================================================================

/**
 * @brief Naive benchmark where loop results are never observed.
 *
 * Under -O3, the compiler detects no observable side effects and eliminates
 * the loop entirely into an instant return (`0.00 ns` reported).
 */
static void BM_DeadCode_Eliminated(benchmark::State& state) {
  for (auto _ : state) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 1000; ++i) {
      sum += (i * 31) ^ (i >> 3);
    }
    // Result discarded: compiler deletes computation under as-if rule!
  }
}
BENCHMARK(BM_DeadCode_Eliminated);

/**
 * @brief Forcing value materialization with benchmark::DoNotOptimize.
 *
 * Informs the compiler that `sum` must be preserved in a register or memory,
 * reflecting true computation latency.
 */
static void BM_DeadCode_DoNotOptimize(benchmark::State& state) {
  for (auto _ : state) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 1000; ++i) {
      sum += (i * 31) ^ (i >> 3);
    }
    benchmark::DoNotOptimize(sum);
  }
}
BENCHMARK(BM_DeadCode_DoNotOptimize);

/**
 * @brief Forcing value materialization with ForceOptimizationBarrier inline
 * assembly.
 */
static void BM_DeadCode_ForceBarrier(benchmark::State& state) {
  for (auto _ : state) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 1000; ++i) {
      sum += (i * 31) ^ (i >> 3);
    }
    ForceOptimizationBarrier(sum);
  }
}
BENCHMARK(BM_DeadCode_ForceBarrier);

// ============================================================================
// PART 2: THE CPU TRAP — BRANCH PREDICTOR OMNISCIENCE
// ============================================================================

constexpr size_t kBranchArraySize = 16384;

/**
 * @brief Predictable Branch: Sorted data allows >99% BTB prediction accuracy.
 *
 * Decorated with no-tree-vectorize to preserve the conditional branch
 * instruction on x86-64, measuring actual branch predictor performance.
 */
__attribute__((optimize("no-tree-vectorize"))) static void
BM_Branch_PredictableSorted(benchmark::State& state) {
  const auto data = DatasetGenerator::GenerateSorted(kBranchArraySize);
  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < kBranchArraySize; ++i) {
      if (data[i] > 0) {
        sum += data[i];
      }
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kBranchArraySize);
}
BENCHMARK(BM_Branch_PredictableSorted);

/**
 * @brief Unpredictable Branch: Random data triggers ~50% branch mispredictions.
 *
 * Each misprediction causes a 15-20 cycle pipeline flush on modern x86-64 CPUs.
 */
__attribute__((optimize("no-tree-vectorize"))) static void
BM_Branch_UnpredictableRandom(benchmark::State& state) {
  const auto data = DatasetGenerator::GenerateUniformRandom(kBranchArraySize);
  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < kBranchArraySize; ++i) {
      if (data[i] > 0) {
        sum += data[i];
      }
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kBranchArraySize);
}
BENCHMARK(BM_Branch_UnpredictableRandom);

/**
 * @brief Branchless Execution: Compiles to conditional move (`cmovg`).
 *
 * Latency is deterministic and immune to input entropy.
 */
__attribute__((optimize("no-tree-vectorize"))) static void
BM_Branch_BranchlessCmov(benchmark::State& state) {
  const auto data = DatasetGenerator::GenerateUniformRandom(kBranchArraySize);
  for (auto _ : state) {
    int64_t sum = 0;
    for (size_t i = 0; i < kBranchArraySize; ++i) {
      sum += (data[i] > 0 ? data[i] : 0);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kBranchArraySize);
}
BENCHMARK(BM_Branch_BranchlessCmov);

// ============================================================================
// PART 3: THE MEMORY TRAP — CACHE HIERARCHY LATENCY
// ============================================================================

/**
 * @brief Pointer chasing through a random cyclic ring of arbitrary memory size.
 */
static void RunPointerChase(benchmark::State& state, size_t buffer_bytes) {
  const auto ring =
      DatasetGenerator::GeneratePointerChaseRing(buffer_bytes, true);
  uint32_t curr = 0;

  for (auto _ : state) {
    for (int step = 0; step < 100; ++step) {
      curr = ring[curr];
    }
    benchmark::DoNotOptimize(curr);
  }
  state.SetItemsProcessed(state.iterations() * 100);
}

// 16 KiB fits inside L1 Data Cache (typically 32-48 KiB)
static void BM_Cache_L1Resident(benchmark::State& state) {
  RunPointerChase(state, 16 * 1024);
}
BENCHMARK(BM_Cache_L1Resident);

// 256 KiB fits inside L2 Cache (typically 512-1024 KiB)
static void BM_Cache_L2Resident(benchmark::State& state) {
  RunPointerChase(state, 256 * 1024);
}
BENCHMARK(BM_Cache_L2Resident);

// 8 MiB fits inside L3 Cache (typically 16-64 MiB)
static void BM_Cache_L3Resident(benchmark::State& state) {
  RunPointerChase(state, 8 * 1024 * 1024);
}
BENCHMARK(BM_Cache_L3Resident);

// 64 MiB spills to DRAM (Main Memory Wall)
static void BM_Cache_DRAM(benchmark::State& state) {
  RunPointerChase(state, 64 * 1024 * 1024);
}
BENCHMARK(BM_Cache_DRAM);

// ============================================================================
// PART 4: TIMER OVERHEAD & RESOLUTION
// ============================================================================

/**
 * @brief Measures overhead of std::chrono::high_resolution_clock query.
 */
static void BM_TimerOverhead_StdChrono(benchmark::State& state) {
  for (auto _ : state) {
    auto now = std::chrono::high_resolution_clock::now();
    benchmark::DoNotOptimize(now);
  }
}
BENCHMARK(BM_TimerOverhead_StdChrono);

/**
 * @brief Measures overhead of serialized RDTSCP instruction query.
 */
static void BM_TimerOverhead_RDTSCP(benchmark::State& state) {
  for (auto _ : state) {
    uint64_t tsc = ReadTscSerialized();
    benchmark::DoNotOptimize(tsc);
  }
}
BENCHMARK(BM_TimerOverhead_RDTSCP);

// ============================================================================
// PART 5: COMMON MEASUREMENT PITFALLS & CHALLENGES
// ============================================================================

/**
 * @brief Setup Overhead Trap: Dynamic memory allocation inside the hot loop.
 */
static void BM_Trap_SetupAllocationPollution(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int64_t> buffer(1024);
    for (size_t i = 0; i < buffer.size(); ++i) {
      buffer[i] = static_cast<int64_t>(i * 3);
    }
    int64_t acc = std::accumulate(buffer.begin(), buffer.end(), int64_t{0});
    benchmark::DoNotOptimize(acc);
  }
}
BENCHMARK(BM_Trap_SetupAllocationPollution);

/**
 * @brief Setup Overhead Fix: Buffer allocated and pre-warmed outside the loop.
 */
static void BM_Trap_SetupPreAllocated(benchmark::State& state) {
  std::vector<int64_t> buffer(1024);
  for (size_t i = 0; i < buffer.size(); ++i) {
    buffer[i] = static_cast<int64_t>(i * 3);
  }
  for (auto _ : state) {
    int64_t acc = std::accumulate(buffer.begin(), buffer.end(), int64_t{0});
    benchmark::DoNotOptimize(acc);
  }
}
BENCHMARK(BM_Trap_SetupPreAllocated);

}  // namespace cognitas::trading
