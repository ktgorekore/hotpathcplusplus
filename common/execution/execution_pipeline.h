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

#ifndef HOTPATH_COMMON_EXECUTION_EXECUTION_PIPELINE_H_
#define HOTPATH_COMMON_EXECUTION_EXECUTION_PIPELINE_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hotpath::execution {

/**
 * @brief Demonstrates a serialized, loop-carried data dependency chain.
 *
 * Each iteration requires the result of the previous iteration to calculate
 * the next accumulator state: acc_{i+1} = f(acc_i, data[i]).
 * This starves superscalar execution units, forcing a wide CPU (e.g. 4 to 8
 * ALUs) to execute at an Instructions Per Cycle (IPC) close to 1.0.
 */
class DependentPipeline {
 public:
  /**
   * @brief Calculates a non-associative hash accumulation over a data span.
   * @param data The input stream of 64-bit integers.
   * @return The accumulated scalar result.
   */
  static uint64_t AccumulateHash(std::span<const uint64_t> data) noexcept {
    uint64_t acc = 0;
    for (size_t i = 0; i < data.size(); ++i) {
      acc = (acc * 33) ^ data[i];
    }
    return acc;
  }

  /**
   * @brief Sums 64-bit integers with a single scalar accumulator.
   * @param data The input stream.
   * @return Total sum.
   */
  static uint64_t Sum(std::span<const uint64_t> data) noexcept {
    uint64_t sum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
      sum += data[i];
    }
    return sum;
  }

  /**
   * @brief Sums double-precision floating-point numbers with a single
   * accumulator. Note: Compilers cannot reorder floating-point additions
   * without -ffast-math due to IEEE 754 non-associativity: (a + b) + c != a +
   * (b + c).
   * @param data The input floating-point stream.
   * @return Total sum.
   */
  static double SumFloat(std::span<const double> data) noexcept {
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
      sum += data[i];
    }
    return sum;
  }
};

/**
 * @brief Multi-accumulator pipeline breaking loop-carried dependency chains.
 *
 * By maintaining N independent accumulators (e.g., 4 or 8), the hardware's
 * Out-of-Order execution engine and superscalar issue logic can dispatch
 * operations concurrently across parallel ALU ports, raising IPC toward
 * hardware limits (IPC > 3.5).
 */
class IndependentPipeline {
 public:
  /**
   * @brief 4-way independent multi-accumulator summation.
   * @param data Input 64-bit integer stream.
   * @return Combined sum.
   */
  static uint64_t Sum4Way(std::span<const uint64_t> data) noexcept {
    uint64_t acc0 = 0;
    uint64_t acc1 = 0;
    uint64_t acc2 = 0;
    uint64_t acc3 = 0;

    const size_t size = data.size();
    const size_t unrolled_limit = size - (size % 4);

    for (size_t i = 0; i < unrolled_limit; i += 4) {
      acc0 += data[i];
      acc1 += data[i + 1];
      acc2 += data[i + 2];
      acc3 += data[i + 3];
    }

    uint64_t total = (acc0 + acc1) + (acc2 + acc3);
    for (size_t i = unrolled_limit; i < size; ++i) {
      total += data[i];
    }
    return total;
  }

  /**
   * @brief 8-way independent multi-accumulator summation (saturating Zen 5
   * width).
   * @param data Input 64-bit integer stream.
   * @return Combined sum.
   */
  static uint64_t Sum8Way(std::span<const uint64_t> data) noexcept {
    uint64_t acc0 = 0;
    uint64_t acc1 = 0;
    uint64_t acc2 = 0;
    uint64_t acc3 = 0;
    uint64_t acc4 = 0;
    uint64_t acc5 = 0;
    uint64_t acc6 = 0;
    uint64_t acc7 = 0;

    const size_t size = data.size();
    const size_t unrolled_limit = size - (size % 8);

    for (size_t i = 0; i < unrolled_limit; i += 8) {
      acc0 += data[i];
      acc1 += data[i + 1];
      acc2 += data[i + 2];
      acc3 += data[i + 3];
      acc4 += data[i + 4];
      acc5 += data[i + 5];
      acc6 += data[i + 6];
      acc7 += data[i + 7];
    }

    uint64_t total =
        ((acc0 + acc1) + (acc2 + acc3)) + ((acc4 + acc5) + (acc6 + acc7));
    for (size_t i = unrolled_limit; i < size; ++i) {
      total += data[i];
    }
    return total;
  }

  /**
   * @brief 4-way independent floating-point accumulation.
   * Unlocks Instruction-Level Parallelism on floating point pipelines without
   * requiring global -ffast-math compilation flags.
   * @param data Input double stream.
   * @return Combined sum.
   */
  static double SumFloat4Way(std::span<const double> data) noexcept {
    double acc0 = 0.0;
    double acc1 = 0.0;
    double acc2 = 0.0;
    double acc3 = 0.0;

    const size_t size = data.size();
    const size_t unrolled_limit = size - (size % 4);

    for (size_t i = 0; i < unrolled_limit; i += 4) {
      acc0 += data[i];
      acc1 += data[i + 1];
      acc2 += data[i + 2];
      acc3 += data[i + 3];
    }

    double total = (acc0 + acc1) + (acc2 + acc3);
    for (size_t i = unrolled_limit; i < size; ++i) {
      total += data[i];
    }
    return total;
  }
};

}  // namespace hotpath::execution

#endif  // HOTPATH_COMMON_EXECUTION_EXECUTION_PIPELINE_H_
