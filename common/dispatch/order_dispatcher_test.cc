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

#include "common/dispatch/order_dispatcher.h"

#include <memory>
#include <variant>
#include <vector>

#include "common/dispatch/instrument_feed.h"
#include "gtest/gtest.h"

namespace hotpath::dispatch {
namespace {

TEST(OrderDispatcherTest, OOPExecutionMatchesDataOrientedDesign) {
  constexpr double kMarketPrice = 150.0;

  // 1. Buy Limit: Limit > Market -> Fills
  auto oop_buy = std::make_unique<BuyLimitOrder>(1, 155.0, 100);
  OrderVariant dod_buy = BuyLimit{1, 155.0, 100};
  EXPECT_DOUBLE_EQ(oop_buy->Execute(kMarketPrice), 155.0 * 100);
  EXPECT_DOUBLE_EQ(ExecuteVariant(dod_buy, kMarketPrice), 155.0 * 100);

  // 2. Buy Limit: Limit < Market -> Does not fill
  auto oop_buy_unfilled = std::make_unique<BuyLimitOrder>(2, 145.0, 100);
  OrderVariant dod_buy_unfilled = BuyLimit{2, 145.0, 100};
  EXPECT_DOUBLE_EQ(oop_buy_unfilled->Execute(kMarketPrice), 0.0);
  EXPECT_DOUBLE_EQ(ExecuteVariant(dod_buy_unfilled, kMarketPrice), 0.0);

  // 3. Sell Market: Fills at market price
  auto oop_sell = std::make_unique<SellMarketOrder>(3, 50);
  OrderVariant dod_sell = SellMarket{3, 50};
  EXPECT_DOUBLE_EQ(oop_sell->Execute(kMarketPrice), 150.0 * 50);
  EXPECT_DOUBLE_EQ(ExecuteVariant(dod_sell, kMarketPrice), 150.0 * 50);

  // 4. Cancel Order: 0 volume
  auto oop_cancel = std::make_unique<CancelOrder>(4);
  OrderVariant dod_cancel = Cancel{4};
  EXPECT_DOUBLE_EQ(oop_cancel->Execute(kMarketPrice), 0.0);
  EXPECT_DOUBLE_EQ(ExecuteVariant(dod_cancel, kMarketPrice), 0.0);
}

TEST(OrderDispatcherTest, InstrumentFeedExecutionMatches) {
  // Stock
  auto oop_stock = std::make_unique<Stock>(1, 180.50, 100);
  InstrumentVariant dod_stock = StockPOD{1, 180.50, 100};
  EXPECT_DOUBLE_EQ(oop_stock->Process(), 180.50 * 100);
  EXPECT_DOUBLE_EQ(ProcessInstrument(dod_stock), 180.50 * 100);

  // Bond
  auto oop_bond = std::make_unique<Bond>(2, 1000.0, 0.05);
  InstrumentVariant dod_bond = BondPOD{2, 1000.0, 0.05};
  EXPECT_DOUBLE_EQ(oop_bond->Process(), 1000.0 * 1.05);
  EXPECT_DOUBLE_EQ(ProcessInstrument(dod_bond), 1000.0 * 1.05);

  // Option
  auto oop_option = std::make_unique<Option>(3, 185.0, 4.5, true);
  InstrumentVariant dod_option = OptionPOD{3, 185.0, 4.5, true};
  EXPECT_DOUBLE_EQ(oop_option->Process(), 185.0 + 4.5);
  EXPECT_DOUBLE_EQ(ProcessInstrument(dod_option), 185.0 + 4.5);
}

TEST(OrderDispatcherTest, MemoryLayoutAndSizing) {
  // BuyLimit POD has 8 bytes id + 8 bytes limit_price + 4 bytes qty + 4 padding
  // = 24 bytes
  EXPECT_LE(sizeof(BuyLimit), 24ULL);

  // OrderVariant contains maximum type size + discriminator index byte +
  // padding
  EXPECT_LE(sizeof(OrderVariant), 32ULL);

  // Verify alignment is natural 8-byte aligned for cache lines
  EXPECT_EQ(alignof(OrderVariant), 8ULL);
}

}  // namespace
}  // namespace hotpath::dispatch
