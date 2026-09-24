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

#include "common/concurrency/rcu_hash_map.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace cognitas::trading {
namespace {

TEST(RcuHashMapTest, BasicInsertAndGet) {
  RcuHashMap<std::string, int> map(16);

  EXPECT_EQ(map.Size(), 0u);
  EXPECT_FALSE(map.Get("key1").has_value());

  map.Put("key1", 42);
  EXPECT_EQ(map.Size(), 1u);

  auto val = map.Get("key1");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 42);

  // Update existing key
  map.Put("key1", 100);
  EXPECT_EQ(map.Size(), 1u);
  EXPECT_EQ(map.Get("key1").value_or(0), 100);

  // Remove key
  EXPECT_TRUE(map.Remove("key1"));
  EXPECT_EQ(map.Size(), 0u);
  EXPECT_FALSE(map.Get("key1").has_value());
  EXPECT_FALSE(map.Remove("key1"));
}

TEST(RcuHashMapTest, AutomaticResize) {
  // Start with very small table to force multiple resizes
  RcuHashMap<int, int> map(4);

  for (int i = 0; i < 100; ++i) {
    map.Put(i, i * 10);
  }

  EXPECT_EQ(map.Size(), 100u);

  for (int i = 0; i < 100; ++i) {
    auto val = map.Get(i);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, i * 10);
  }
}

TEST(RcuHashMapTest, ConcurrentReadersAndWriters) {
  RcuHashMap<int, int> map(64);

  for (int i = 0; i < 50; ++i) {
    map.Put(i, i);
  }

  std::atomic<bool> stop_flag{false};
  std::vector<std::thread> readers;

  // 4 reader threads
  for (int t = 0; t < 4; ++t) {
    readers.emplace_back([&map, &stop_flag] {
      EpochBasedReclamation::RegisterThread();
      while (!stop_flag.load(std::memory_order_relaxed)) {
        for (int i = 0; i < 50; ++i) {
          auto val = map.Get(i);
          if (val.has_value()) {
            EXPECT_GE(*val, 0);
          }
        }
      }
      EpochBasedReclamation::UnregisterThread();
    });
  }

  // Writer thread updating values
  std::thread writer([&map, &stop_flag] {
    EpochBasedReclamation::RegisterThread();
    for (int round = 1; round <= 20; ++round) {
      for (int i = 0; i < 50; ++i) {
        map.Put(i, i + round * 100);
      }
    }
    stop_flag.store(true, std::memory_order_release);
    EpochBasedReclamation::UnregisterThread();
  });

  writer.join();
  for (auto& r : readers) {
    r.join();
  }

  EXPECT_EQ(map.Size(), 50u);
}

}  // namespace
}  // namespace cognitas::trading
