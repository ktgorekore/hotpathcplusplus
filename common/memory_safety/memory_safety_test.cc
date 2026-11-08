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
 * @file memory_safety_test.cc
 * @brief GoogleTest verification suite for Episode 14 Spatial and Temporal
 * Memory Safety primitives (`memory_safety.h`).
 */

#include "common/memory_safety/memory_safety.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace hotpath::memory_safety {
namespace {

TEST(MemorySafetyTest, StructSizesAndCacheLineAlignment) {
  EXPECT_EQ(sizeof(Order), 32U);
  EXPECT_EQ(alignof(Order), 32U);
  EXPECT_EQ(sizeof(AuditEntry), 32U);
  EXPECT_EQ(alignof(AuditEntry), 32U);
  EXPECT_EQ(sizeof(OrderHandle), 8U);
  EXPECT_EQ(sizeof(OrderSlabSlot), 64U);
  EXPECT_EQ(alignof(OrderSlabSlot), 64U);
}

TEST(MemorySafetyTest, GenerationalSlabPoolRejectsStaleAndRecycledHandles) {
  OrderSlabPool pool(4);
  EXPECT_EQ(pool.Capacity(), 4U);
  EXPECT_EQ(pool.ActiveCount(), 0U);

  const std::optional<OrderHandle> h0 = pool.Allocate(Order{
      .order_id = 101ULL,
      .price_ticks = 5000LL,
      .quantity = 10U,
      .symbol_id = 1U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });
  ASSERT_TRUE(h0.has_value());
  EXPECT_EQ(h0->index, 0U);
  EXPECT_EQ(h0->generation, 1U);
  EXPECT_EQ(pool.ActiveCount(), 1U);

  const Order* live0 = pool.Get(*h0);
  ASSERT_NE(live0, nullptr);
  EXPECT_EQ(live0->order_id, 101ULL);
  EXPECT_EQ(live0->price_ticks, 5000LL);

  // Release slot 0 and verify double-free returns false.
  EXPECT_TRUE(pool.Release(*h0));
  EXPECT_FALSE(pool.Release(*h0));
  EXPECT_EQ(pool.Get(*h0), nullptr);

  // Immediately allocate a replacement order into recycled slot 0.
  const std::optional<OrderHandle> h0_reused = pool.Allocate(Order{
      .order_id = 202ULL,
      .price_ticks = 999'999LL,
      .quantity = 50U,
      .symbol_id = 1U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });
  ASSERT_TRUE(h0_reused.has_value());
  EXPECT_EQ(h0_reused->index, 0U);
  EXPECT_EQ(h0_reused->generation, 2U);

  // Stale handle h0 (generation 1) MUST be rejected even though slot 0 is now
  // occupied by order 202!
  EXPECT_EQ(pool.Get(*h0), nullptr);
  ASSERT_NE(pool.Get(*h0_reused), nullptr);
  EXPECT_EQ(pool.Get(*h0_reused)->order_id, 202ULL);

  // Out-of-bounds handle index is safely rejected.
  EXPECT_EQ(pool.Get(OrderHandle{999U, 1U}), nullptr);
  EXPECT_FALSE(pool.Release(OrderHandle{999U, 1U}));
}

TEST(MemorySafetyTest, Battle1SpatialLookupsMatchExactNotionalAcrossRegimes) {
  const SpatialDataset ds(1024);
  const std::uint32_t mask = 1023U;

  // Regime 1A: Sequential Scan
  const std::int64_t seq_unchecked =
      ScanSequentialUnchecked(ds.orders.data(), ds.orders.size());
  const std::int64_t seq_hardened = ScanSequentialHardenedTrap(ds.orders);
  const std::int64_t seq_span =
      ScanSequentialSpan(std::span<const Order>(ds.orders));
  const std::int64_t seq_at = ScanSequentialVectorAt(ds.orders);

  EXPECT_GT(seq_unchecked, 0);
  EXPECT_EQ(seq_unchecked, seq_hardened);
  EXPECT_EQ(seq_unchecked, seq_span);
  EXPECT_EQ(seq_unchecked, seq_at);

  // Regime 1B: Independent Batch Gather
  const std::int64_t gather_unchecked = GatherBatchUnchecked(
      ds.orders.data(), ds.random_idx.data(), ds.random_idx.size());
  const std::int64_t gather_hardened =
      GatherBatchHardenedTrap(ds.orders, ds.random_idx);
  const std::int64_t gather_span =
      GatherBatchSpan(std::span<const Order>(ds.orders),
                      std::span<const std::uint32_t>(ds.random_idx));
  const std::int64_t gather_at = GatherBatchVectorAt(ds.orders, ds.random_idx);
  const std::int64_t gather_bitmask = GatherBatchBitmaskSafe(
      ds.orders.data(), ds.random_idx.data(), ds.random_idx.size(), mask);

  EXPECT_GT(gather_unchecked, 0);
  EXPECT_EQ(gather_unchecked, gather_hardened);
  EXPECT_EQ(gather_unchecked, gather_span);
  EXPECT_EQ(gather_unchecked, gather_at);
  EXPECT_EQ(gather_unchecked, gather_bitmask);

  // Verify out-of-bounds index throws std::out_of_range in GatherBatchVectorAt
  // while GatherBatchBitmaskSafe masks it safely into bounds.
  const std::vector<std::uint32_t> oob_idx = {5000U};
  EXPECT_THROW((void)GatherBatchVectorAt(ds.orders, oob_idx),
               std::out_of_range);
  EXPECT_GT(GatherBatchBitmaskSafe(ds.orders.data(), oob_idx.data(),
                                   oob_idx.size(), mask),
            0);

  // Regime 1C: Dependent Probe Chain
  const std::int64_t dep_unchecked = ProbeDependentUnchecked(
      ds.orders.data(), ds.random_idx.data(), ds.random_idx.size(), mask);
  const std::int64_t dep_hardened =
      ProbeDependentHardenedTrap(ds.orders, ds.random_idx, mask);
  const std::int64_t dep_at =
      ProbeDependentVectorAt(ds.orders, ds.random_idx, mask);

  EXPECT_GT(dep_unchecked, 0);
  EXPECT_EQ(dep_unchecked, dep_hardened);
  EXPECT_EQ(dep_unchecked, dep_at);
}

TEST(MemorySafetyTest,
     Battle3TemporalChurnDetectsABACorruptionAndMatchesWeakPtr) {
  const TemporalChurnScenario scenario(1024);

  const TemporalAuditResult unchecked_audit =
      AuditTemporalLookups(scenario, /*use_generational_check=*/false);
  const TemporalAuditResult safe_audit =
      AuditTemporalLookups(scenario, /*use_generational_check=*/true);

  // 25% of 1024 = 256 canceled and ABA-recycled slots.
  EXPECT_EQ(unchecked_audit.valid_active_reads, 768U);
  EXPECT_EQ(unchecked_audit.rejected_stale_handles, 0U);
  EXPECT_EQ(unchecked_audit.silent_aba_corruptions, 256U);

  EXPECT_EQ(safe_audit.valid_active_reads, 768U);
  EXPECT_EQ(safe_audit.rejected_stale_handles, 256U);
  EXPECT_EQ(safe_audit.silent_aba_corruptions, 0U);

  // Hot-loop notional parity between 64-bit Generational Handle and weak_ptr.
  const std::int64_t gen_notional = RunTemporalGenerationalHotLoop(
      scenario.pool, scenario.original_handles, scenario.lookup_order);
  const std::int64_t weak_notional = RunTemporalWeakPtrHotLoop(
      scenario.original_weak_handles, scenario.lookup_order);
  const std::int64_t corrupted_notional = RunTemporalUncheckedSlotHotLoop(
      scenario.pool, scenario.original_handles, scenario.lookup_order);

  EXPECT_EQ(gen_notional, weak_notional);
  EXPECT_GT(corrupted_notional, gen_notional);
}

}  // namespace
}  // namespace hotpath::memory_safety
