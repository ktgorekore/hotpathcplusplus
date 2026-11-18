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
 * @file memory_allocators_test.cc
 * @brief GoogleTest verification suite for Episode 12 allocator primitives,
 * cache-line packing invariants, multi-threaded magazine conservation, and
 * C++17 `std::pmr` allocator-aware propagation (`BrokenPmrVenueFill` vs.
 * `PmrVenueFill`).
 */

#include "common/memory_allocators/memory_allocators.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <string_view>
#include <thread>
#include <vector>

namespace hotpath::memory_allocators {
namespace {

TEST(OrderNodeLayoutTest, Exact32BytePackingAndZeroHeaderSlab) {
  EXPECT_EQ(sizeof(OrderNode), 32U);
  EXPECT_EQ(sizeof(FixedSlabPool<OrderNode>::Slot), 32U);
  EXPECT_EQ(sizeof(FixedSlabPool<OrderNode>::Slot), sizeof(OrderNode));
}

TEST(BumpArenaTest, AlignmentPackingAndReset) {
  BumpArena arena(8192);
  ASSERT_EQ(arena.Capacity(), 8192U);
  EXPECT_EQ(arena.BytesUsed(), 0U);

  auto* n1 = arena.Create<OrderNode>(1ULL, 18525LL, 100U, 1U, nullptr);
  auto* n2 = arena.Create<OrderNode>(2ULL, 18530LL, 200U, 1U, n1);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);

  // Two consecutive 32-byte OrderNodes occupy exactly 1 cache line (64 bytes).
  const auto addr1 = reinterpret_cast<std::uintptr_t>(n1);
  const auto addr2 = reinterpret_cast<std::uintptr_t>(n2);
  EXPECT_EQ(addr2 - addr1, 32U);
  EXPECT_EQ(arena.BytesUsed(), 64U);

  void* cache_aligned = arena.AllocateBytes(16, kCacheLineBytes);
  ASSERT_NE(cache_aligned, nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(cache_aligned) % kCacheLineBytes,
            0U);

  arena.Reset();
  EXPECT_EQ(arena.BytesUsed(), 0U);

