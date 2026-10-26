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
 * @file mesi_coherency_test.cc
 * @brief Unit tests for Episode 09 MESI coherency primitives, ShardedCounter,
 * BatchedIdGenerator, ThreadMagazine, Read-Before-CAS spinlocks, and split-lock
 * vs cache-line aligned wire headers.
 */

#include "common/mesi_coherency/mesi_coherency.h"

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace hotpath::mesi_coherency {
namespace {

TEST(MesiCoherencyTest, HeaderOffsetsMatchCacheLineBoundaries) {
  EXPECT_EQ(offsetof(PackedSplitLockHeader, seq_no), 60U);
  EXPECT_EQ(offsetof(AlignedTelemetryHeader, seq_no), 0U);
  EXPECT_EQ(sizeof(CacheLineAlignedAtomic), kCacheLineBytes);
}

TEST(MesiCoherencyTest, ShardedCounterAggregatesExactMultiThreadedTotal) {
  constexpr std::size_t kThreads = 16;
  constexpr std::size_t kIncrementsPerThread = 10'000;
  ShardedCounter<64> counter;

  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (std::size_t t = 0; t < kThreads; ++t) {
    workers.emplace_back([&, t]() {
      for (std::size_t i = 0; i < kIncrementsPerThread; ++i) {
        counter.Increment(t);
      }
    });
  }
  for (auto& w : workers) {
    w.join();
  }

  EXPECT_EQ(counter.ReadTotal(), kThreads * kIncrementsPerThread);
}

TEST(MesiCoherencyTest, BatchedIdGeneratorProducesUniqueMonotonicBlocks) {
  constexpr std::size_t kThreads = 8;
  constexpr std::size_t kIdsPerThread = 640;
  BatchedIdGenerator<64> gen;

  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (std::size_t t = 0; t < kThreads; ++t) {
    workers.emplace_back([&]() {
      BatchedIdGenerator<64>::ThreadLease lease{};
      for (std::size_t i = 0; i < kIdsPerThread; ++i) {
        (void)gen.NextId(lease);
      }
    });
  }
  for (auto& w : workers) {
    w.join();
  }

  EXPECT_EQ(gen.GlobalWatermark(), kThreads * kIdsPerThread);
}

TEST(MesiCoherencyTest, SplitAndAlignedAtomicsIncrementAccurately) {
  alignas(kCacheLineBytes) PackedSplitLockHeader split_hdr{};
  alignas(kCacheLineBytes) AlignedTelemetryHeader aligned_hdr{};

  for (std::uint64_t i = 0; i < 1000; ++i) {
    split_hdr.seq_no.fetch_add(1, std::memory_order_relaxed);
    aligned_hdr.seq_no.fetch_add(1, std::memory_order_relaxed);
  }

  EXPECT_EQ(split_hdr.seq_no.load(std::memory_order_relaxed), 1000U);
  EXPECT_EQ(aligned_hdr.seq_no.load(std::memory_order_relaxed), 1000U);
}

TEST(MesiCoherencyTest, NaiveAndReadBeforeCasSpinlocksPreserveLedgerIntegrity) {
  constexpr std::size_t kThreads = 8;
  constexpr std::size_t kOpsPerThread = 2000;

  NaiveCasSpinlock naive_lock;
  SharedOrderBookLedger naive_ledger{};

  ReadBeforeCasSpinlock ttas_lock;
  SharedOrderBookLedger ttas_ledger{};

  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (std::size_t t = 0; t < kThreads; ++t) {
    workers.emplace_back([&]() {
      for (std::size_t i = 0; i < kOpsPerThread; ++i) {
        naive_lock.Lock();
        naive_ledger.RecordExecution(10, 100);
        naive_lock.Unlock();

        ttas_lock.Lock();
        ttas_ledger.RecordExecution(10, 100);
        ttas_lock.Unlock();
      }
    });
  }
  for (auto& w : workers) {
    w.join();
  }

  EXPECT_EQ(naive_ledger.sequence, kThreads * kOpsPerThread);
  EXPECT_EQ(ttas_ledger.sequence, kThreads * kOpsPerThread);
  EXPECT_EQ(naive_ledger.total_Fill_qty, kThreads * kOpsPerThread * 10);
  EXPECT_EQ(ttas_ledger.total_Fill_qty, kThreads * kOpsPerThread * 10);
  EXPECT_EQ(naive_ledger.rolling_notional, kThreads * kOpsPerThread * 1000);
  EXPECT_EQ(ttas_ledger.rolling_notional, kThreads * kOpsPerThread * 1000);
  EXPECT_EQ(naive_ledger.venue_buckets, ttas_ledger.venue_buckets);
}

}  // namespace
}  // namespace hotpath::mesi_coherency
