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
 * @file execution_benchmark.cc
 * @brief Hot Path C++ — Episode 07 Benchmark: Out-of-Order Execution & ILP.
 *
 * Demonstrates:
 * 1. Loop-carried data dependency chains that serialize the execution pipeline.
 * 2. Multi-accumulator independent instruction streams that allow Out-of-Order
 *    execution units to achieve maximum Instructions Per Cycle (IPC > 3.5).
 */

#include <cstdint>
#include <random>
#include <vector>

#include "benchmark/benchmark.h"
#include "common/execution/execution_pipeline.h"

namespace hotpath::execution {
namespace {

constexpr size_t kDataSize = 65'536;

std::vector<uint64_t> CreateData() {
  std::vector<uint64_t> data(kDataSize);
  std::mt19937_64 rng(42);
  for (auto &x : data) {
    x = rng();
  }
  return data;
}

const std::vector<uint64_t> &GetData() {
  static const auto kData = CreateData();
  return kData;
}

std::vector<double> CreateFloatData() {
  std::vector<double> data(kDataSize);
  std::mt19937_64 rng(1337);
  std::uniform_real_distribution<double> dist(0.1, 100.0);
  for (auto &x : data) {
    x = dist(rng);
  }
  return data;
}

const std::vector<double> &GetFloatData() {
  static const auto kData = CreateFloatData();
  return kData;
}

void BM_DependentChain(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t result = DependentPipeline::AccumulateHash(data);
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_DependentChain);

void BM_DependentSum(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t result = DependentPipeline::Sum(data);
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_DependentSum);

void BM_IndependentSum4Way(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t result = IndependentPipeline::Sum4Way(data);
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_IndependentSum4Way);

void BM_IndependentSum8Way(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t result = IndependentPipeline::Sum8Way(data);
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_IndependentSum8Way);

void BM_DependentFloatSum(benchmark::State &state) {
  const auto &data = GetFloatData();

  for (auto _ : state) {
    double result = DependentPipeline::SumFloat(data);
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_DependentFloatSum);

void BM_IndependentFloatSum4Way(benchmark::State &state) {
  const auto &data = GetFloatData();

  for (auto _ : state) {
    double result = IndependentPipeline::SumFloat4Way(data);
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_IndependentFloatSum4Way);

} // namespace
} // namespace hotpath::execution
