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
 * @brief Hot Path C++ Episode 05 Community Measurement Challenge.
 *
 * This benchmark contains 3 subtle measurement bugs frequently encountered
 * in microbenchmarking production code:
 *
 * TRAP 1 (Dead Code Elimination):
 * The compiler observes that the accumulator `sum` is never observed or
 * leaked outside the loop scope. Under the ISO C++ "as-if" rule, the entire
 * calculation is deleted at -O3.
 *
 * TRAP 2 (Setup Overhead in the Measurement Loop):
 * Allocating a vector or dynamic buffer inside `for (auto _ : state)` pollutes
 * hot-path timing with OS page faults, `mmap` calls, and malloc mutexes.
 *
 * TRAP 3 (Synthetic Predictability / Zero Entropy):
 * Evaluating a branch against a repetitive or predictable pattern teaches the
 * hardware TAGE / BTB branch predictor to achieve near-100% accuracy,
 * masking real-world branch misprediction penalties.
 *
 * Can you identify and patch all three traps?
 */

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include "common/measurement/entropy_generator.h"
#include "common/measurement/measurement_harness.h"

namespace cognitas::trading {

// ============================================================================
// CHALLENGE 1: THE INVISIBLE WORKLOAD (DEAD CODE TRAP)
// ============================================================================

/**
 * @brief Challenge 1: Find why this benchmark reports 0.00 ns on -O3.
 *
 * BUG: The compiler computes nothing because the result is dead code!
 * FIX: Use `benchmark::DoNotOptimize(sum)` or `benchmark::ClobberMemory()`.
 *
 * @param state Benchmark state object.
 */
static void BM_Challenge_Trap1_DeadCode(benchmark::State& state) {
  for (auto _ : state) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 500; ++i) {
      sum += (i * 17) ^ (i >> 2);
    }
    // Bug: `sum` is never escaped. The compiler eliminates the inner loop!
  }
}
BENCHMARK(BM_Challenge_Trap1_DeadCode);

/**
 * @brief Reference Fix for Challenge 1.
 *
 * @param state Benchmark state object.
 */
static void BM_Challenge_Trap1_Fixed(benchmark::State& state) {
  for (auto _ : state) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 500; ++i) {
      sum += (i * 17) ^ (i >> 2);
    }
    benchmark::DoNotOptimize(sum);
  }
}
BENCHMARK(BM_Challenge_Trap1_Fixed);

// ============================================================================
// CHALLENGE 2: MEASURING THE ALLOCATOR (SETUP OVERHEAD TRAP)
// ============================================================================

/**
 * @brief Challenge 2: Find why this compute benchmark measures heap latency.
 *
 * BUG: `std::vector` dynamic allocation and deallocation occur inside the timed
 * loop on every iteration, dominating the CPU compute latency.
 * FIX: Allocate and populate outside the loop, or use `state.PauseTiming()`.
 *
 * @param state Benchmark state object.
 */
static void BM_Challenge_Trap2_SetupPollution(benchmark::State& state) {
  for (auto _ : state) {
    // Bug: Dynamic memory allocation inside the hot benchmark loop!
    std::vector<int64_t> buffer(1024);
    for (size_t i = 0; i < buffer.size(); ++i) {
      buffer[i] = static_cast<int64_t>(i * 3);
    }
    int64_t acc = std::accumulate(buffer.begin(), buffer.end(), int64_t{0});
    benchmark::DoNotOptimize(acc);
  }
}
BENCHMARK(BM_Challenge_Trap2_SetupPollution);

/**
 * @brief Reference Fix for Challenge 2.
 *
 * @param state Benchmark state object.
 */
static void BM_Challenge_Trap2_Fixed(benchmark::State& state) {
  std::vector<int64_t> buffer(1024);
  for (size_t i = 0; i < buffer.size(); ++i) {
    buffer[i] = static_cast<int64_t>(i * 3);
  }
  for (auto _ : state) {
    int64_t acc = std::accumulate(buffer.begin(), buffer.end(), int64_t{0});
    benchmark::DoNotOptimize(acc);
  }
}
BENCHMARK(BM_Challenge_Trap2_Fixed);

// ============================================================================
// CHALLENGE 3: THE ALL-KNOWING CPU (PREDICTOR TRAP)
// ============================================================================

constexpr size_t kDataSize = 8192;

/**
 * @brief Challenge 3: Find why this branch looks free until production.
 *
 * BUG: Alternating pattern [1, -1, 1, -1] is trivially learned by the branch
 * predictor history table, showing 0% miss rate and unrealistic IPC.
 * FIX: Use high-entropy pseudo-random data to measure real branch penalty.
 *
 * @param state Benchmark state object.
 */
__attribute__((optimize("no-tree-vectorize"))) static void
BM_Challenge_Trap3_PredictablePattern(benchmark::State& state) {
  std::vector<int64_t> pattern(kDataSize);
  for (size_t i = 0; i < kDataSize; ++i) {
    pattern[i] = (i % 2 == 0) ? 1 : -1;
  }

  for (auto _ : state) {
    int64_t count = 0;
    for (size_t i = 0; i < kDataSize; ++i) {
      if (pattern[i] > 0) {
        count += pattern[i];
      }
    }
    benchmark::DoNotOptimize(count);
  }
}
BENCHMARK(BM_Challenge_Trap3_PredictablePattern);

/**
 * @brief Reference Fix for Challenge 3: High entropy branch evaluation.
 *
 * @param state Benchmark state object.
 */
__attribute__((optimize("no-tree-vectorize"))) static void
BM_Challenge_Trap3_FixedWithEntropy(benchmark::State& state) {
  const auto random_data = DatasetGenerator::GenerateUniformRandom(kDataSize);

  for (auto _ : state) {
    int64_t count = 0;
    for (size_t i = 0; i < kDataSize; ++i) {
      if (random_data[i] > 0) {
        count += random_data[i];
      }
    }
    benchmark::DoNotOptimize(count);
  }
}
BENCHMARK(BM_Challenge_Trap3_FixedWithEntropy);

}  // namespace cognitas::trading
