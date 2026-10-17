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
 * @file store_buffer_test.cc
 * @brief Unit tests for SpscRingBuffer memory ordering variants.
 */

#include "common/store_buffer/store_buffer.h"

#include <cstdint>
#include <thread>

#include "gtest/gtest.h"

namespace hotpath::store_buffer {
namespace {

TEST(StoreBufferTest, SeqCstPushPopPreservesOrderAndBoundaries) {
  SpscRingBuffer<MarketTick, 4> ring;
  MarketTick out{};
  EXPECT_FALSE(ring.TryPopSeqCst(out));

  for (uint64_t i = 0; i < 4; ++i) {
    EXPECT_TRUE(ring.TryPushSeqCst(MarketTick{i, 1000 + i, 10, 1}));
  }
  EXPECT_FALSE(ring.TryPushSeqCst(MarketTick{99, 9999, 1, 1}));

  for (uint64_t i = 0; i < 4; ++i) {
    ASSERT_TRUE(ring.TryPopSeqCst(out));
    EXPECT_EQ(out.sequence, i);
    EXPECT_EQ(out.price_micros, 1000 + i);
  }
  EXPECT_FALSE(ring.TryPopSeqCst(out));
}

TEST(StoreBufferTest, AcquireReleaseConcurrentHandoffChecksum) {
  constexpr size_t kMessages = 50000;
  SpscRingBuffer<MarketTick, 1024> ring;
  uint64_t consumer_checksum = 0;

  std::thread producer([&]() {
    for (uint64_t i = 1; i <= kMessages; ++i) {
      const MarketTick tick{i, i * 100ULL, static_cast<uint32_t>(i & 0xFFU), 7};
      while (!ring.TryPushAcquireRelease(tick)) {
      }
    }
  });

  std::thread consumer([&]() {
    MarketTick tick{};
    for (uint64_t i = 1; i <= kMessages; ++i) {
      while (!ring.TryPopAcquireRelease(tick)) {
      }
      consumer_checksum += tick.sequence + tick.price_micros + tick.quantity;
    }
  });

  producer.join();
  consumer.join();

  uint64_t expected_checksum = 0;
  for (uint64_t i = 1; i <= kMessages; ++i) {
    expected_checksum += i + (i * 100ULL) + (i & 0xFFU);
  }
  EXPECT_EQ(consumer_checksum, expected_checksum);
}

}  // namespace
}  // namespace hotpath::store_buffer
