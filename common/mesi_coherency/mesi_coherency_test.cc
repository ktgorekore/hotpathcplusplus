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
 * @brief Unit tests for MESI coherency primitives, Read-Before-CAS spinlocks,
 * and split-lock vs cache-line aligned headers.
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
