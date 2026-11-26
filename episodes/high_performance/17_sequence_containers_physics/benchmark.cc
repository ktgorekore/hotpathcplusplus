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
 * @file benchmark.cc
 * @brief Episode 17 official Google Benchmark suite covering the 3 deep
 *        sequence container stories inside `RealtimeGatewayService`.
 */

#include <benchmark/benchmark.h>

#include <cstdint>
#include <deque>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "common/sequence_containers/sequence_containers.h"
#include "folly/FBVector.h"

namespace hotpath::episode17 {
namespace {

// =============================================================================
// Story 1A: Active Alert-Routing Subscription Table on an Aged Heap
// =============================================================================

void BM_Story1A_AgedScan_StdVector(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  AgedRuleTableFixture fx(n);
  for (auto _ : state) {
    std::uint64_t sum = 0;
    for (const auto& rule : fx.vec_rules) {
      sum += EvaluateRuleScore(rule);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story1A_AgedScan_StdVector)->Arg(1'024)->Arg(4'096)->Arg(16'384);

void BM_Story1A_AgedScan_StdList(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  AgedRuleTableFixture fx(n);
  for (auto _ : state) {
    std::uint64_t sum = 0;
    for (const auto& rule : fx.list_rules) {
      sum += EvaluateRuleScore(rule);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story1A_AgedScan_StdList)->Arg(1'024)->Arg(4'096)->Arg(16'384);

void BM_Story1A_AgedScan_BoostIntrusiveList(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  AgedRuleTableFixture fx(n);
  for (auto _ : state) {
    std::uint64_t sum = 0;
    for (const auto& node : fx.intrusive_list) {
      sum += EvaluateRuleScore(node.rule);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story1A_AgedScan_BoostIntrusiveList)
    ->Arg(1'024)
    ->Arg(4'096)
    ->Arg(16'384);

void BM_Story1A_Unsub_StdListLinearAged(benchmark::State& state) {
  const auto wl = BuildRuleTableWorkload(8'192, 512, 8);
  PreAgedListUnsubscribeState aged(wl);
  std::list<SubscriptionRule> working = aged.prototype_list;
  for (auto _ : state) {
    state.PauseTiming();
    PreAgedListUnsubscribeState::RestoreInPlace(working, wl);
    state.ResumeTiming();
    auto cs = RunRuleTableStdListLinear(working, wl);
    benchmark::DoNotOptimize(cs);
  }
}
BENCHMARK(BM_Story1A_Unsub_StdListLinearAged);

void BM_Story1A_Unsub_StdListIndexedAged(benchmark::State& state) {
  const auto wl = BuildRuleTableWorkload(8'192, 512, 8);
  PreAgedListUnsubscribeState aged(wl);
  std::list<SubscriptionRule> working = aged.prototype_list;
  absl::flat_hash_map<std::uint64_t, std::list<SubscriptionRule>::iterator>
      index;
  index.reserve(16'384);
  for (auto _ : state) {
    state.PauseTiming();
    PreAgedListUnsubscribeState::RestoreInPlace(working, wl);
    index.clear();
    for (auto it = working.begin(); it != working.end(); ++it) {
      index.emplace(it->rule_id, it);
    }
    state.ResumeTiming();
    auto cs = RunRuleTableStdListIndexed(working, index, wl);
    benchmark::DoNotOptimize(cs);
  }
}
BENCHMARK(BM_Story1A_Unsub_StdListIndexedAged);

void BM_Story1A_Unsub_StdVectorOrdered(benchmark::State& state) {
  const auto wl = BuildRuleTableWorkload(8'192, 512, 8);
  std::vector<SubscriptionRule> working;
  working.reserve(8'192);
  for (auto _ : state) {
    state.PauseTiming();
    working = wl.initial_rules;
    state.ResumeTiming();
    auto cs = RunRuleTableVectorOrderedErase(working, wl);
    benchmark::DoNotOptimize(cs);
  }
}
BENCHMARK(BM_Story1A_Unsub_StdVectorOrdered);

void BM_Story1A_Unsub_StdVectorSwapPop(benchmark::State& state) {
  const auto wl = BuildRuleTableWorkload(8'192, 512, 8);
  std::vector<SubscriptionRule> working;
  working.reserve(8'192);
  for (auto _ : state) {
    state.PauseTiming();
    working = wl.initial_rules;
    state.ResumeTiming();
    auto cs = RunRuleTableVectorSwapPop(working, wl);
    benchmark::DoNotOptimize(cs);
  }
}
BENCHMARK(BM_Story1A_Unsub_StdVectorSwapPop);

void BM_Story1A_Unsub_StdVectorSparseSetSwapPop(benchmark::State& state) {
  const auto wl = BuildRuleTableWorkload(8'192, 512, 8);
  std::vector<SubscriptionRule> working;
  working.reserve(8'192);
  absl::flat_hash_map<std::uint64_t, std::size_t> index;
  index.reserve(16'384);
  for (auto _ : state) {
    state.PauseTiming();
    working = wl.initial_rules;
    index.clear();
    for (std::size_t i = 0; i < working.size(); ++i) {
      index.emplace(working[i].rule_id, i);
    }
    state.ResumeTiming();
    auto cs = RunRuleTableVectorIndexedSwapPop(working, index, wl);
    benchmark::DoNotOptimize(cs);
  }
}
BENCHMARK(BM_Story1A_Unsub_StdVectorSparseSetSwapPop);

void BM_Story1A_Unsub_StdVectorTombstoneCompact(benchmark::State& state) {
  const auto wl = BuildRuleTableWorkload(8'192, 512, 8);
  std::vector<SubscriptionRule> working;
  working.reserve(8'704);
  absl::flat_hash_map<std::uint64_t, std::size_t> index;
  index.reserve(16'384);
  for (auto _ : state) {
    state.PauseTiming();
    working = wl.initial_rules;
    working.reserve(8'704);
    index.clear();
    for (std::size_t i = 0; i < working.size(); ++i) {
      index.emplace(working[i].rule_id, i);
    }
    state.ResumeTiming();
    auto cs = RunRuleTableVectorTombstoneCompact(working, index, wl);
    benchmark::DoNotOptimize(cs);
  }
}
BENCHMARK(BM_Story1A_Unsub_StdVectorTombstoneCompact);

// =============================================================================
// Story 1B: Where Intrusive Lists Win — 16,384 Dual-Hook Connections (200K Ev)
// =============================================================================

const DualQueueEventWorkload& GetDualQueueWorkload() {
  static const DualQueueEventWorkload wl =
      BuildDualQueueEventWorkload(16'384, 200'000);
  return wl;
}

void BM_Story1B_DualQueue_BoostIntrusiveList(benchmark::State& state) {
  const auto& wl = GetDualQueueWorkload();
  for (auto _ : state) {
    auto cs = RunDualQueueIntrusive(wl);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(wl.active_indices.size()));
}
BENCHMARK(BM_Story1B_DualQueue_BoostIntrusiveList);

void BM_Story1B_DualQueue_StdListSplice(benchmark::State& state) {
  const auto& wl = GetDualQueueWorkload();
  for (auto _ : state) {
    auto cs = RunDualQueueStdListSplice(wl);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(wl.active_indices.size()));
}
BENCHMARK(BM_Story1B_DualQueue_StdListSplice);

void BM_Story1B_DualQueue_VectorIndexedMemmove(benchmark::State& state) {
  const auto& wl = GetDualQueueWorkload();
  for (auto _ : state) {
    auto cs = RunDualQueueVectorIndexedRotate(wl);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(wl.active_indices.size()));
}
BENCHMARK(BM_Story1B_DualQueue_VectorIndexedMemmove);

void BM_Story1B_DualQueue_VectorOfPointersRotate(benchmark::State& state) {
  const auto& wl = GetDualQueueWorkload();
  for (auto _ : state) {
    auto cs = RunDualQueueVectorOfPointers(wl);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(wl.active_indices.size()));
}
BENCHMARK(BM_Story1B_DualQueue_VectorOfPointersRotate);

// =============================================================================
// Story 2B: High-Volume FIFO Stream (`std::deque` vs. Contiguous Rings)
// =============================================================================

void BM_Story2B_FifoStream_StdDeque(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunFifoStreamStdDeque(1'024, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_FifoStream_StdDeque);

void BM_Story2B_FifoStream_BoostCircularBuffer(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunFifoStreamBoostCircular(1'024, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_FifoStream_BoostCircularBuffer);

void BM_Story2B_FifoStream_CompactRingDeque(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunFifoStreamCompactRing(1'024, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_FifoStream_CompactRingDeque);

void BM_Story2B_MuxGateway1024_StdDeque(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunMultiplexedGatewayFifo<std::deque<OutboundMessage>>(1'024, 16,
                                                                     1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_MuxGateway1024_StdDeque);

void BM_Story2B_MuxGateway1024_CompactRingDeque(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunMultiplexedGatewayFifo<CompactRingDeque<OutboundMessage>>(
        1'024, 16, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_MuxGateway1024_CompactRingDeque);

// =============================================================================
// Story 3: Slot-Indexed Session Reconnect Wave (`150,000` Sessions)
// =============================================================================

void BM_Story3_Sessions_RuleOfZeroStdQueue(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m =
        RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>>(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_RuleOfZeroStdQueue)->Arg(150'000);

void BM_Story3_Sessions_RuleOfZeroStdQueueReserved(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m =
        RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>, true>(
            n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_RuleOfZeroStdQueueReserved)->Arg(150'000);

void BM_Story3_Sessions_MissingNoexceptMove(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m = RunSessionTableGrowth<std::vector<SessionMissingNoexceptMove>>(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_MissingNoexceptMove)->Arg(150'000);

void BM_Story3_Sessions_NoexceptRingQueue(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m = RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>>(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_NoexceptRingQueue)->Arg(150'000);

void BM_Story3_Sessions_VectorUniquePtr(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m = RunSessionPointerVectorGrowth(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_VectorUniquePtr)->Arg(150'000);

void BM_Story3_Sessions_FollyFbVectorSameStruct(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m =
        RunSessionTableGrowth<folly::fbvector<SessionNoexceptRingQueue>>(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_FollyFbVectorSameStruct)->Arg(150'000);

void BM_Story3_Sessions_NoexceptRingQueueReserved(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m =
        RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>, true>(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_NoexceptRingQueueReserved)->Arg(150'000);

void BM_Story3_Sessions_FollyFbVectorRelocatable(benchmark::State& state) {
  const std::size_t n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    auto m = RunSessionTableGrowth<folly::fbvector<SessionRelocatable>>(n);
    benchmark::DoNotOptimize(m);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(n));
}
BENCHMARK(BM_Story3_Sessions_FollyFbVectorRelocatable)->Arg(150'000);

}  // namespace
}  // namespace hotpath::episode17
