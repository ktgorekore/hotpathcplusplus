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
 * @brief Episode 13 Interactive C++20 Forensic Lab & Self-Verifying Challenge
 * Runner: C++20 vs. Java 21 (HotSpot C2 JIT, Cache-Line Physics & ZGC Tail
 * Latency).
 */

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

#include "common/cpp_vs_java_jit/cpp_vs_java_jit.h"

namespace hotpath::cpp_vs_java_jit {
namespace {

/**
 * @brief Runs Battle 1 C++20 Allocation & Polymorphism benchmarks and prints
 * the forensic scoreboard.
 */
void RunBattle1Suite() {
  std::printf(
      "====================================================================\n");
  std::printf(
      "C++20 BATTLE 1: HEAP ALLOCATION vs. CROSS-TU VIRTUAL vs. FLAT VALUE\n");
  std::printf(
      "====================================================================\n");

  constexpr std::size_t kMeasureOps = 20'000'000;
  std::vector<std::uint8_t> mono_tags(kBattle1BatchSize);
  std::vector<std::uint8_t> mega_tags(kBattle1BatchSize);
  std::vector<std::unique_ptr<Event>> mono_ring(kBattle1BatchSize);
  std::vector<std::unique_ptr<Event>> bimodal_ring(kBattle1BatchSize);
  std::vector<std::unique_ptr<Event>> mega_ring(kBattle1BatchSize);
  std::vector<FlatEvent> flat_mono(kBattle1BatchSize);
  std::vector<FlatEvent> flat_mega(kBattle1BatchSize);

  for (std::size_t i = 0; i < kBattle1BatchSize; ++i) {
    mono_tags[i] = (i % 1000 == 999) ? 1 : 0;
    mega_tags[i] = static_cast<std::uint8_t>(i % 5);
    const auto p = static_cast<std::int64_t>(100 + (i & 63));
    const auto q = static_cast<std::int64_t>(10 + (i & 15));

    mono_ring[i] = MakeEventByType(mono_tags[i], p, q, 42);
    bimodal_ring[i] =
        MakeEventByType(static_cast<std::uint8_t>(i & 1), p, q, 42);
    mega_ring[i] = MakeEventByType(mega_tags[i], p, q, 42);
    flat_mono[i] = FlatEvent{p, q, 42, mono_tags[i]};
    flat_mega[i] = FlatEvent{p, q, 42, mega_tags[i]};
  }

  // 1. Naive OOP C++ (Monomorphic 99.9% & Megamorphic 20%x5)
  for (int mode = 0; mode < 2; ++mode) {
    const auto& tags = (mode == 0) ? mono_tags : mega_tags;
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kMeasureOps; ++i) {
      const std::uint8_t tag = tags[i & (kBattle1BatchSize - 1)];
      auto ev = MakeEventByType(tag, static_cast<std::int64_t>(100 + (i & 63)),
                                static_cast<std::int64_t>(10 + (i & 15)), 42);
      auto res = ev->ProcessHeapAlloc();
      acc += res->notional - res->fee_ticks;
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    const double ns =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()) /
        static_cast<double>(kMeasureOps);
    std::printf("  %-58s : %6.2f ns/op (%6.1f M ops/s)\n",
                (mode == 0)
                    ? "1A. C++ Naive OOP (make_unique + Cross-TU, 99.9% Mono)"
                    : "1B. C++ Naive OOP (make_unique + Cross-TU, 20%x5 Mega)",
                ns, 1000.0 / ns);
  }

  // 2. Pre-allocated Cross-TU Virtual (No Alloc)
  auto bench_virtual = [&](const char* label,
                           const std::vector<std::unique_ptr<Event>>& ring) {
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kMeasureOps; ++i) {
      acc += ring[i & (kBattle1BatchSize - 1)]->ProcessNoAlloc();
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    const double ns =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()) /
        static_cast<double>(kMeasureOps);
    std::printf("  %-58s : %6.2f ns/op (%6.1f M ops/s)\n", label, ns,
                1000.0 / ns);
  };
  bench_virtual("2A. C++ Cross-TU Virtual NoAlloc (99.9% Monomorphic)",
                mono_ring);
  bench_virtual("2B. C++ Cross-TU Virtual NoAlloc (50/50 Bimodal)",
                bimodal_ring);
  bench_virtual("2C. C++ Cross-TU Virtual NoAlloc (20%x5 Megamorphic)",
                mega_ring);

