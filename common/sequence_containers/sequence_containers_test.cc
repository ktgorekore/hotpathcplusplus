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
 * @file sequence_containers_test.cc
 * @brief GoogleTest parity and compile-time invariant suite for Episode 17.
 */

#include "common/sequence_containers/sequence_containers.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "folly/FBVector.h"

namespace hotpath::episode17 {
namespace {

TEST(CompactRingDequeTest, WrapAroundAndGrowthPreserveFifoOrder) {
  CompactRingDeque<std::uint64_t> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.capacity(), 0U);

  for (std::uint64_t i = 0; i < 6; ++i) {
    q.push_back(i);
  }
  EXPECT_EQ(q.capacity(), 8U);

  for (std::uint64_t i = 0; i < 4; ++i) {
    EXPECT_EQ(q.front(), i);
    q.pop_front();
  }
  for (std::uint64_t i = 6; i < 20; ++i) {
    q.push_back(i);
  }
  EXPECT_EQ(q.size(), 16U);

  std::vector<std::uint64_t> observed;
  q.ForEach([&](std::uint64_t v) { observed.push_back(v); });
  ASSERT_EQ(observed.size(), 16U);
  for (std::uint64_t i = 0; i < 16; ++i) {
    EXPECT_EQ(observed[i], i + 4U);
    EXPECT_EQ(q[i], i + 4U);
  }

