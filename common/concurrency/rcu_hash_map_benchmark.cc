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
 * @file rcu_hash_map_benchmark.cc
 * @brief Benchmark comparing std::unordered_map, absl::flat_hash_map, and
 * RcuHashMap.
 */

#include <benchmark/benchmark.h>

#include <cstdint>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "common/concurrency/ebr.h"
#include "common/concurrency/rcu_hash_map.h"

namespace cognitas::trading {
namespace {

constexpr int kKeySpace = 65536;

struct LockedStdMap {
  std::shared_mutex mutex;
  std::unordered_map<int, int> map;

  LockedStdMap() {
    for (int i = 0; i < kKeySpace; ++i) {
      map[i] = i ^ 0x5a5a;
    }
  }
};

static LockedStdMap& GetLockedStdMap() {
  static LockedStdMap instance;
  return instance;
}

void BM_StdUnorderedMap_SharedMutex_Reads(benchmark::State& state) {
  auto& fixture = GetLockedStdMap();
  std::mt19937 rng(1337 + state.thread_index());
  std::uniform_int_distribution<int> dist(0, kKeySpace - 1);

  for (auto _ : state) {
    int key = dist(rng);
    int value = 0;
    {
      std::shared_lock<std::shared_mutex> lock(fixture.mutex);
      auto it = fixture.map.find(key);
      if (it != fixture.map.end()) {
        value = it->second;
      }
    }
    benchmark::DoNotOptimize(value);
  }
  state.SetItemsProcessed(state.iterations());
}

struct LockedAbslMap {
  absl::Mutex mutex;
  absl::flat_hash_map<int, int> map;

  LockedAbslMap() {
    absl::MutexLock lock(mutex);
    for (int i = 0; i < kKeySpace; ++i) {
      map[i] = i ^ 0x5a5a;
    }
  }
};

static LockedAbslMap& GetLockedAbslMap() {
  static LockedAbslMap instance;
  return instance;
}

void BM_AbslFlatHashMap_AbslMutex_Reads(benchmark::State& state) {
  auto& fixture = GetLockedAbslMap();
  std::mt19937 rng(1337 + state.thread_index());
  std::uniform_int_distribution<int> dist(0, kKeySpace - 1);

  for (auto _ : state) {
    int key = dist(rng);
    int value = 0;
    {
      absl::ReaderMutexLock lock(fixture.mutex);
      auto it = fixture.map.find(key);
      if (it != fixture.map.end()) {
        value = it->second;
      }
    }
    benchmark::DoNotOptimize(value);
  }
  state.SetItemsProcessed(state.iterations());
}

struct RcuMapFixture {
  RcuHashMap<int, int> map{kKeySpace * 2};

  RcuMapFixture() {
    for (int i = 0; i < kKeySpace; ++i) {
      map.Put(i, i ^ 0x5a5a);
    }
  }
};

static RcuMapFixture& GetRcuMapFixture() {
  static RcuMapFixture instance;
  return instance;
}

void BM_RcuHashMap_WaitFree_Reads(benchmark::State& state) {
  auto& fixture = GetRcuMapFixture();
  EpochBasedReclamation::RegisterThread();

  std::mt19937 rng(1337 + state.thread_index());
  std::uniform_int_distribution<int> dist(0, kKeySpace - 1);

  for (auto _ : state) {
    int key = dist(rng);
    auto val = fixture.map.Get(key);
    benchmark::DoNotOptimize(val);
  }

  EpochBasedReclamation::UnregisterThread();
  state.SetItemsProcessed(state.iterations());
}

}  // namespace
}  // namespace cognitas::trading

BENCHMARK(cognitas::trading::BM_StdUnorderedMap_SharedMutex_Reads)
    ->Threads(1)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16)
    ->UseRealTime();

BENCHMARK(cognitas::trading::BM_AbslFlatHashMap_AbslMutex_Reads)
    ->Threads(1)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16)
    ->UseRealTime();

BENCHMARK(cognitas::trading::BM_RcuHashMap_WaitFree_Reads)
    ->Threads(1)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16)
    ->UseRealTime();

BENCHMARK_MAIN();
