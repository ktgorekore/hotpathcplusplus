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
 * @file dispatch_benchmark.cc
 * @brief Empirical benchmark comparing OOP Virtual Dispatch against Modern C++
 * std::variant.
 *
 * Demonstrates the silicon costs of classical object-oriented polymorphism:
 * 1. Double-pointer dereference (object pointer -> vptr -> vtable -> function
 * code).
 * 2. Cache-line fragmentation (individual heap allocations scattering data
 * across DRAM).
 * 3. Indirect branch predictor stalls (call *%rax failing to predict targets on
 * randomized streams).
 *
 * Contrasted with Data-Oriented Design (DOD):
 * 1. Contiguous flat array layout (perfect L1 data prefetching).
 * 2. Zero heap allocations per element.
 * 3. Predictable compiler-generated jump tables / jump-to-type dispatches via
 * std::visit.
 */

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "benchmark/benchmark.h"
#include "common/dispatch/instrument_feed.h"
#include "common/dispatch/order_dispatcher.h"

namespace hotpath::dispatch {
namespace {

constexpr size_t kOrderBatchSize = 100'000;
constexpr double kMarketPrice = 150.25;

enum class OrderType : uint8_t {
  kBuyLimit = 0,
  kSellMarket = 1,
  kCancel = 2,
};

struct BenchmarkData {
  std::vector<std::unique_ptr<BaseOrder>> oop_orders;
  std::vector<std::unique_ptr<BaseOrder>> oop_orders_sorted;
  std::vector<OrderVariant> dod_orders;
  std::vector<OrderVariant> dod_orders_sorted;