  CompactRingDeque<std::uint64_t> copy = q;
  CompactRingDeque<std::uint64_t> moved = std::move(q);
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(copy.size(), 16U);
  EXPECT_EQ(moved.size(), 16U);
  for (std::uint64_t i = 0; i < 16; ++i) {
    EXPECT_EQ(copy[i], i + 4U);
    EXPECT_EQ(moved[i], i + 4U);
  }
}

TEST(Story1RuleTableTest, AllSixUnsubscribeStrategiesMatchExactParity) {
  const auto wl = BuildRuleTableWorkload(512, 160, 8);
  PreAgedListUnsubscribeState aged(wl);

  std::list<SubscriptionRule> list_linear = aged.prototype_list;
  const std::uint64_t list_lin_sum = RunRuleTableStdListLinear(list_linear, wl);

  std::list<SubscriptionRule> list_idx = aged.prototype_list;
  absl::flat_hash_map<std::uint64_t, std::list<SubscriptionRule>::iterator>
      it_map;
  it_map.reserve(list_idx.size() * 2U);
  for (auto it = list_idx.begin(); it != list_idx.end(); ++it) {
    it_map.emplace(it->rule_id, it);
  }
  const std::uint64_t list_idx_sum =
      RunRuleTableStdListIndexed(list_idx, it_map, wl);

  std::vector<SubscriptionRule> vec_ord = wl.initial_rules;
  const std::uint64_t vec_ord_sum = RunRuleTableVectorOrderedErase(vec_ord, wl);

  std::vector<SubscriptionRule> vec_swap = wl.initial_rules;
  const std::uint64_t vec_swap_sum = RunRuleTableVectorSwapPop(vec_swap, wl);

  std::vector<SubscriptionRule> vec_sparse = wl.initial_rules;
  absl::flat_hash_map<std::uint64_t, std::size_t> sparse_map;
  sparse_map.reserve(wl.initial_rules.size() * 2U);
  for (std::size_t i = 0; i < vec_sparse.size(); ++i) {
    sparse_map.emplace(vec_sparse[i].rule_id, i);
  }
  const std::uint64_t vec_sparse_sum =
      RunRuleTableVectorIndexedSwapPop(vec_sparse, sparse_map, wl);

  std::vector<SubscriptionRule> vec_tomb = wl.initial_rules;
  vec_tomb.reserve(wl.initial_rules.size() + 256U);
  absl::flat_hash_map<std::uint64_t, std::size_t> pos_map;
  pos_map.reserve(wl.initial_rules.size() * 2U);
  for (std::size_t i = 0; i < vec_tomb.size(); ++i) {
    pos_map.emplace(vec_tomb[i].rule_id, i);
  }
  const std::uint64_t vec_tomb_sum =
      RunRuleTableVectorTombstoneCompact(vec_tomb, pos_map, wl);

  EXPECT_GT(list_lin_sum, 0U);
  EXPECT_EQ(list_idx_sum, list_lin_sum);
  EXPECT_EQ(vec_ord_sum, list_lin_sum);
  EXPECT_EQ(vec_swap_sum, list_lin_sum);
  EXPECT_EQ(vec_sparse_sum, list_lin_sum);
  EXPECT_EQ(vec_tomb_sum, list_lin_sum);
}

TEST(Story1DualQueueTest, IntrusiveStdListAndBothVectorModesMatchExactly) {
  const auto wl = BuildDualQueueEventWorkload(512, 2'048);
  const std::uint64_t intrusive_cs = RunDualQueueIntrusive(wl);
  const std::uint64_t std_list_cs = RunDualQueueStdListSplice(wl);
  const std::uint64_t vec_idx_cs = RunDualQueueVectorIndexedRotate(wl);
  const std::uint64_t vec_ptr_cs = RunDualQueueVectorOfPointers(wl);
  EXPECT_NE(intrusive_cs, 0U);
  EXPECT_EQ(intrusive_cs, std_list_cs);
  EXPECT_EQ(intrusive_cs, vec_idx_cs);
  EXPECT_EQ(intrusive_cs, vec_ptr_cs);
}

TEST(Story2FifoQueuesTest, SingleAndMultiplexedFifoMatchNonZeroParity) {
  const std::uint64_t deque_cs = RunFifoStreamStdDeque(128, 4'096);
  const std::uint64_t boost_cs = RunFifoStreamBoostCircular(128, 4'096);
  const std::uint64_t ring_cs = RunFifoStreamCompactRing(128, 4'096);
  EXPECT_NE(deque_cs, 0U);
  EXPECT_EQ(boost_cs, deque_cs);
  EXPECT_EQ(ring_cs, deque_cs);

  // Verify the rolling hash never cancels to zero either.
  const std::uint64_t mux_deq =
      RunMultiplexedGatewayFifo<std::deque<OutboundMessage>>(64, 16, 16'384);
  const std::uint64_t mux_ring =
      RunMultiplexedGatewayFifo<CompactRingDeque<OutboundMessage>>(64, 16,
                                                                   16'384);
  EXPECT_NE(mux_deq, 0U);
  EXPECT_EQ(mux_ring, mux_deq);
}

TEST(Story3SessionsTest, AllSessionGrowthVariantsMatchChecksumAndCapacity) {
  const auto m_std_queue =
      RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>>(512);
  const auto m_std_queue_res =
      RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>, true>(
          512);
  const auto m_no_noexcept =
      RunSessionTableGrowth<std::vector<SessionMissingNoexceptMove>>(512);
  const auto m_ring =
      RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>>(512);
  const auto m_uptr = RunSessionPointerVectorGrowth(512);
  const auto m_ring_res =
      RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>, true>(512);
  const auto m_fbvec =
      RunSessionTableGrowth<folly::fbvector<SessionNoexceptRingQueue>>(512);
  const auto m_reloc =
      RunSessionTableGrowth<folly::fbvector<SessionRelocatable>>(512);

  EXPECT_NE(m_ring.checksum, 0U);
  EXPECT_EQ(m_std_queue.checksum, m_ring.checksum);
  EXPECT_EQ(m_std_queue_res.checksum, m_ring.checksum);
  EXPECT_EQ(m_no_noexcept.checksum, m_ring.checksum);
  EXPECT_EQ(m_uptr.checksum, m_ring.checksum);
  EXPECT_EQ(m_ring_res.checksum, m_ring.checksum);
  EXPECT_EQ(m_fbvec.checksum, m_ring.checksum);
  EXPECT_EQ(m_reloc.checksum, m_ring.checksum);

  const auto m_std_150k =
      RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>>(150'000);
  const auto m_fb_150k =
      RunSessionTableGrowth<folly::fbvector<SessionNoexceptRingQueue>>(150'000);
  EXPECT_EQ(m_std_150k.final_capacity, 262'144U);
  EXPECT_LT(m_fb_150k.final_capacity, m_std_150k.final_capacity);
}

}  // namespace
}  // namespace hotpath::episode17
