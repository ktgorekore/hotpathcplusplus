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
 * @file challenge.cc
 * @brief Episode 17 standalone bare-metal forensic harness for
 *        `RealtimeGatewayService` across Stories 1, 2, and 3 (multi-trial
 *        medians, isolated `VmRSS`, and unmultiplexed `perf stat` PMU modes).
 */

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "boost/circular_buffer.hpp"
#include "common/sequence_containers/sequence_containers.h"
#include "folly/FBVector.h"

ABSL_FLAG(std::string, mode, "all",
          "Workload mode: all, story1a, story1b, story2a_rss, story2b, story3, "
          "perf_s1_vec, perf_s1_list, perf_s1b_intrusive, perf_s1b_vecidx, "
          "perf_s1b_vecptr, perf_s2_deque, perf_s2_ring, perf_s3_stdqueue, "
          "perf_s3_ring");
ABSL_FLAG(int, iters, 5, "Number of timed trials (reports median)");
ABSL_FLAG(int, n, 16384, "Active subscription rule count for Story 1A / perf");

namespace hotpath::episode17 {
namespace {

using Clock = std::chrono::steady_clock;

/**
 * @brief Executes one warmup pass followed by `iters` timed trials with an
 *        untimed per-trial setup callback, returning the median duration.
 * @tparam SetupFn Callable preparing state before each timed trial.
 * @tparam RunFn Callable returning a 64-bit verification checksum.
 * @param iters Number of timed trials.
 * @param setup Untimed state-reset callback invoked before each trial.
 * @param run Timed kernel returning a deterministic verification checksum.
 * @return Pair of `{median_ms, last_checksum}`.
 */
template <typename SetupFn, typename RunFn>
std::pair<double, std::uint64_t> MeasureMedianWithSetupMs(int iters,
                                                          SetupFn&& setup,
                                                          RunFn&& run) {
  setup();
  volatile std::uint64_t sink = run();  // Warmup.
  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(iters));
  std::uint64_t last_cs = static_cast<std::uint64_t>(sink);
  for (int i = 0; i < iters; ++i) {
    setup();
    const auto t0 = Clock::now();
    last_cs = run();
    const auto t1 = Clock::now();
    sink = last_cs;
    samples.push_back(
        std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(samples.begin(), samples.end());
  return {samples[samples.size() / 2U], last_cs};
}

/**
 * @brief Executes one warmup pass followed by `iters` timed trials without
 *        per-trial setup, returning the median duration in milliseconds.
 * @tparam Fn Callable returning a 64-bit verification checksum.
 * @param iters Number of timed trials.
 * @param fn Timed kernel returning a deterministic verification checksum.
 * @return Pair of `{median_ms, last_checksum}`.
 */
template <typename Fn>
std::pair<double, std::uint64_t> MeasureMedianMs(int iters, Fn&& fn) {
  return MeasureMedianWithSetupMs(iters, [] {}, std::forward<Fn>(fn));
}

/**
 * @brief Runs Story 1A aged-heap subscription rule sweeps (`N = 1K, 4K, 16K`)
 *        and the six-strategy mid-table unsubscribe showdown (`N = 8,192`).
 * @param iters Number of timed trials per configuration (reports median).
 */
void RunStory1A(int iters) {
  LOG(INFO) << "=== STORY 1A: Active Alert-Routing Subscription Table Scan & "
               "Mid-Table Unsubscribe (Aged Heap) ===";
  for (std::size_t n : {1'024U, 4'096U, 16'384U}) {
    AgedRuleTableFixture fx(n);
    constexpr int kScanReps = 500;
    const double total_evals =
        static_cast<double>(kScanReps) * static_cast<double>(n);
    auto [vec_ms, vec_cs] = MeasureMedianMs(iters, [&] {
      std::uint64_t sum = 0;
      for (int r = 0; r < kScanReps; ++r) {
        for (const auto& rule : fx.vec_rules) {
          sum += EvaluateRuleScore(rule);
        }
      }
      return sum;
    });
    auto [list_ms, list_cs] = MeasureMedianMs(iters, [&] {
      std::uint64_t sum = 0;
      for (int r = 0; r < kScanReps; ++r) {
        for (const auto& rule : fx.list_rules) {
          sum += EvaluateRuleScore(rule);
        }
      }
      return sum;
    });
    auto [intr_ms, intr_cs] = MeasureMedianMs(iters, [&] {
      std::uint64_t sum = 0;
      for (int r = 0; r < kScanReps; ++r) {
        for (const auto& node : fx.intrusive_list) {
          sum += EvaluateRuleScore(node.rule);
        }
      }
      return sum;
    });
    const double vec_mops = (total_evals / (vec_ms / 1000.0)) / 1e6;
    const double list_mops = (total_evals / (list_ms / 1000.0)) / 1e6;
    LOG(INFO) << absl::StrFormat(
        "  [Aged Scan x%d, N=%5zu] vec=%7.3f ms (%6.1f M/s) | list=%7.3f ms "
        "(%5.1f M/s, %.2fx slower) | intrusive=%7.3f ms | parity=%s",
        kScanReps, n, vec_ms, vec_mops, list_ms, list_mops, list_ms / vec_ms,
        intr_ms, (vec_cs == list_cs && vec_cs == intr_cs) ? "OK" : "MISMATCH");
  }

  constexpr std::size_t kDepth = 8'192;
  constexpr std::size_t kUnsubs = 512;
  const auto wl = BuildRuleTableWorkload(kDepth, kUnsubs, 8);
  PreAgedListUnsubscribeState aged(wl);

  std::list<SubscriptionRule> list_lin = aged.prototype_list;
  auto [ll_ms, ll_cs] = MeasureMedianWithSetupMs(
      iters, [&] { PreAgedListUnsubscribeState::RestoreInPlace(list_lin, wl); },
      [&] { return RunRuleTableStdListLinear(list_lin, wl); });

  std::list<SubscriptionRule> list_idx = aged.prototype_list;
  absl::flat_hash_map<std::uint64_t, std::list<SubscriptionRule>::iterator>
      it_map;
  it_map.reserve(kDepth * 2U);
  auto [li_ms, li_cs] = MeasureMedianWithSetupMs(
      iters,
      [&] {
        PreAgedListUnsubscribeState::RestoreInPlace(list_idx, wl);
        it_map.clear();
        for (auto it = list_idx.begin(); it != list_idx.end(); ++it) {
          it_map.emplace(it->rule_id, it);
        }
      },
      [&] { return RunRuleTableStdListIndexed(list_idx, it_map, wl); });

  std::vector<SubscriptionRule> vec_ord;
  vec_ord.reserve(kDepth);
  auto [vo_ms, vo_cs] = MeasureMedianWithSetupMs(
      iters, [&] { vec_ord = wl.initial_rules; },
      [&] { return RunRuleTableVectorOrderedErase(vec_ord, wl); });

  std::vector<SubscriptionRule> vec_swap;
  vec_swap.reserve(kDepth);
  auto [vs_ms, vs_cs] = MeasureMedianWithSetupMs(
      iters, [&] { vec_swap = wl.initial_rules; },
      [&] { return RunRuleTableVectorSwapPop(vec_swap, wl); });

  std::vector<SubscriptionRule> vec_sparse;
  vec_sparse.reserve(kDepth);
  absl::flat_hash_map<std::uint64_t, std::size_t> sparse_map;
  sparse_map.reserve(kDepth * 2U);
  auto [vsp_ms, vsp_cs] = MeasureMedianWithSetupMs(
      iters,
      [&] {
        vec_sparse = wl.initial_rules;
        sparse_map.clear();
        for (std::size_t i = 0; i < vec_sparse.size(); ++i) {
          sparse_map.emplace(vec_sparse[i].rule_id, i);
        }
      },
      [&] {
        return RunRuleTableVectorIndexedSwapPop(vec_sparse, sparse_map, wl);
      });

  std::vector<SubscriptionRule> vec_tomb;
  vec_tomb.reserve(kDepth + 256U);
  absl::flat_hash_map<std::uint64_t, std::size_t> pos_map;
  pos_map.reserve(kDepth * 2U);
  auto [vt_ms, vt_cs] = MeasureMedianWithSetupMs(
      iters,
      [&] {
        vec_tomb = wl.initial_rules;
        vec_tomb.reserve(kDepth + 256U);
        pos_map.clear();
        for (std::size_t i = 0; i < vec_tomb.size(); ++i) {
          pos_map.emplace(vec_tomb[i].rule_id, i);
        }
      },
      [&] {
        return RunRuleTableVectorTombstoneCompact(vec_tomb, pos_map, wl);
      });

  const bool ok = (ll_cs == li_cs) && (ll_cs == vo_cs) && (ll_cs == vs_cs) &&
                  (ll_cs == vsp_cs) && (ll_cs == vt_cs);
  LOG(INFO) << absl::StrFormat(
      "  [N=%zu, %zu Unsubs + 65 Sweeps, Parity=%s, cs=%016llx]", kDepth,
      kUnsubs, ok ? "OK" : "MISMATCH", static_cast<unsigned long long>(vsp_cs));
  LOG(INFO) << absl::StrFormat(
      "    std::list (linear search + erase)          : %7.3f ms (%.2fx vs "
      "sparse-set)",
      ll_ms, ll_ms / vsp_ms);
  LOG(INFO) << absl::StrFormat(
      "    std::list + flat_hash_map (O(1) erase)     : %7.3f ms (%.2fx vs "
      "sparse-set)",
      li_ms, li_ms / vsp_ms);
  LOG(INFO) << absl::StrFormat(
      "    std::vector (ordered memmove erase)        : %7.3f ms", vo_ms);
  LOG(INFO) << absl::StrFormat(
      "    std::vector (unindexed swap-and-pop)       : %7.3f ms", vs_ms);
  LOG(INFO) << absl::StrFormat(
      "    std::vector (O(1) tombstone + compact)     : %7.3f ms (order-safe)",
      vt_ms);
  LOG(INFO) << absl::StrFormat(
      "    std::vector + map (Sparse-Set O(1) swap)   : %7.3f ms (unordered)",
      vsp_ms);
}

/**
 * @brief Runs Story 1B dual-queue LRU + timeout promotion across `16,384`
 *        pinned `256B` connections and `200,000` skewed events.
 * @param iters Number of timed trials per engine (reports median).
 */
void RunStory1B(int iters) {
  const auto wl = BuildDualQueueEventWorkload(16'384, 200'000);
  LOG(INFO) << "=== STORY 1B: Where Intrusive Lists Win (16,384 Pinned 256B "
               "Connections, Dual LRU + Timeout, 200K Events) ===";
  auto [intr_ms, intr_cs] =
      MeasureMedianMs(iters, [&] { return RunDualQueueIntrusive(wl); });
  auto [list_ms, list_cs] =
      MeasureMedianMs(iters, [&] { return RunDualQueueStdListSplice(wl); });
  auto [vidx_ms, vidx_cs] = MeasureMedianMs(
      iters, [&] { return RunDualQueueVectorIndexedRotate(wl); });
  auto [vec_ms, vec_cs] =
      MeasureMedianMs(iters, [&] { return RunDualQueueVectorOfPointers(wl); });
  const double ev = static_cast<double>(wl.active_indices.size());
  LOG(INFO) << absl::StrFormat(
      "  boost::intrusive::list (dual hooks)      | median=%7.3f ms | %6.2f M "
      "ev/s | cs=%016llx",
      intr_ms, (ev / (intr_ms / 1000.0)) / 1e6,
      static_cast<unsigned long long>(intr_cs));
  LOG(INFO) << absl::StrFormat(
      "  std::list<ActiveConnection*> (splice)    | median=%7.3f ms | %6.2f M "
      "ev/s (%.2fx slower) | cs=%016llx",
      list_ms, (ev / (list_ms / 1000.0)) / 1e6, list_ms / intr_ms,
      static_cast<unsigned long long>(list_cs));
  LOG(INFO) << absl::StrFormat(
      "  std::vector<u32> (O(1) idx + memmove)    | median=%7.3f ms | %6.2f M "
      "ev/s (%.2fx slower) | cs=%016llx",
      vidx_ms, (ev / (vidx_ms / 1000.0)) / 1e6, vidx_ms / intr_ms,
      static_cast<unsigned long long>(vidx_cs));
  LOG(INFO) << absl::StrFormat(
      "  std::vector<ActiveConnection*> (find+rot)| median=%7.3f ms | %6.2f M "
      "ev/s (%.2fx slower) | cs=%016llx",
      vec_ms, (ev / (vec_ms / 1000.0)) / 1e6, vec_ms / intr_ms,
      static_cast<unsigned long long>(vec_cs));
}

/**
 * @brief Runs Story 2A `VmRSS` memory footprint comparison across `100,000`
 *        idle per-connection outbound queues.
 */
void RunStory2ARss() {
  constexpr std::size_t kQueues = 100'000;
  LOG(INFO) << absl::StrFormat(
      "=== STORY 2A: VmRSS Snapshot for %zu Idle Per-Connection Outbound "
      "Queues ===",
      kQueues);
  const std::uint64_t rss0 = ReadProcessVmRssBytes();
  {
    std::vector<CompactRingDeque<OutboundMessage>> rings(kQueues);
    const std::uint64_t rss_ring = ReadProcessVmRssBytes();
    LOG(INFO) << absl::StrFormat(
        "  CompactRingDeque (100K empty)               : +%6.2f MiB VmRSS "
        "(sizeof=%zuB)",
        static_cast<double>(rss_ring > rss0 ? rss_ring - rss0 : 0) /
            (1024.0 * 1024.0),
        sizeof(CompactRingDeque<OutboundMessage>));
  }
  const std::uint64_t rss1 = ReadProcessVmRssBytes();
  {
    using SpaceOptRing =
        boost::circular_buffer_space_optimized<OutboundMessage>;
    std::vector<SpaceOptRing> opt_rings(
        kQueues, SpaceOptRing(SpaceOptRing::capacity_type(1'024, 0)));
    const std::uint64_t rss_opt = ReadProcessVmRssBytes();
    LOG(INFO) << absl::StrFormat(
        "  boost::circular_buffer_space_optimized(100K): +%6.2f MiB VmRSS "
        "(sizeof=%zuB)",
        static_cast<double>(rss_opt > rss1 ? rss_opt - rss1 : 0) /
            (1024.0 * 1024.0),
        sizeof(boost::circular_buffer_space_optimized<OutboundMessage>));
  }
  const std::uint64_t rss2 = ReadProcessVmRssBytes();
  {
    std::vector<std::deque<OutboundMessage>> deques(kQueues);
    const std::uint64_t rss_deq = ReadProcessVmRssBytes();
    LOG(INFO) << absl::StrFormat(
        "  std::deque (100K empty, libstdc++ 512B+64B) : +%6.2f MiB VmRSS "
        "(sizeof=%zuB)",
        static_cast<double>(rss_deq > rss2 ? rss_deq - rss2 : 0) /
            (1024.0 * 1024.0),
        sizeof(std::deque<OutboundMessage>));
  }
}

/**
 * @brief Runs Story 2B `8,000,000`-message FIFO streams (single-channel depth
 *        `1,024` and `1,024`-socket multiplexed gateway).
 * @param iters Number of timed trials per engine (reports median).
 */
void RunStory2B(int iters) {
  constexpr std::size_t kDepth = 1'024;
  constexpr std::size_t kCycles = 8'000'000;
  LOG(INFO) << absl::StrFormat(
      "=== STORY 2B: Steady-State FIFO Stream (%zu push+pop @ depth %zu & "
      "1,024-Conn Multiplexed) ===",
      kCycles, kDepth);
  auto report = [&](const char* label, auto fn) {
    const auto [ms, cs] = MeasureMedianMs(iters, fn);
    const double mops = (static_cast<double>(kCycles) / (ms / 1000.0)) / 1e6;
    LOG(INFO) << absl::StrFormat(
        "  %-42s | median=%7.3f ms | %6.2f M msgs/s | cs=%016llx", label, ms,
        mops, static_cast<unsigned long long>(cs));
  };
  report("Single Queue: std::deque (libstdc++)",
         [&] { return RunFifoStreamStdDeque(kDepth, kCycles); });
  report("Single Queue: boost::circular_buffer",
         [&] { return RunFifoStreamBoostCircular(kDepth, kCycles); });
  report("Single Queue: CompactRingDeque",
         [&] { return RunFifoStreamCompactRing(kDepth, kCycles); });
  report("1,024 Queues: std::deque (libstdc++)", [&] {
    return RunMultiplexedGatewayFifo<std::deque<OutboundMessage>>(1'024, 16,
                                                                  kCycles);
  });
  report("1,024 Queues: CompactRingDeque", [&] {
    return RunMultiplexedGatewayFifo<CompactRingDeque<OutboundMessage>>(
        1'024, 16, kCycles);
  });
}

/**
 * @brief Runs Story 3 `150,000`-session reconnect wave comparing Rule-of-Zero
 *        `std::queue`, missing `noexcept`, `CompactRingDeque`, pointer
 *        indirection, and `folly::fbvector` (`1.5x` & `IsRelocatable`).
 * @param iters Number of timed trials per engine (reports median).
 */
void RunStory3(int iters) {
  constexpr std::size_t kSessions = 150'000;
  LOG(INFO) << absl::StrFormat(
      "=== STORY 3: Gateway Reconnect Wave — %zu Slot-Indexed ClientSessions "
      "===",
      kSessions);
  auto report = [&](const char* label, bool nothrow_move, auto fn) {
    SessionGrowthMetrics last{};
    const auto [ms, cs] = MeasureMedianMs(iters, [&] {
      last = fn();
      return last.checksum;
    });
    const double msess = (static_cast<double>(kSessions) / (ms / 1000.0)) / 1e6;
    LOG(INFO) << absl::StrFormat(
        "  %-45s | nothrow=%-5s | median=%7.3f ms | %5.2f M/s | cap=%6zu | "
        "cs=%016llx",
        label, nothrow_move ? "true" : "FALSE", ms, msess, last.final_capacity,
        static_cast<unsigned long long>(cs));
  };
  report("std::vector + std::queue (unreserved)",
         std::is_nothrow_move_constructible_v<SessionRuleOfZeroWithStdQueue>,
         [] {
           return RunSessionTableGrowth<
               std::vector<SessionRuleOfZeroWithStdQueue>>(kSessions);
         });
  report(
      "std::vector + std::queue (.reserve(150K))",
      std::is_nothrow_move_constructible_v<SessionRuleOfZeroWithStdQueue>, [] {
        return RunSessionTableGrowth<std::vector<SessionRuleOfZeroWithStdQueue>,
                                     true>(kSessions);
      });
  report(
      "std::vector + Custom Move (missing noexcept)",
      std::is_nothrow_move_constructible_v<SessionMissingNoexceptMove>, [] {
        return RunSessionTableGrowth<std::vector<SessionMissingNoexceptMove>>(
            kSessions);
      });
  report("std::vector + CompactRingDeque (2.0x growth)",
         std::is_nothrow_move_constructible_v<SessionNoexceptRingQueue>, [] {
           return RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>>(
               kSessions);
         });
  report("std::vector<unique_ptr<Session>> (unreserved)", true,
         [] { return RunSessionPointerVectorGrowth(kSessions); });
  report(
      "folly::fbvector + CompactRingDeque (1.5x)",
      std::is_nothrow_move_constructible_v<SessionNoexceptRingQueue>, [] {
        return RunSessionTableGrowth<folly::fbvector<SessionNoexceptRingQueue>>(
            kSessions);
      });
  report("std::vector + CompactRingDeque (.reserve)",
         std::is_nothrow_move_constructible_v<SessionNoexceptRingQueue>, [] {
           return RunSessionTableGrowth<std::vector<SessionNoexceptRingQueue>,
                                        true>(kSessions);
         });
  report("folly::fbvector + IsRelocatable (1.5x)",
         std::is_nothrow_move_constructible_v<SessionRelocatable>, [] {
           return RunSessionTableGrowth<folly::fbvector<SessionRelocatable>>(
               kSessions);
         });
}

}  // namespace
}  // namespace hotpath::episode17

/**
 * @brief Entry point initializing Abseil flags/logging and dispatching the
 *        requested Episode 17 forensic workload or Linux `perf stat` mode.
 * @param argc Command-line argument count.
 * @param argv Command-line argument vector.
 * @return Process exit code (`0` on success, `1` on unknown `--mode`).
 */
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  const std::string mode = absl::GetFlag(FLAGS_mode);
  const int iters = absl::GetFlag(FLAGS_iters);
  const std::size_t n = static_cast<std::size_t>(absl::GetFlag(FLAGS_n));

  if (mode == "all") {
    hotpath::episode17::RunStory1A(iters);
    hotpath::episode17::RunStory1B(iters);
    hotpath::episode17::RunStory2ARss();
    hotpath::episode17::RunStory2B(iters);
    hotpath::episode17::RunStory3(iters);
  } else if (mode == "story1a") {
    hotpath::episode17::RunStory1A(iters);
  } else if (mode == "story1b") {
    hotpath::episode17::RunStory1B(iters);
  } else if (mode == "story2a_rss") {
    hotpath::episode17::RunStory2ARss();
  } else if (mode == "story2b") {
    hotpath::episode17::RunStory2B(iters);
  } else if (mode == "story3") {
    hotpath::episode17::RunStory3(iters);
  } else if (mode == "perf_s1_vec") {
    hotpath::episode17::AgedRuleTableFixture fx(n);
    volatile std::uint64_t cs = 0;
    for (int i = 0; i < iters * 2'000; ++i) {
      for (const auto& rule : fx.vec_rules) {
        cs += hotpath::episode17::EvaluateRuleScore(rule);
      }
    }
    LOG(INFO) << "perf_s1_vec cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s1_list") {
    hotpath::episode17::AgedRuleTableFixture fx(n);
    volatile std::uint64_t cs = 0;
    for (int i = 0; i < iters * 2'000; ++i) {
      for (const auto& rule : fx.list_rules) {
        cs += hotpath::episode17::EvaluateRuleScore(rule);
      }
    }
    LOG(INFO) << "perf_s1_list cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s1b_intrusive") {
    const auto wl =
        hotpath::episode17::BuildDualQueueEventWorkload(16'384, 200'000);
    volatile std::uint64_t cs = 0x17BULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs, hotpath::episode17::RunDualQueueIntrusive(wl));
    }
    LOG(INFO) << "perf_s1b_intrusive cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s1b_vecidx") {
    const auto wl =
        hotpath::episode17::BuildDualQueueEventWorkload(16'384, 200'000);
    volatile std::uint64_t cs = 0x17BULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs, hotpath::episode17::RunDualQueueVectorIndexedRotate(wl));
    }
    LOG(INFO) << "perf_s1b_vecidx cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s1b_vecptr") {
    const auto wl =
        hotpath::episode17::BuildDualQueueEventWorkload(16'384, 200'000);
    volatile std::uint64_t cs = 0x17BULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs, hotpath::episode17::RunDualQueueVectorOfPointers(wl));
    }
    LOG(INFO) << "perf_s1b_vecptr cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s2_deque") {
    volatile std::uint64_t cs = 0x172ULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs, hotpath::episode17::RunMultiplexedGatewayFifo<
                  std::deque<hotpath::episode17::OutboundMessage>>(1'024, 16,
                                                                   8'000'000));
    }
    LOG(INFO) << "perf_s2_deque cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s2_ring") {
    volatile std::uint64_t cs = 0x172ULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs,
          hotpath::episode17::RunMultiplexedGatewayFifo<
              hotpath::episode17::CompactRingDeque<
                  hotpath::episode17::OutboundMessage>>(1'024, 16, 8'000'000));
    }
    LOG(INFO) << "perf_s2_ring cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s3_stdqueue") {
    volatile std::uint64_t cs = 0x173ULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs,
          hotpath::episode17::RunSessionTableGrowth<
              std::vector<hotpath::episode17::SessionRuleOfZeroWithStdQueue>>(
              150'000)
              .checksum);
    }
    LOG(INFO) << "perf_s3_stdqueue cs=" << static_cast<std::uint64_t>(cs);
  } else if (mode == "perf_s3_ring") {
    volatile std::uint64_t cs = 0x173ULL;
    for (int i = 0; i < iters; ++i) {
      cs = hotpath::episode17::MixOrderHash(
          cs, hotpath::episode17::RunSessionTableGrowth<
                  std::vector<hotpath::episode17::SessionNoexceptRingQueue>>(
                  150'000)
                  .checksum);
    }
    LOG(INFO) << "perf_s3_ring cs=" << static_cast<std::uint64_t>(cs);
  } else {
    LOG(ERROR) << "Unknown --mode=" << mode;
    return 1;
  }
  return 0;
}
