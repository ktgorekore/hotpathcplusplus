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

#include "common/concurrency/seqlock.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace cognitas::trading {

TEST(SeqLockTest, SingleThreaded) {
  SeqLock lock;
  uint64_t seq = lock.ReadBegin();
  // Initially even (0), so should not retry.
  EXPECT_FALSE(lock.ReadRetry(seq));
  EXPECT_EQ(seq, 0ULL);

  lock.WriteBegin();
  seq = lock.ReadBegin();
  // In the middle of write (odd), should retry.
  EXPECT_TRUE(lock.ReadRetry(seq));
  EXPECT_EQ(seq, 1ULL);

  lock.WriteEnd();
  seq = lock.ReadBegin();
  // Write finished (even), should not retry.
  EXPECT_FALSE(lock.ReadRetry(seq));
  EXPECT_EQ(seq, 2ULL);
}

TEST(SeqLockTest, SequenceProgression) {
  SeqLock lock(100);
  EXPECT_EQ(lock.RawSequence(), 100ULL);

  for (int i = 0; i < 50; ++i) {
    lock.WriteBegin();
    EXPECT_EQ(lock.RawSequence(), 100ULL + i * 2 + 1);
    lock.WriteEnd();
    EXPECT_EQ(lock.RawSequence(), 100ULL + (i + 1) * 2);
  }
}

TEST(SeqLockTest, MultiThreadedIntegrity) {
  SeqLock lock;
  struct alignas(64) MarketQuote {
    uint64_t bid_price;
    uint64_t ask_price;
    uint64_t bid_size;
    uint64_t ask_size;
    int64_t timestamp;
  } shared_quote = {0, 1, 0, 5, 0};

  std::atomic<bool> start{false};
  std::atomic<bool> stop{false};
  std::atomic<uint64_t> total_valid_reads{0};
  std::atomic<uint64_t> total_retries{0};

  // Single Producer Writer
  std::thread writer([&]() {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    uint64_t tick = 1;
    while (!stop.load(std::memory_order_relaxed)) {
      lock.WriteBegin();
      // Multi-field update: All fields derived from tick to detect torn reads
      shared_quote.bid_price = tick * 100;
      shared_quote.ask_price = tick * 100 + 1;
      shared_quote.bid_size = tick;
      shared_quote.ask_size = tick + 5;
      shared_quote.timestamp = static_cast<int64_t>(tick);
      lock.WriteEnd();
      tick++;
    }
  });

  // Multiple Concurrent Readers
  constexpr size_t kReaderCount = 4;
  constexpr size_t kReadsPerReader = 2500;
  std::vector<std::thread> readers;
  readers.reserve(kReaderCount);

  for (size_t r = 0; r < kReaderCount; ++r) {
    readers.emplace_back([&]() {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      uint64_t valid_reads = 0;
      uint64_t retries = 0;

      while (valid_reads < kReadsPerReader &&
             !stop.load(std::memory_order_relaxed)) {
        uint64_t seq;
        MarketQuote local;
        do {
          seq = lock.ReadBegin();
          local = shared_quote;
          retries++;
        } while (lock.ReadRetry(seq));

        // Invariant: ask_price MUST be bid_price + 1 and ask_size MUST be
        // bid_size + 5
        ASSERT_EQ(local.ask_price, local.bid_price + 1)
            << "Torn read detected: bid=" << local.bid_price
            << ", ask=" << local.ask_price;
        ASSERT_EQ(local.ask_size, local.bid_size + 5)
            << "Torn size detected: bid_size=" << local.bid_size
            << ", ask_size=" << local.ask_size;
        ASSERT_EQ(local.timestamp, static_cast<int64_t>(local.bid_size))
            << "Torn timestamp detected: ts=" << local.timestamp;

        valid_reads++;
      }

      total_valid_reads.fetch_add(valid_reads, std::memory_order_relaxed);
      total_retries.fetch_add(retries, std::memory_order_relaxed);
    });
  }

  start.store(true, std::memory_order_release);

  for (auto& reader : readers) {
    reader.join();
  }
  stop.store(true, std::memory_order_release);
  writer.join();

  EXPECT_EQ(total_valid_reads.load(), kReaderCount * kReadsPerReader);
}

}  // namespace cognitas::trading
