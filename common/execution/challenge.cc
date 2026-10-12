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
 * @brief Hot Path C++ — Episode 07 Community Performance Challenge.
 *
 * MISSION:
 * Optimize an autoregressive financial moving-average filter that suffers from
 * a loop-carried dependency chain. The naive implementation serializes ALU
 * execution to an IPC of ~1.0.
 *
 * Your goal:
 * 1. Transform the computation graph using block-level parallel formulations
 *    or multi-accumulator prefix techniques.
 * 2. Saturate all superscalar ALU ports (IPC > 3.0 on modern x86-64 / Zen 5).
 * 3. Verify zero numerical drift within float precision epsilon.
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

namespace hotpath::execution {

constexpr size_t kSampleSize = 1'000'000;

/**
 * @struct FilterResult
 * @brief Encapsulates verification checksum and timing metrics for the
 * challenge filter.
 */
struct FilterResult {
  double checksum;          ///< Total accumulated sum of filtered outputs.
  double duration_ms;       ///< Execution latency in milliseconds.
  double elements_per_sec;  ///< Processed throughput in elements per second.
};

/**
 * @brief Baseline: Naive 1st-order IIR Filter with loop-carried recurrence.
 *
 * Mathematical recurrence:
 *   y[i] = x[i] + alpha * y[i - 1]
 *
 * This loop contains a true Read-After-Write (RAW) data dependency chain that
 * serializes instruction dispatch to IPC ~ 1.0.
 *
 * @param x Input sequence of double-precision floating-point samples.
 * @param alpha Feedback decay coefficient.
 * @return FilterResult containing the checksum and duration metrics.
 */
FilterResult RunBaseline(const std::vector<double> &x, double alpha) {
  const size_t n = x.size();
  std::vector<double> y(n, 0.0);

  auto start = std::chrono::high_resolution_clock::now();

  double prev = 0.0;
  for (size_t i = 0; i < n; ++i) {
    prev = x[i] + alpha * prev;
    y[i] = prev;
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end - start;

  double checksum = 0.0;
  for (double val : y) {
    checksum += val;
  }

  return {checksum, elapsed.count(),
          static_cast<double>(n) / (elapsed.count() / 1000.0)};
}

/**
 * @brief Optimized Challenge Submission:
 * Parallel 4-way block-lookahead IIR implementation.
 *
 * Mathematical expansion over 4 steps:
 *   y0 = x0 + alpha * y_prev
 *   y1 = x1 + alpha * x0 + alpha^2 * y_prev
 *   y2 = x2 + alpha * x1 + alpha^2 * x0 + alpha^3 * y_prev
 *   y3 = x3 + alpha * x2 + alpha^2 * x1 + alpha^3 * x0 + alpha^4 * y_prev
 *
 * Exposes independent sub-terms that modern out-of-order execution pipelines
 * can schedule across parallel superscalar ALU ports (IPC > 3.0).
 *
 * @param x Input sequence of double-precision floating-point samples.
 * @param alpha Feedback decay coefficient.
 * @return FilterResult containing the checksum and duration metrics.
 */
FilterResult RunOptimizedChallenge(const std::vector<double> &x, double alpha) {
  const size_t n = x.size();
  std::vector<double> y(n, 0.0);

  auto start = std::chrono::high_resolution_clock::now();

  const double a2 = alpha * alpha;
  const double a3 = a2 * alpha;
  const double a4 = a3 * alpha;

  double y_prev = 0.0;
  const size_t limit = n - (n % 4);

  for (size_t i = 0; i < limit; i += 4) {
    // Lookahead expansion of 4-step recurrence:
    // y0 = x0 + alpha * y_prev
    // y1 = x1 + alpha*x0 + a2*y_prev
    // y2 = x2 + alpha*x1 + a2*x0 + a3*y_prev
    // y3 = x3 + alpha*x2 + a2*x1 + a3*x0 + a4*y_prev
    const double x0 = x[i];
    const double x1 = x[i + 1];
    const double x2 = x[i + 2];
    const double x3 = x[i + 3];

    // Compute independent sub-terms across superscalar ALUs
    const double y0 = x0 + alpha * y_prev;
    const double y1 = x1 + alpha * x0 + a2 * y_prev;
    const double y2 = x2 + alpha * x1 + a2 * x0 + a3 * y_prev;
    const double y3 = x3 + alpha * x2 + a2 * x1 + a3 * x0 + a4 * y_prev;

    y[i] = y0;
    y[i + 1] = y1;
    y[i + 2] = y2;
    y[i + 3] = y3;

    y_prev = y3;
  }

  for (size_t i = limit; i < n; ++i) {
    y_prev = x[i] + alpha * y_prev;
    y[i] = y_prev;
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end - start;

  double checksum = 0.0;
  for (double val : y) {
    checksum += val;
  }

  return {checksum, elapsed.count(),
          static_cast<double>(n) / (elapsed.count() / 1000.0)};
}

}  // namespace hotpath::execution

/**
 * @brief Main entrypoint for the Episode 07 community challenge harness.
 * @return 0 if validation passes within numerical epsilon (1e-4), 1 otherwise.
 */
int main() {
  std::cout << "=========================================================\n"
            << "🔥 Hot Path C++ — Episode 07 Community Challenge 🔥\n"
            << "   Out-of-Order Execution & Recurrence Parallelization\n"
            << "=========================================================\n\n";

  std::mt19937_64 rng(42);
  std::uniform_real_distribution<double> dist(0.1, 10.0);

  std::vector<double> input_data(hotpath::execution::kSampleSize);
  for (auto &val : input_data) {
    val = dist(rng);
  }

  const double alpha = 0.85;

  std::cout << "Running Baseline Naive Recurrence (Serialized RAW Chain)...\n";
  auto baseline = hotpath::execution::RunBaseline(input_data, alpha);

  std::cout << "Running Optimized Lookahead Pipeline (Superscalar ILP)...\n";
  auto optimized = hotpath::execution::RunOptimizedChallenge(input_data, alpha);

  const double speedup = baseline.duration_ms / optimized.duration_ms;
  const double diff = std::abs(baseline.checksum - optimized.checksum);

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "\n---------------------------------------------------------\n";
  std::cout << "Baseline Duration : " << baseline.duration_ms << " ms ("
            << (baseline.elements_per_sec / 1e6) << " M items/sec)\n";
  std::cout << "Optimized Duration: " << optimized.duration_ms << " ms ("
            << (optimized.elements_per_sec / 1e6) << " M items/sec)\n";
  std::cout << "Speedup Factor    : " << speedup << "x\n";
  std::cout << "Checksum Delta    : " << diff << " (Validation "
            << (diff < 1e-4 ? "PASSED ✅" : "FAILED ❌") << ")\n";
  std::cout << "---------------------------------------------------------\n";

  return (diff < 1e-4) ? 0 : 1;
}
