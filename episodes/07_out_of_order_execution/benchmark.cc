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
 * @brief Hot Path C++ — Episode 07 Benchmark: Out-of-Order Execution & ILP.
 *
 * Demonstrates:
 * 1. Loop-carried data dependency chains that serialize the execution pipeline.
 * 2. Multi-accumulator independent instruction streams that allow Out-of-Order
 *    execution units to achieve maximum Instructions Per Cycle (IPC > 3.5).
 */

#include "benchmark/benchmark.h"

#include <cstdint>
#include <random>
#include <vector>

namespace hotpath::episode07 {
namespace {

constexpr size_t kDataSize = 65'536;

/**
 * @brief Generates pseudo-random 64-bit unsigned integer data.
 * @return Vector of 64-bit random values.
 */
std::vector<uint64_t> CreateData() {
  std::vector<uint64_t> data(kDataSize);
  std::mt19937_64 rng(42);
  for (auto &x : data) {
    x = rng();
  }
  return data;
}

/**
 * @brief Retrieves cached static 64-bit integer benchmark data.
 * @return Const reference to static dataset.
 */
const std::vector<uint64_t> &GetData() {
  static const auto kData = CreateData();
  return kData;
}

/**
 * @brief Generates pseudo-random double precision floating-point data.
 * @return Vector of random double values.
 */
std::vector<double> CreateFloatData() {
  std::vector<double> data(kDataSize);
  std::mt19937_64 rng(1337);
  std::uniform_real_distribution<double> dist(0.1, 100.0);
  for (auto &x : data) {
    x = dist(rng);
  }
  return data;
}

/**
 * @brief Retrieves cached static double precision floating-point benchmark
 * data.
 * @return Const reference to static float dataset.
 */
const std::vector<double> &GetFloatData() {
  static const auto kData = CreateFloatData();
  return kData;
}

/**
 * @brief Serialized dependency chain: each iteration depends on the previous
 * result (RAW hazard).
 * @param state Benchmark runner state.
 */
void BM_DependentChain(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t acc = 0;
    for (size_t i = 0; i < data.size(); ++i) {
      acc = (acc * 33) ^ data[i];
    }
    benchmark::DoNotOptimize(acc);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_DependentChain);

/**
 * @brief Independent multi-pipeline accumulation: allows out-of-order execution
 * to dispatch 4 operations concurrently across superscalar execution ports.
 * @param state Benchmark runner state.
 */
void BM_IndependentPipelines(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;
    for (size_t i = 0; i < data.size(); i += 4) {
      acc0 += data[i];
      acc1 += data[i + 1];
      acc2 += data[i + 2];
      acc3 += data[i + 3];
    }
    uint64_t total = (acc0 + acc1) + (acc2 + acc3);
    benchmark::DoNotOptimize(total);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_IndependentPipelines);

/**
 * @brief Independent 8-way multi-pipeline accumulation: saturates wide
 * superscalar execution engines (e.g. AMD Zen 5 integer ALU ports).
 * @param state Benchmark runner state.
 */
void BM_IndependentPipelines8Way(benchmark::State &state) {
  const auto &data = GetData();

  for (auto _ : state) {
    uint64_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;
    uint64_t acc4 = 0, acc5 = 0, acc6 = 0, acc7 = 0;
    for (size_t i = 0; i < data.size(); i += 8) {
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
    benchmark::DoNotOptimize(total);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_IndependentPipelines8Way);

/**
 * @brief Floating-point scalar accumulation (IEEE 754 serialized dependency).
 * @param state Benchmark runner state.
 */
void BM_DependentFloatSum(benchmark::State &state) {
  const auto &data = GetFloatData();

  for (auto _ : state) {
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
      sum += data[i];
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_DependentFloatSum);

/**
 * @brief 4-way independent floating-point accumulation unlocking ILP.
 * @param state Benchmark runner state.
 */
void BM_IndependentFloatSum4Way(benchmark::State &state) {
  const auto &data = GetFloatData();

  for (auto _ : state) {
    double acc0 = 0.0, acc1 = 0.0, acc2 = 0.0, acc3 = 0.0;
    for (size_t i = 0; i < data.size(); i += 4) {
      acc0 += data[i];
      acc1 += data[i + 1];
      acc2 += data[i + 2];
      acc3 += data[i + 3];
    }
    double total = (acc0 + acc1) + (acc2 + acc3);
    benchmark::DoNotOptimize(total);
  }

  state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_IndependentFloatSum4Way);

}  // namespace
}  // namespace hotpath::episode07