  std::vector<std::unique_ptr<Instrument>> oop_instruments;
  std::vector<InstrumentVariant> dod_instruments;
};

BenchmarkData CreateBenchmarkData() {
  BenchmarkData data;
  data.oop_orders.reserve(kOrderBatchSize);
  data.dod_orders.reserve(kOrderBatchSize);
  data.oop_instruments.reserve(kOrderBatchSize);
  data.dod_instruments.reserve(kOrderBatchSize);

  std::mt19937_64 rng(1337);
  std::uniform_int_distribution<int> type_dist(0, 2);
  std::uniform_real_distribution<double> price_dist(145.0, 155.0);
  std::uniform_int_distribution<uint32_t> qty_dist(10, 1000);

  // Generate randomized orders
  for (size_t i = 0; i < kOrderBatchSize; ++i) {
    auto type = static_cast<OrderType>(type_dist(rng));
    double limit = price_dist(rng);
    uint32_t qty = qty_dist(rng);
    uint64_t id = i + 1;

    switch (type) {
      case OrderType::kBuyLimit:
        data.oop_orders.push_back(
            std::make_unique<BuyLimitOrder>(id, limit, qty));
        data.dod_orders.push_back(BuyLimit{id, limit, qty});
        break;
      case OrderType::kSellMarket:
        data.oop_orders.push_back(std::make_unique<SellMarketOrder>(id, qty));
        data.dod_orders.push_back(SellMarket{id, qty});
        break;
      case OrderType::kCancel:
        data.oop_orders.push_back(std::make_unique<CancelOrder>(id));
        data.dod_orders.push_back(Cancel{id});
        break;
    }
  }

  // Generate sorted orders (to isolate branch prediction vs cache locality)
  data.dod_orders_sorted = data.dod_orders;
  std::stable_sort(data.dod_orders_sorted.begin(), data.dod_orders_sorted.end(),
                   [](const OrderVariant& a, const OrderVariant& b) {
                     return a.index() < b.index();
                   });

  data.oop_orders_sorted.reserve(kOrderBatchSize);
  for (const auto& item : data.dod_orders_sorted) {
    std::visit(
        [&data](const auto& concrete) {
          using T = std::decay_t<decltype(concrete)>;
          if constexpr (std::is_same_v<T, BuyLimit>) {
            data.oop_orders_sorted.push_back(std::make_unique<BuyLimitOrder>(
                concrete.id, concrete.limit_price, concrete.quantity));
          } else if constexpr (std::is_same_v<T, SellMarket>) {
            data.oop_orders_sorted.push_back(std::make_unique<SellMarketOrder>(
                concrete.id, concrete.quantity));
          } else if constexpr (std::is_same_v<T, Cancel>) {
            data.oop_orders_sorted.push_back(
                std::make_unique<CancelOrder>(concrete.id));
          }
        },
        item);
  }

  // Generate randomized financial instruments
  for (size_t i = 0; i < kOrderBatchSize; ++i) {
    int inst_type = type_dist(rng);
    uint64_t id = i + 1;
    if (inst_type == 0) {
      data.oop_instruments.push_back(std::make_unique<Stock>(id, 182.50, 200));
      data.dod_instruments.push_back(StockPOD{id, 182.50, 200});
    } else if (inst_type == 1) {
      data.oop_instruments.push_back(std::make_unique<Bond>(id, 1000.0, 0.048));
      data.dod_instruments.push_back(BondPOD{id, 1000.0, 0.048});
    } else {
      data.oop_instruments.push_back(
          std::make_unique<Option>(id, 185.0, 3.80, true));
      data.dod_instruments.push_back(OptionPOD{id, 185.0, 3.80, true});
    }
  }

  return data;
}

const BenchmarkData& GetGlobalData() {
  static const BenchmarkData kData = CreateBenchmarkData();
  return kData;
}

// ============================================================================
// Order Execution Benchmarks
// ============================================================================

/**
 * @brief Classical OOP Virtual Dispatch over randomized heap objects.
 *
 * Incurs L1 cache misses and indirect branch mispredictions due to dynamic
 * dispatch via vptr and heap fragmentation.
 *
 * @param state Google Benchmark state.
 */
void BM_OOP_VirtualDispatch_Randomized(benchmark::State& state) {
  const auto& orders = GetGlobalData().oop_orders;

  for (auto _ : state) {
    double total_value = 0.0;
    for (const auto& order : orders) {
      total_value += order->Execute(kMarketPrice);
    }
    benchmark::DoNotOptimize(total_value);
  }

  state.SetItemsProcessed(state.iterations() * orders.size());
  state.SetBytesProcessed(state.iterations() * orders.size() * sizeof(void*));
}
BENCHMARK(BM_OOP_VirtualDispatch_Randomized);

/**
 * @brief Classical OOP Virtual Dispatch over sorted heap objects.
 *
 * Branch prediction is predictable, but cache lines remain fragmented across
 * individual heap nodes.
 *
 * @param state Google Benchmark state.
 */
void BM_OOP_VirtualDispatch_Sorted(benchmark::State& state) {
  const auto& orders = GetGlobalData().oop_orders_sorted;

  for (auto _ : state) {
    double total_value = 0.0;
    for (const auto& order : orders) {
      total_value += order->Execute(kMarketPrice);
    }
    benchmark::DoNotOptimize(total_value);
  }

  state.SetItemsProcessed(state.iterations() * orders.size());
  state.SetBytesProcessed(state.iterations() * orders.size() * sizeof(void*));
}
BENCHMARK(BM_OOP_VirtualDispatch_Sorted);

/**
 * @brief Modern Data-Oriented std::variant over randomized contiguous array.
 *
 * Contiguous memory layout with zero pointer chasing and predictable
 * compiler-generated jump tables.
 *
 * @param state Google Benchmark state.
 */
void BM_DataOriented_Variant_Randomized(benchmark::State& state) {
  const auto& orders = GetGlobalData().dod_orders;

  for (auto _ : state) {
    double total_value = 0.0;
    for (const auto& order : orders) {
      total_value += DispatchOrder(order, kMarketPrice);
    }
    benchmark::DoNotOptimize(total_value);
  }

  state.SetItemsProcessed(state.iterations() * orders.size());
  state.SetBytesProcessed(state.iterations() * orders.size() *
                          sizeof(OrderVariant));
}
BENCHMARK(BM_DataOriented_Variant_Randomized);

/**
 * @brief Modern Data-Oriented std::variant over sorted contiguous array.
 *
 * Peak hardware throughput: contiguous cache lines + branch predictability.
 *
 * @param state Google Benchmark state.
 */
void BM_DataOriented_Variant_Sorted(benchmark::State& state) {
  const auto& orders = GetGlobalData().dod_orders_sorted;

  for (auto _ : state) {
    double total_value = 0.0;
    for (const auto& order : orders) {
      total_value += DispatchOrder(order, kMarketPrice);
    }
    benchmark::DoNotOptimize(total_value);
  }

  state.SetItemsProcessed(state.iterations() * orders.size());
  state.SetBytesProcessed(state.iterations() * orders.size() *
                          sizeof(OrderVariant));
}
BENCHMARK(BM_DataOriented_Variant_Sorted);

// ============================================================================
// Financial Instrument Feed Benchmarks
// ============================================================================

/**
 * @brief Classical OOP Virtual Dispatch over polymorphic instruments.
 *
 * @param state Google Benchmark state.
 */
void BM_Instrument_OOP_VirtualDispatch(benchmark::State& state) {
  const auto& instruments = GetGlobalData().oop_instruments;

  for (auto _ : state) {
    double total_val = 0.0;
    for (const auto& inst : instruments) {
      total_val += inst->Process();
    }
    benchmark::DoNotOptimize(total_val);
  }

  state.SetItemsProcessed(state.iterations() * instruments.size());
  state.SetBytesProcessed(state.iterations() * instruments.size() *
                          sizeof(void*));
}
BENCHMARK(BM_Instrument_OOP_VirtualDispatch);

/**
 * @brief Data-Oriented std::variant over contiguous financial instruments.
 *
 * @param state Google Benchmark state.
 */
void BM_Instrument_DataOriented_Variant(benchmark::State& state) {
  const auto& instruments = GetGlobalData().dod_instruments;

  for (auto _ : state) {
    double total_val = 0.0;
    for (const auto& inst : instruments) {
      total_val += ProcessInstrument(inst);
    }
    benchmark::DoNotOptimize(total_val);
  }

  state.SetItemsProcessed(state.iterations() * instruments.size());
  state.SetBytesProcessed(state.iterations() * instruments.size() *
                          sizeof(InstrumentVariant));
}
BENCHMARK(BM_Instrument_DataOriented_Variant);

}  // namespace
}  // namespace hotpath::dispatch
