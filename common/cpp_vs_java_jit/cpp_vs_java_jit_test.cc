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
 * @file cpp_vs_java_jit_test.cc
 * @brief Unit tests verifying Episode 13 C++20 primitives across Battle 1
 * (`Event` cross-TU hierarchy vs. `FlatEvent`), Battle 2 (`Order` AoS vs.
 * `OrderBookSoA`), and Battle 3 (`RunBattle3SlabTailLatencyAndDeoptCheck`).
 */

#include "common/cpp_vs_java_jit/cpp_vs_java_jit.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace hotpath::cpp_vs_java_jit {
namespace {

TEST(CppVsJavaJitTest, CrossTuPolymorphicEventsAndFlatEventsMatchAllFiveTags) {
  for (std::uint8_t tag = 0; tag < 5; ++tag) {
    const std::int64_t price = 512;
    const std::int64_t qty = 10;
    const std::uint32_t symbol_id = 42;

    auto ev = MakeEventByType(tag, price, qty, symbol_id);
    auto heap_res = ev->ProcessHeapAlloc();
    const std::int64_t heap_net = heap_res->notional - heap_res->fee_ticks;
    const std::int64_t no_alloc_net = ev->ProcessNoAlloc();

    const FlatEvent flat{price, qty, symbol_id, tag};
    const std::int64_t flat_net = flat.ProcessInline();

    EXPECT_EQ(no_alloc_net, flat_net);
    if (tag == 0 || tag == 1 || tag == 4) {
      EXPECT_EQ(heap_net, no_alloc_net);
    } else if (tag == 2) {
      const std::int64_t gross = price * (qty + 1);
      EXPECT_EQ(heap_net, gross - (gross >> 9));
    } else if (tag == 3) {
      EXPECT_EQ(heap_net, (price + 2) * qty - 1);
    }
  }
}

TEST(CppVsJavaJitTest, Battle2ContiguousAoSAndSoAComputeIdenticalExposure) {
  constexpr std::size_t kCount = 4096;
  std::vector<Order> orders(kCount);
  for (std::size_t i = 0; i < kCount; ++i) {
    orders[i] = Order{i, static_cast<std::int64_t>(1000 + (i & 1023)),
                      static_cast<std::int64_t>(1 + (i & 31)),
                      static_cast<std::uint32_t>(i & 255), kOrderFlagActiveBuy};
  }
  const OrderBookSoA soa(kCount);

  std::int64_t aos_sum = 0;
  std::int64_t soa_sum = 0;
  for (std::size_t i = 0; i < kCount; ++i) {
    if (orders[i].flags == kOrderFlagActiveBuy) {
      aos_sum += orders[i].price * orders[i].qty;
    }
    soa_sum += (soa.prices[i] * soa.qtys[i]) & soa.masks[i];
  }
  EXPECT_GT(aos_sum, 0);
  EXPECT_EQ(aos_sum, soa_sum);
}

TEST(CppVsJavaJitTest, Battle3SlabPoolMetricsAreValidAndMonotonic) {
  const Battle3TailMetrics m =
      RunBattle3SlabTailLatencyAndDeoptCheck(/*print_report=*/false);
  EXPECT_GT(m.p50_us, 0.0);
  EXPECT_GE(m.p99_us, m.p50_us);
  EXPECT_GE(m.p99_9_us, m.p99_us);
  EXPECT_GE(m.p99_99_us, m.p99_9_us);
  EXPECT_GE(m.max_us, m.p99_99_us);
  EXPECT_GE(m.rare_event_ns, 0.0);
}

}  // namespace
}  // namespace hotpath::cpp_vs_java_jit
