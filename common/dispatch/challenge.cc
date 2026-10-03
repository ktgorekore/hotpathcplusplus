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
 * @brief Hot Path C++ Episode 06 Community Challenge: The Accidental Copy Trap.
 *
 * MISSION FOR HOT PATH C++ AUDITORS:
 * This benchmark processes market data instrument feeds using std::variant.
 * However, the visitor in BM_Challenge_AccidentalCopy contains a subtle
 * performance bug: it captures the parameter by-value ('auto item') instead of
 * by-reference ('const auto& item')!
 *
 * Because each payload carries symbol identifiers and pricing fields, this
 * accidental copy turns an ultra-fast L1 cache traversal into thousands of
 * unnecessary stack spills and memcpys.
 *
 * CAN YOU FIX IT?
 * 1. Audit the visitor lambda signature in BM_Challenge_AccidentalCopy.
 * 2. Change 'auto item' to 'const auto& item'.
 * 3. Run: bazel run -c opt //common/dispatch:challenge
 * 4. Measure the throughput and latency delta!
 */

#include <array>
#include <cstdint>
#include <random>
#include <variant>
#include <vector>

#include "benchmark/benchmark.h"

namespace hotpath::dispatch {
namespace {

constexpr size_t kBatchSize = 100'000;

struct StockPayload {
  uint64_t id;
  std::array<char, 16> symbol;
  double price;
  uint32_t volume;

  double Process() const noexcept { return price * volume; }
};

struct BondPayload {
  uint64_t id;
  std::array<char, 16> isin;
  double face_value;
  double coupon_rate;

  double Process() const noexcept { return face_value * (1.0 + coupon_rate); }
};

struct OptionPayload {
  uint64_t id;
  std::array<char, 16> underlying;
  double strike;
  double premium;
  bool is_call;

  double Process() const noexcept { return strike + premium; }
};

using FeedVariant = std::variant<StockPayload, BondPayload, OptionPayload>;

std::vector<FeedVariant> CreateTestFeed() {
  std::vector<FeedVariant> feed;
  feed.reserve(kBatchSize);

  std::mt19937_64 rng(42);
  std::uniform_int_distribution<int> type_dist(0, 2);

  for (size_t i = 0; i < kBatchSize; ++i) {
    int type = type_dist(rng);
    if (type == 0) {
      StockPayload s{i + 1, {"AAPL"}, 180.50, 100};
      feed.push_back(s);
    } else if (type == 1) {
      BondPayload b{i + 1, {"US10Y"}, 1000.0, 0.045};
      feed.push_back(b);
    } else {
      OptionPayload o{i + 1, {"AAPL240621C"}, 185.0, 4.25, true};
      feed.push_back(o);
    }
  }
  return feed;
}

const std::vector<FeedVariant>& GetFeed() {
  static const auto kFeed = CreateTestFeed();
  return kFeed;
}

// ============================================================================
// Buggy Implementation: Accidental Value Copy in std::visit
// ============================================================================

/**
 * @brief Buggy visitor taking variant elements by value.
 *
 * Forces an expensive copy of the entire 40+ byte payload onto the stack
 * on every visit invocation.
 *
 * @param state Google Benchmark state.
 */
void BM_Challenge_AccidentalCopy(benchmark::State& state) {
  const auto& feed = GetFeed();

  for (auto _ : state) {
    double total = 0.0;
    for (const auto& item : feed) {
      // BUG: 'auto instrument' forces an expensive value copy of the struct!
      total += std::visit(
          [](auto instrument) noexcept { return instrument.Process(); }, item);
    }
    benchmark::DoNotOptimize(total);
  }

  state.SetItemsProcessed(state.iterations() * feed.size());
  state.SetBytesProcessed(state.iterations() * feed.size() *
                          sizeof(FeedVariant));
}
BENCHMARK(BM_Challenge_AccidentalCopy);

// ============================================================================
// Fixed Implementation: Const Reference Visitor
// ============================================================================

/**
 * @brief Optimized visitor passing variant elements by const reference.
 *
 * Direct zero-copy traversal within cache lines.
 *
 * @param state Google Benchmark state.
 */
void BM_Challenge_Fixed_ConstRef(benchmark::State& state) {
  const auto& feed = GetFeed();

  for (auto _ : state) {
    double total = 0.0;
    for (const auto& item : feed) {
      // FIX: 'const auto& instrument' avoids copying, passing directly by
      // reference!
      total += std::visit(
          [](const auto& instrument) noexcept { return instrument.Process(); },
          item);
    }
    benchmark::DoNotOptimize(total);
  }

  state.SetItemsProcessed(state.iterations() * feed.size());
  state.SetBytesProcessed(state.iterations() * feed.size() *
                          sizeof(FeedVariant));
}
BENCHMARK(BM_Challenge_Fixed_ConstRef);

}  // namespace
}  // namespace hotpath::dispatch
