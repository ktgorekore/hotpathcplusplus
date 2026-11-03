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
 * @brief Episode 13 C++20 Google Benchmark & Tail-Latency Suite:
 * 1. Battle 1: Allocation & Polymorphic Dispatch (`BM_Battle1_Cpp_*`).
 * 2. Battle 2: Cache-Line Physics across 4,000,000 32-Byte Orders
 *    (`BM_Battle2_*`).
 * 3. Battle 3: Pre-Faulted Slab Pool Tail-Latency Histogram & Rare-Event AOT
 *    Check (`--run_battle3_tail` and `BM_Battle3_*`).
 */

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "common/cpp_vs_java_jit/cpp_vs_java_jit.h"

namespace hotpath::cpp_vs_java_jit {
namespace {

// ============================================================================
// BATTLE 1: ALLOCATION & POLYMORPHIC DISPATCH (C++ vs. Java C2 JIT)
// ============================================================================

/**
 * @brief 1A. Naive "Java-Style" C++: Heap allocation per event
 * (`MakeEventByType` + `ProcessHeapAlloc`) + Cross-TU `virtual` call.
 *
 * @param state Google Benchmark state (`range(0)`: `0` = Monomorphic 99.9%,
 * `1` = Megamorphic 20%x5).
 */
void BM_Battle1_Cpp_NaiveOop_HeapAndVirtual(benchmark::State& state) {
  PinThreadToCore(0);
  const std::int64_t mode = state.range(0);
  std::vector<std::uint8_t> tags(kBattle1BatchSize);
  for (std::size_t i = 0; i < kBattle1BatchSize; ++i) {
    tags[i] = (mode == 0) ? ((i % 1000 == 999) ? 1 : 0)
                          : static_cast<std::uint8_t>(i % 5);
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    const std::uint8_t tag = tags[idx++ & (kBattle1BatchSize - 1)];
    auto ev = MakeEventByType(tag, static_cast<std::int64_t>(100 + (idx & 63)),
                              static_cast<std::int64_t>(10 + (idx & 15)), 42);
    auto res = ev->ProcessHeapAlloc();
    std::int64_t net = res->notional - res->fee_ticks;
    benchmark::DoNotOptimize(net);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Battle1_Cpp_NaiveOop_HeapAndVirtual)
    ->Arg(0)   // Monomorphic (99.9% LimitOrderEvent)
    ->Arg(1);  // Megamorphic (20% each across 5 types)

/**
 * @brief 1B. Pre-Allocated Polymorphic Pointers (No `malloc` in loop; isolates
 * pure cross-TU `virtual` call cost across Monomorphic, Bimodal, and
 * Megamorphic profiles).
 *
 * @param state Google Benchmark state (`range(0)`: `0` = Monomorphic 99.9%,
 * `1` = Bimodal 50/50, `2` = Megamorphic 20%x5).
 */
void BM_Battle1_Cpp_CrossTuVirtual_NoAlloc(benchmark::State& state) {
  PinThreadToCore(0);
  const std::int64_t mode = state.range(0);
  std::vector<std::unique_ptr<Event>> ring(kBattle1BatchSize);
  for (std::size_t i = 0; i < kBattle1BatchSize; ++i) {
    std::uint8_t tag = 0;
    if (mode == 0) {
      tag = (i % 1000 == 999) ? 1 : 0;
    } else if (mode == 1) {
      tag = static_cast<std::uint8_t>(i & 1);
    } else {
      tag = static_cast<std::uint8_t>(i % 5);
    }
    ring[i] = MakeEventByType(tag, static_cast<std::int64_t>(100 + (i & 63)),
                              static_cast<std::int64_t>(10 + (i & 15)), 42);
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    const Event* ev = ring[idx++ & (kBattle1BatchSize - 1)].get();
    std::int64_t net = ev->ProcessNoAlloc();
    benchmark::DoNotOptimize(net);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Battle1_Cpp_CrossTuVirtual_NoAlloc)
    ->Arg(0)   // Monomorphic
    ->Arg(1)   // Bimodal
    ->Arg(2);  // Megamorphic

/**
 * @brief 1C. Idiomatic Hot-Path C++20: Contiguous `FlatEvent` value structs +
 * inlined dispatch (`0` heap allocations, `0` vtable calls).
 *
 * @param state Google Benchmark state (`range(0)`: `0` = Monomorphic 99.9%,
 * `1` = Megamorphic 20%x5).
 */
void BM_Battle1_Cpp_IdiomaticFlatValue(benchmark::State& state) {
  PinThreadToCore(0);
  const std::int64_t mode = state.range(0);
  std::vector<FlatEvent> ring(kBattle1BatchSize);
  for (std::size_t i = 0; i < kBattle1BatchSize; ++i) {
    const std::uint8_t tag = (mode == 0) ? ((i % 1000 == 999) ? 1 : 0)
                                         : static_cast<std::uint8_t>(i % 5);
    ring[i] = FlatEvent{static_cast<std::int64_t>(100 + (i & 63)),
                        static_cast<std::int64_t>(10 + (i & 15)), 42, tag};
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    const FlatEvent& ev = ring[idx++ & (kBattle1BatchSize - 1)];
    std::int64_t net = ev.ProcessInline();
    benchmark::DoNotOptimize(net);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Battle1_Cpp_IdiomaticFlatValue)
    ->Arg(0)   // Monomorphic
    ->Arg(1);  // Megamorphic

// ============================================================================
// BATTLE 2: CACHE-LINE PHYSICS (4,000,000 Active 32-Byte Orders)
// ============================================================================

/**
 * @brief 2A. Pointer-Array Indirection (Simulating Java `ArrayList<Order>`
 * after realistic order-book heap churn).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Battle2_PointerArray_ChurnedHeap(benchmark::State& state) {
  PinThreadToCore(0);
  std::vector<std::unique_ptr<Order>> storage(kBattle2OrderCount);
  std::vector<const Order*> ptr_list(kBattle2OrderCount);
  for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
    storage[i] = std::make_unique<Order>(
        Order{i, static_cast<std::int64_t>(1000 + (i & 1023)),
              static_cast<std::int64_t>(1 + (i & 31)),
              static_cast<std::uint32_t>(i & 255), kOrderFlagActiveBuy});
    ptr_list[i] = storage[i].get();
  }
  std::mt19937_64 rng(0xC0FFEE13ULL);
  std::shuffle(ptr_list.begin(), ptr_list.end(), rng);

  for (auto _ : state) {
    std::int64_t total_exposure = 0;
    for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
      const Order* o = ptr_list[i];
      if (o->flags == kOrderFlagActiveBuy) {
        total_exposure += o->price * o->qty;
      }
    }
    benchmark::DoNotOptimize(total_exposure);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kBattle2OrderCount));
  state.SetBytesProcessed(
      state.iterations() *
      static_cast<std::int64_t>(kBattle2OrderCount * sizeof(Order)));
}
BENCHMARK(BM_Battle2_PointerArray_ChurnedHeap)->Unit(benchmark::kMillisecond);

/**
 * @brief 2B. Contiguous `std::vector<Order>` (2 headerless 32-byte structs per
 * 64-byte L1D cache line).
 *
 * @param state Google Benchmark state controller.
 */
void BM_Battle2_StdVector_ContiguousAoS(benchmark::State& state) {
  PinThreadToCore(0);
  std::vector<Order> orders(kBattle2OrderCount);
  for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
    orders[i] = Order{i, static_cast<std::int64_t>(1000 + (i & 1023)),
                      static_cast<std::int64_t>(1 + (i & 31)),
                      static_cast<std::uint32_t>(i & 255), kOrderFlagActiveBuy};
  }

  for (auto _ : state) {
    std::int64_t total_exposure = 0;
    const Order* __restrict data = orders.data();
    for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
      if (data[i].flags == kOrderFlagActiveBuy) {
        total_exposure += data[i].price * data[i].qty;
      }
    }
    benchmark::DoNotOptimize(total_exposure);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kBattle2OrderCount));
  state.SetBytesProcessed(
      state.iterations() *
      static_cast<std::int64_t>(kBattle2OrderCount * sizeof(Order)));
}
BENCHMARK(BM_Battle2_StdVector_ContiguousAoS)->Unit(benchmark::kMillisecond);

/**
 * @brief 2C. Structure-of-Arrays (`OrderBookSoA`) aligned for AVX2 / AVX-512
 * SIMD vectorization.
 *
 * @param state Google Benchmark state controller.
 */
void BM_Battle2_SIMD_StructureOfArrays(benchmark::State& state) {
  PinThreadToCore(0);
  const OrderBookSoA soa(kBattle2OrderCount);
  const std::int64_t* __restrict px = soa.prices.data();
  const std::int64_t* __restrict qt = soa.qtys.data();
  const std::int64_t* __restrict mk = soa.masks.data();

  for (auto _ : state) {
    std::int64_t total_exposure = 0;
    for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
      total_exposure += (px[i] * qt[i]) & mk[i];
    }
    benchmark::DoNotOptimize(total_exposure);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kBattle2OrderCount));
  state.SetBytesProcessed(state.iterations() *
                          static_cast<std::int64_t>(kBattle2OrderCount * 24));
}
BENCHMARK(BM_Battle2_SIMD_StructureOfArrays)->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace hotpath::cpp_vs_java_jit

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--run_battle3_tail") {
      hotpath::cpp_vs_java_jit::RunBattle3SlabTailLatencyAndDeoptCheck(
          /*print_report=*/true);
      return 0;
    }
  }
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
