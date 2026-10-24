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
 * @file locks_under_the_hood_test.cc
 * @brief Unit tests for Episode 10 spinlocks, TTAS + `_mm_pause()`, 3-state
 * Linux `futex`, Adaptive Hybrid `futex`, and C++20 `std::atomic::wait` locks.
 */

#include "common/locks_under_the_hood/locks_under_the_hood.h"

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace hotpath::locks_under_the_hood {
namespace {

template <typename LockType>
void VerifyLockMutualExclusion(std::size_t num_threads,
                               std::size_t ops_per_thread) {
  LockType lock;
  ProtectedState shared{};

  std::vector<std::thread> workers;
  workers.reserve(num_threads);
  for (std::size_t t = 0; t < num_threads; ++t) {
    workers.emplace_back([&]() {
      for (std::size_t i = 0; i < ops_per_thread; ++i) {
        lock.lock();
        shared.ApplyTransfer(1);
        lock.unlock();
      }
    });
  }
  for (auto& w : workers) {
    w.join();
  }

  EXPECT_EQ(shared.sequence, num_threads * ops_per_thread);
  EXPECT_EQ(shared.balance_a + shared.balance_b, 2'000'000ULL);
}

TEST(LocksUnderTheHoodTest, CacheLineAlignmentAndSizes) {
  EXPECT_EQ(alignof(NaiveTASLock), kCacheLineBytes);
  EXPECT_EQ(alignof(TTASNoPauseLock), kCacheLineBytes);
  EXPECT_EQ(alignof(TTASPauseLock), kCacheLineBytes);
  EXPECT_EQ(alignof(RawFutexMutex), kCacheLineBytes);
  EXPECT_EQ(alignof(AdaptiveFutexMutex), kCacheLineBytes);
  EXPECT_EQ(alignof(Cpp20AtomicWaitLock), kCacheLineBytes);
}

TEST(LocksUnderTheHoodTest, AllSpinlocksPreserveInvariantsUnderContention) {
  VerifyLockMutualExclusion<NaiveTASLock>(8, 2000);
  VerifyLockMutualExclusion<TTASNoPauseLock>(8, 2000);
  VerifyLockMutualExclusion<TTASPauseLock>(8, 2000);
}

TEST(LocksUnderTheHoodTest,
     FutexAndCpp20AtomicWaitLocksPreserveInvariantsUnderOversubscription) {
  VerifyLockMutualExclusion<StdMutexWrapper>(16, 1500);
  VerifyLockMutualExclusion<RawFutexMutex>(16, 1500);
  VerifyLockMutualExclusion<AdaptiveFutexMutex>(16, 1500);
  VerifyLockMutualExclusion<Cpp20AtomicWaitLock>(16, 1500);
}

}  // namespace
}  // namespace hotpath::locks_under_the_hood