  // Exhaustion returns nullptr cleanly without throwing.
  EXPECT_NE(arena.AllocateBytes(8192, 8), nullptr);
  EXPECT_EQ(arena.AllocateBytes(8, 8), nullptr);
}

TEST(FixedSlabPoolTest, O1LifoReuseAndExhaustion) {
  constexpr std::size_t kCap = 128;
  FixedSlabPool<OrderNode> pool(kCap);
  EXPECT_EQ(pool.Capacity(), kCap);
  EXPECT_EQ(pool.Available(), kCap);

  std::vector<OrderNode*> live;
  live.reserve(kCap);
  for (std::size_t i = 0; i < kCap; ++i) {
    OrderNode* node = pool.Allocate(i, 18525LL, 100U, 7U, nullptr);
    ASSERT_NE(node, nullptr);
    live.push_back(node);
  }
  EXPECT_EQ(pool.Available(), 0U);
  EXPECT_EQ(pool.Allocate(999ULL, 0LL, 0U, 0U, nullptr), nullptr);

  // LIFO reuse verification: freeing a node returns the exact same slot.
  OrderNode* freed_addr = live.back();
  live.pop_back();
  pool.Deallocate(freed_addr);
  EXPECT_EQ(pool.Available(), 1U);

  OrderNode* reallocated = pool.Allocate(42ULL, 19000LL, 50U, 7U, nullptr);
  EXPECT_EQ(reallocated, freed_addr);
  live.push_back(reallocated);

  for (OrderNode* node : live) {
    pool.Deallocate(node);
  }
  EXPECT_EQ(pool.Available(), kCap);
}

TEST(ThreadMagazinePoolTest, MultiThreadedProducerConsumerConservation) {
  constexpr std::size_t kProducers = 4;
  constexpr std::size_t kItemsPerProducer = 25000;
  constexpr std::size_t kBatchSize = 32;
  ThreadMagazinePool<OrderNode, kBatchSize> pool(
      kProducers * kItemsPerProducer + 2048);
  const std::size_t initial_depot = pool.DepotAvailableSlots();

  std::vector<SpscRingQueue<OrderNode*, 16384>> queues(kProducers);
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  producers.reserve(kProducers);
  consumers.reserve(kProducers);

  for (std::size_t t = 0; t < kProducers; ++t) {
    producers.emplace_back([&, t]() {
      ThreadMagazinePool<OrderNode, kBatchSize>::LocalCache cache(pool);
      for (std::size_t i = 0; i < kItemsPerProducer; ++i) {
        OrderNode* node = pool.Allocate(
            cache, static_cast<std::uint64_t>(t * kItemsPerProducer + i),
            18525LL, 100U, static_cast<std::uint32_t>(t), nullptr);
        ASSERT_NE(node, nullptr);
        while (!queues[t].TryPush(node)) {
          std::this_thread::yield();
        }
      }
    });

    consumers.emplace_back([&, t]() {
      ThreadMagazinePool<OrderNode, kBatchSize>::LocalCache cache(pool);
      std::size_t consumed = 0;
      while (consumed < kItemsPerProducer) {
        OrderNode* node = nullptr;
        if (queues[t].TryPop(node)) {
          EXPECT_EQ(node->symbol_id, static_cast<std::uint32_t>(t));
          pool.Deallocate(cache, node);
          ++consumed;
        } else {
          std::this_thread::yield();
        }
      }
    });
  }

  for (auto& p : producers) p.join();
  for (auto& c : consumers) c.join();

  EXPECT_EQ(pool.DepotAvailableSlots(), initial_depot);
}

TEST(StdPmrPropagationTest, DetectsBrokenNestedStringAndProvesZeroHeapFix) {
  // 24 characters exceeds libstdc++'s 15-byte SSO buffer on 64-bit Linux.
  constexpr std::string_view kLongExecId = "XNAS-EXEC-20261118-00042";
  constexpr std::size_t kFillCount = 24;
  ASSERT_GT(kLongExecId.size(), 15U);

  CountingMemoryResource upstream;

  // Trap case: BrokenPmrVenueFill lacks `allocator_type`, so pmr::vector
  // fits in stack buffer, while 24 inner pmr::strings escape to `default_res`.
  {
    std::pmr::memory_resource* prev_default =
        std::pmr::set_default_resource(&upstream);
    std::array<std::byte, 4096> stack_buf{};
    std::pmr::monotonic_buffer_resource stack_arena(
        stack_buf.data(), stack_buf.size(), &upstream);
    std::pmr::vector<BrokenPmrVenueFill> fills(&stack_arena);
    fills.reserve(kFillCount);
    for (std::size_t i = 0; i < kFillCount; ++i) {
      fills.emplace_back(i, 18525LL, 100U, 1U, kLongExecId);
    }
    EXPECT_EQ(upstream.alloc_count(), kFillCount);
    std::pmr::set_default_resource(prev_default);
  }

  upstream.ResetCounters();

  // Fixed case: PmrVenueFill declares `allocator_type`, achieving ZERO
  // upstream heap allocations across vector growth + 24 long strings.
  {
    std::pmr::memory_resource* prev_default =
        std::pmr::set_default_resource(&upstream);
    std::array<std::byte, 4096> stack_buf{};
    std::pmr::monotonic_buffer_resource stack_arena(
        stack_buf.data(), stack_buf.size(), &upstream);
    std::pmr::vector<PmrVenueFill> fills(&stack_arena);
    fills.reserve(kFillCount);
    for (std::size_t i = 0; i < kFillCount; ++i) {
      fills.emplace_back(i, 18525LL, 100U, 1U, kLongExecId);
    }
    EXPECT_EQ(upstream.alloc_count(), 0U);
    EXPECT_EQ(upstream.bytes_allocated(), 0U);
    std::pmr::set_default_resource(prev_default);
  }
}

}  // namespace
}  // namespace hotpath::memory_allocators
