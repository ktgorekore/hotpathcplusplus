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

#include <cstddef>
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

/**
 * @brief Story 1A: full-table predicate evaluation sweep over an aged
 *        `std::vector<SubscriptionRule>`.
 * @param state Google Benchmark runner state (`state.range(0)` = rule count).
 */
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

/**
 * @brief Story 1A: full-table predicate evaluation sweep over an aged
 *        `std::list<SubscriptionRule>`.
 * @param state Google Benchmark runner state (`state.range(0)` = rule count).
 */
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

/**
 * @brief Story 1A: full-table predicate evaluation sweep over an aged
 *        `boost::intrusive::list<IntrusiveRuleNode>`.
 * @param state Google Benchmark runner state (`state.range(0)` = rule count).
 */
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

/**
 * @brief Story 1A: mid-table rule unsubscribes + periodic sweeps on a pre-aged
 *        `std::list` using linear search + `O(1)` erase.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1A: mid-table rule unsubscribes + periodic sweeps on a pre-aged
 *        `std::list` paired with an `O(1)` `absl::flat_hash_map` iterator
 *        index.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1A: mid-table rule unsubscribes + periodic sweeps on a
 *        contiguous `std::vector` using order-preserving `erase`.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1A: mid-table rule unsubscribes + periodic sweeps on a
 *        contiguous `std::vector` using unindexed swap-and-pop.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1A: mid-table rule unsubscribes + periodic sweeps using the
 *        Sparse-Set pattern (`std::vector` + `absl::flat_hash_map` + `O(1)`
 *        swap-and-pop).
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1A: order-preserving mid-table rule unsubscribes + periodic
 *        sweeps on `std::vector` using indexed tombstones and batch
 *        `std::erase_if` compaction.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Returns lazily constructed deterministic Story 1B dual-queue event
 *        workload (`16,384` connections, `200,000` skewed events).
 * @return Const reference to process-lifetime `DualQueueEventWorkload`.
 */
const DualQueueEventWorkload& GetDualQueueWorkload() {
  static const DualQueueEventWorkload wl =
      BuildDualQueueEventWorkload(16'384, 200'000);
  return wl;
}

/**
 * @brief Story 1B: dual LRU + timeout promotion across `16,384` pinned `256B`
 *        connections using embedded `boost::intrusive::list` hooks.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1B: dual LRU + timeout promotion using two
 *        `std::list<ActiveConnection*>` queues with stored iterators
 *        (`splice`).
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1B: dual LRU + timeout promotion using `O(1)` indexed
 *        `std::vector<std::uint32_t>` + `std::memmove` (zero linear search).
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 1B: dual LRU + timeout promotion using unindexed
 *        `std::vector<ActiveConnection*>` (`std::find` + `std::rotate`).
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 2B: single-channel `1,000,000`-message FIFO stream at depth
 *        `1,024` on `std::deque<OutboundMessage>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story2B_FifoStream_StdDeque(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunFifoStreamStdDeque(1'024, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_FifoStream_StdDeque);

/**
 * @brief Story 2B: single-channel `1,000,000`-message FIFO stream at depth
 *        `1,024` on bounded `boost::circular_buffer<OutboundMessage>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story2B_FifoStream_BoostCircularBuffer(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunFifoStreamBoostCircular(1'024, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_FifoStream_BoostCircularBuffer);

/**
 * @brief Story 2B: single-channel `1,000,000`-message FIFO stream at depth
 *        `1,024` on growable `CompactRingDeque<OutboundMessage>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story2B_FifoStream_CompactRingDeque(benchmark::State& state) {
  for (auto _ : state) {
    auto cs = RunFifoStreamCompactRing(1'024, 1'000'000);
    benchmark::DoNotOptimize(cs);
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          1'000'000);
}
BENCHMARK(BM_Story2B_FifoStream_CompactRingDeque);

/**
 * @brief Story 2B: `1,000,000`-message FIFO stream multiplexed across `1,024`
 *        active `std::deque<OutboundMessage>` socket queues.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 2B: `1,000,000`-message FIFO stream multiplexed across `1,024`
 *        active `CompactRingDeque<OutboundMessage>` socket queues.
 * @param state Google Benchmark runner state.
 */
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

/**
 * @brief Story 3: unreserved `std::vector` growth for Rule-of-Zero sessions
 *        embedding `std::queue` (`noexcept(false)` move on `libstdc++`).
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: pre-reserved `std::vector` control for Rule-of-Zero sessions
 *        embedding `std::queue` (isolating `std::deque` move allocations).
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: unreserved `std::vector` growth for sessions with a custom
 *        move constructor omitting `noexcept` (`std::move_if_noexcept` copy
 *        fallback).
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: unreserved `std::vector` (`2.0x`) growth for Rule-of-Zero
 *        `noexcept`-movable `SessionNoexceptRingQueue` entries.
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: pointer-indirection control storing heap-allocated sessions
 *        in `std::vector<std::unique_ptr<SessionNoexceptRingQueue>>`.
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: unreserved `folly::fbvector` (`1.5x` growth) with identical
 *        `SessionNoexceptRingQueue` elements.
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: pre-reserved `std::vector` growth for `noexcept`-movable
 *        `SessionNoexceptRingQueue` entries.
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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

/**
 * @brief Story 3: unreserved `folly::fbvector` (`1.5x` growth) with
 *        bitwise-relocatable `SessionRelocatable` (`folly::IsRelocatable`).
 * @param state Google Benchmark runner state (`state.range(0)` = sessions).
 */
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