  // 3. Idiomatic FlatValue
  auto bench_flat = [&](const char* label, const std::vector<FlatEvent>& ring) {
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < kMeasureOps; ++i) {
      acc += ring[i & (kBattle1BatchSize - 1)].ProcessInline();
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    const double ns =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()) /
        static_cast<double>(kMeasureOps);
    std::printf("  %-58s : %6.2f ns/op (%6.1f M ops/s)\n", label, ns,
                1000.0 / ns);
  };
  bench_flat("3A. C++20 Idiomatic FlatEvent Value (99.9% Monomorphic)",
             flat_mono);
  bench_flat("3B. C++20 Idiomatic FlatEvent Value (20%x5 Megamorphic)",
             flat_mega);
}

/**
 * @brief Runs Battle 2 C++20 Cache-Line Physics benchmarks across 4,000,000
 * active 32-byte orders.
 */
void RunBattle2Suite() {
  std::printf(
      "\n===================================================================="
      "\n");
  std::printf(
      "C++20 BATTLE 2: CACHE-LINE PHYSICS (4,000,000 32-Byte Orders)\n");
  std::printf(
      "====================================================================\n");

  constexpr int kPasses = 40;

  // 1. Churned Pointer Array
  {
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

    const auto t0 = std::chrono::steady_clock::now();
    for (int p = 0; p < kPasses; ++p) {
      std::int64_t total_exposure = 0;
      for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
        const Order* o = ptr_list[i];
        if (o->flags == kOrderFlagActiveBuy) {
          total_exposure += o->price * o->qty;
        }
      }
      DoNotOptimize(total_exposure);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()) /
        (kPasses * 1e6);
    std::printf("  %-55s : %7.3f ms / scan (%5.2f ns/order)\n",
                "1. C++ Pointer-Array Order* (Post-Churn Fragmented Heap)", ms,
                (ms * 1e6) / static_cast<double>(kBattle2OrderCount));
  }

  // 2. Contiguous std::vector<Order>
  {
    std::vector<Order> orders(kBattle2OrderCount);
    for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
      orders[i] =
          Order{i, static_cast<std::int64_t>(1000 + (i & 1023)),
                static_cast<std::int64_t>(1 + (i & 31)),
                static_cast<std::uint32_t>(i & 255), kOrderFlagActiveBuy};
    }
    const auto t0 = std::chrono::steady_clock::now();
    for (int p = 0; p < kPasses; ++p) {
      std::int64_t total_exposure = 0;
      const Order* __restrict data = orders.data();
      for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
        if (data[i].flags == kOrderFlagActiveBuy) {
          total_exposure += data[i].price * data[i].qty;
        }
      }
      DoNotOptimize(total_exposure);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()) /
        (kPasses * 1e6);
    std::printf("  %-55s : %7.3f ms / scan (%5.2f ns/order)\n",
                "2. C++20 std::vector<Order> (Contiguous 32B Structs)", ms,
                (ms * 1e6) / static_cast<double>(kBattle2OrderCount));
  }

  // 3. OrderBookSoA SIMD
  {
    const OrderBookSoA soa(kBattle2OrderCount);
    const std::int64_t* __restrict px = soa.prices.data();
    const std::int64_t* __restrict qt = soa.qtys.data();
    const std::int64_t* __restrict mk = soa.masks.data();
    const auto t0 = std::chrono::steady_clock::now();
    for (int p = 0; p < kPasses; ++p) {
      std::int64_t total_exposure = 0;
      for (std::size_t i = 0; i < kBattle2OrderCount; ++i) {
        total_exposure += (px[i] * qt[i]) & mk[i];
      }
      DoNotOptimize(total_exposure);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                .count()) /
        (kPasses * 1e6);
    std::printf("  %-55s : %7.3f ms / scan (%5.2f ns/order)\n",
                "3. C++20 OrderBookSoA (Dense SIMD Structure-of-Arrays)", ms,
                (ms * 1e6) / static_cast<double>(kBattle2OrderCount));
  }
}

}  // namespace
}  // namespace hotpath::cpp_vs_java_jit

int main(int argc, char** argv) {
  using namespace hotpath::cpp_vs_java_jit;
  PinThreadToCore(0);

  if (argc > 1) {
    const std::string_view arg(argv[1]);
    if (arg == "--run_battle3_tail" || arg == "battle3") {
      RunBattle3SlabTailLatencyAndDeoptCheck(/*print_report=*/true);
      return 0;
    }
  }

  RunBattle1Suite();
  RunBattle2Suite();
  RunBattle3SlabTailLatencyAndDeoptCheck(/*print_report=*/true);
  return 0;
}
