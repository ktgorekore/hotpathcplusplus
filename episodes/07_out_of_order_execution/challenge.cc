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
 * Optimize an autoregressive moving-average filter suffering from a
 * loop-carried dependency chain. The naive implementation serializes ALU
 * execution to an IPC of ~1.0.
 *
 * Your goal:
 * 1. Transform the computation graph using block-level lookahead formulations.
 * 2. Saturate all superscalar ALU ports (IPC > 3.0 on modern Zen 5).
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

namespace hotpath::episode07 {

constexpr size_t kSampleSize = 1'000'000;

struct FilterResult {
  double checksum;
  double duration_ms;
  double elements_per_sec;
};

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
    const double x0 = x[i];
    const double x1 = x[i + 1];
    const double x2 = x[i + 2];
    const double x3 = x[i + 3];

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

}  // namespace hotpath::episode07

int main() {
  std::cout << "=========================================================\n"
            << "🔥 Hot Path C++ — Episode 07 Community Challenge 🔥\n"
            << "   Out-of-Order Execution & Recurrence Parallelization\n"
            << "=========================================================\n\n";

  std::mt19937_64 rng(42);
  std::uniform_real_distribution<double> dist(0.1, 10.0);

  std::vector<double> input_data(hotpath::episode07::kSampleSize);
  for (auto &val : input_data) {
    val = dist(rng);
  }

  const double alpha = 0.85;

  std::cout << "Running Baseline Naive Recurrence (Serialized RAW Chain)...\n";
  auto baseline = hotpath::episode07::RunBaseline(input_data, alpha);

  std::cout << "Running Optimized Lookahead Pipeline (Superscalar ILP)...\n";
  auto optimized = hotpath::episode07::RunOptimizedChallenge(input_data, alpha);

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
