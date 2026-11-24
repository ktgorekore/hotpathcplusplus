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
 * @brief Episode 16 standalone forensic runner, isolated VmRSS auditor,
 *        eviction-churn tail profiler, and Linux `perf stat` driver.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "common/cpp_hash_maps/cpp_hash_maps.h"

ABSL_FLAG(std::string, mode, "all",
          "Forensic workload mode (all, story1, story2, story3_5, story4, "
          "s1_noexcept_demo, s1_shared_hash_control, s1_*, rss_*, "
          "s3_iter256_*, s4_churn_*).");
ABSL_FLAG(std::size_t, iters, 0,
          "Iteration or eviction-cycle count override (0 uses mode default).");
ABSL_FLAG(std::size_t, count, hotpath::episode16::kNumLargeEntriesPostDouble,
          "Entry count for isolated VmRSS or Story 4 churn-band workloads.");
ABSL_FLAG(bool, quiet, false,
          "Suppress informational LOG(INFO) output (for clean perf stat).");

namespace hotpath::episode16 {
namespace {

using Clock = std::chrono::steady_clock;

/**
 * @brief Compiler optimization barrier preventing dead-code elimination.
 * @tparam T Value type to keep live.
 * @param val Mutable reference to observe in inline assembly.
 */
template <typename T>
inline void DoNotOptimize(T& val) {
  asm volatile("" : "+r,m"(val) : : "memory");
}

/**
 * @brief Logs a formatted throughput row via Abseil `LOG(INFO)`.
 * @param label Human-readable workload/engine label.
 * @param total_ms Elapsed total milliseconds.
 * @param total_ops Total completed operations.
 * @param checksum Verification checksum proving bit-for-bit parity.
 */
void LogRateRow(std::string_view label, double total_ms, std::size_t total_ops,
                std::uint64_t checksum) {
  const double ns_per_op =
      (total_ms * 1'000'000.0) / static_cast<double>(total_ops);
  const double mops = static_cast<double>(total_ops) / (total_ms * 1'000.0);
  LOG(INFO) << absl::StrFormat(
      "%-46s | %7.2f ns/op | %7.2f M/s | checksum=0x%x", label, ns_per_op, mops,
      checksum);
}

/**
 * @brief Logs a formatted full-table iteration sweep row via Abseil
 * `LOG(INFO)`.
 * @param label Human-readable workload/engine label.
 * @param total_ms Elapsed total milliseconds across all sweeps.
 * @param sweeps Number of full-table sweeps executed.
 * @param live_entries Number of live entries visited per sweep.
 * @param checksum Verification checksum proving bit-for-bit parity.
 */
void LogSweepRow(std::string_view label, double total_ms, std::size_t sweeps,
                 std::size_t live_entries, std::uint64_t checksum) {
  const double ms_per_sweep = total_ms / static_cast<double>(sweeps);
  const double mops =
      static_cast<double>(sweeps * live_entries) / (total_ms * 1'000.0);
  LOG(INFO) << absl::StrFormat(
      "%-46s | %7.2f ms/swp | %7.2f M/s | checksum=0x%x", label, ms_per_sweep,
      mops, checksum);
}

// -----------------------------------------------------------------------------
// Story 1: Envoy HTTP/RPC Route Lookup (Non-SSO Strings + Transparent Lookup)
// -----------------------------------------------------------------------------

/**
 * @brief Times repeated batches of route-table lookups and logs throughput.
 * @tparam MapT Route table container type.
 * @tparam FindFn Lookup kernel `(const MapT&, const string_view*, size_t)`.
 * @param label Formatted row label.
 * @param map Populated route table.
 * @param queries Span of zero-copy route slices.
 * @param iters Number of timed batch passes.
 * @param find_fn Lookup kernel functor.
 */
template <typename MapT, typename FindFn>
void TimeRouteLookups(std::string_view label, const MapT& map,
                      absl::Span<const std::string_view> queries,
                      std::size_t iters, FindFn&& find_fn) {
  const std::string_view* q_ptr = queries.data();
  const std::size_t q_len = queries.size();
  std::uint64_t sink = find_fn(map, q_ptr, q_len);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += find_fn(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  LogRateRow(label, ms, iters * q_len, sink);
}

/**
 * @brief Runs the complete Story 1 route lookup suite (Zipfian, Uniform, 404).
 * @param iters Number of 1M-query passes per engine and traffic distribution.
 */
void RunStory1All(std::size_t iters) {
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
  LOG(INFO) << absl::StrFormat(
      "=== Story 1: Envoy HTTP/RPC Route Table (%u Non-SSO 78-82B Routes, "
      "Aged Heap) ===",
      kNumRoutes);

  const auto m_std_def = BuildAgedRouteMap<StdRouteMapDefault>(ds);
  const auto m_std_tr = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto m_std_tr_noex =
      BuildAgedRouteMap<StdRouteMapTransparentNoexcept>(ds);
  const auto pmr = BuildPmrRouteMap(ds);
  const auto m_absl = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
  const auto m_f14_fast = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
  const auto m_f14_val = BuildAgedRouteMap<FollyF14ValueRouteMap>(ds);
  const auto m_boost = BuildAgedRouteMap<BoostFlatRouteMap>(ds);

  LOG(INFO) << "--- 1A. Zipfian s=0.99 Hot Skew (70% Hit / 30% 404 Miss) ---";
  TimeRouteLookups("std::unordered_map (Default, tmp std::string)", m_std_def,
                   ds.queries_zipf_70_30, iters, Ep16StdDefaultFindRoute);
  TimeRouteLookups("std::unordered_map (C++20 is_transparent)", m_std_tr,
                   ds.queries_zipf_70_30, iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("std::unordered_map (noexcept hash trap!)", m_std_tr_noex,
                   ds.queries_zipf_70_30, iters,
                   Ep16StdTransparentNoexceptFindRoute);
  TimeRouteLookups("std::pmr::unordered_map (Monotonic Arena)", *pmr.map,
                   ds.queries_zipf_70_30, iters, Ep16PmrTransparentFindRoute);
  TimeRouteLookups("absl::flat_hash_map (16-way SwissTable)", m_absl,
                   ds.queries_zipf_70_30, iters, Ep16AbslFlatFindRoute);
  TimeRouteLookups("folly::F14FastMap (14-way F14VectorMap)", m_f14_fast,
                   ds.queries_zipf_70_30, iters, Ep16FollyF14FastFindRoute);
  TimeRouteLookups("folly::F14ValueMap (14-way inline)", m_f14_val,
                   ds.queries_zipf_70_30, iters, Ep16FollyF14ValueFindRoute);
  TimeRouteLookups("boost::unordered_flat_map (15-way + ofw)", m_boost,
                   ds.queries_zipf_70_30, iters, Ep16BoostFlatFindRoute);

  LOG(INFO) << "--- 1B. Uniform Aged-Heap (70% Hit / 30% 404 Miss) ---";
  TimeRouteLookups("std::unordered_map (Default, tmp std::string)", m_std_def,
                   ds.queries_uniform_70_30, iters, Ep16StdDefaultFindRoute);
  TimeRouteLookups("std::unordered_map (C++20 is_transparent)", m_std_tr,
                   ds.queries_uniform_70_30, iters,
                   Ep16StdTransparentFindRoute);
  TimeRouteLookups("std::unordered_map (noexcept hash trap!)", m_std_tr_noex,
                   ds.queries_uniform_70_30, iters,
                   Ep16StdTransparentNoexceptFindRoute);
  TimeRouteLookups("std::pmr::unordered_map (Monotonic Arena)", *pmr.map,
                   ds.queries_uniform_70_30, iters,
                   Ep16PmrTransparentFindRoute);
  TimeRouteLookups("absl::flat_hash_map (16-way SwissTable)", m_absl,
                   ds.queries_uniform_70_30, iters, Ep16AbslFlatFindRoute);
  TimeRouteLookups("folly::F14FastMap (14-way F14VectorMap)", m_f14_fast,
                   ds.queries_uniform_70_30, iters, Ep16FollyF14FastFindRoute);
  TimeRouteLookups("folly::F14ValueMap (14-way inline)", m_f14_val,
                   ds.queries_uniform_70_30, iters, Ep16FollyF14ValueFindRoute);
  TimeRouteLookups("boost::unordered_flat_map (15-way + ofw)", m_boost,
                   ds.queries_uniform_70_30, iters, Ep16BoostFlatFindRoute);

  LOG(INFO) << "--- 1C. 100% Negative Misses (Bot Scans / 404 Flood) ---";
  TimeRouteLookups("std::unordered_map (Default, tmp std::string)", m_std_def,
                   ds.queries_100_miss, iters, Ep16StdDefaultFindRoute);
  TimeRouteLookups("std::unordered_map (C++20 is_transparent)", m_std_tr,
                   ds.queries_100_miss, iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("std::unordered_map (noexcept hash trap!)", m_std_tr_noex,
                   ds.queries_100_miss, iters,
                   Ep16StdTransparentNoexceptFindRoute);
  TimeRouteLookups("std::pmr::unordered_map (Monotonic Arena)", *pmr.map,
                   ds.queries_100_miss, iters, Ep16PmrTransparentFindRoute);
  TimeRouteLookups("absl::flat_hash_map (16-way SwissTable)", m_absl,
                   ds.queries_100_miss, iters, Ep16AbslFlatFindRoute);
  TimeRouteLookups("folly::F14FastMap (14-way F14VectorMap)", m_f14_fast,
                   ds.queries_100_miss, iters, Ep16FollyF14FastFindRoute);
  TimeRouteLookups("folly::F14ValueMap (14-way inline)", m_f14_val,
                   ds.queries_100_miss, iters, Ep16FollyF14ValueFindRoute);
  TimeRouteLookups("boost::unordered_flat_map (15-way + ofw)", m_boost,
                   ds.queries_100_miss, iters, Ep16BoostFlatFindRoute);
}

/**
 * @brief Isolated forensic proof of GCC libstdc++'s `noexcept` hash-caching
 *        trap on 100K non-SSO routes (comparing allocating default vs. cached
 *        transparent vs. uncached `noexcept` transparent).
 * @param iters Number of 1M-query passes per configuration.
 */
void RunNoexceptHashTrapDemo(std::size_t iters) {
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
  LOG(INFO) << "=== GCC libstdc++ noexcept Hash-Caching Audit (100K Non-SSO "
               "78-82B Routes, 404 Misses) ===";
  const auto m_std_def = BuildAgedRouteMap<StdRouteMapDefault>(ds);
  const auto m_std_tr = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto m_std_tr_noex =
      BuildAgedRouteMap<StdRouteMapTransparentNoexcept>(ds);

  // Warm CPU turbo P-state and glibc tcache identically across all 3 maps.
  std::uint64_t warm_sink = 0;
  warm_sink ^=
      Ep16StdDefaultFindRoute(m_std_def, ds.queries_uniform_70_30.data(),
                              ds.queries_uniform_70_30.size());
  warm_sink ^=
      Ep16StdTransparentFindRoute(m_std_tr, ds.queries_uniform_70_30.data(),
                                  ds.queries_uniform_70_30.size());
  warm_sink ^= Ep16StdTransparentNoexceptFindRoute(
      m_std_tr_noex, ds.queries_uniform_70_30.data(),
      ds.queries_uniform_70_30.size());
  DoNotOptimize(warm_sink);

  TimeRouteLookups("std::unordered_map (default, heap allocs)", m_std_def,
                   ds.queries_100_miss, iters, Ep16StdDefaultFindRoute);
  TimeRouteLookups("std::unordered_map (transparent, cached)", m_std_tr,
                   ds.queries_100_miss, iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("std::unordered_map (transparent + noexcept!)",
                   m_std_tr_noex, ds.queries_100_miss, iters,
                   Ep16StdTransparentNoexceptFindRoute);
}

/**
 * @brief Runs Story 1's shared-hasher control normalizing `std::unordered_map`,
 *        `absl::flat_hash_map`, `folly::F14FastMap`, and
 *        `boost::unordered_flat_map` onto identical
 *        `std::hash<std::string_view>`.
 * @param iters Number of 1M-query passes per engine and workload stream.
 */
void RunSharedHasherControl(std::size_t iters) {
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
  const auto m_std = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto m_absl = BuildAgedRouteMap<AbslFlatRouteMapSharedHash>(ds);
  const auto m_f14 = BuildAgedRouteMap<FollyF14FastRouteMapSharedHash>(ds);
  const auto m_boost = BuildAgedRouteMap<BoostFlatRouteMapSharedHash>(ds);

  auto find_generic = [](const auto& map, const std::string_view* q,
                         std::size_t n) -> std::uint64_t {
    return FindTransparentRouteBatch(map, absl::MakeConstSpan(q, n));
  };

  LOG(INFO) << "=== Story 1 Shared-Hasher Control (Identical "
               "std::hash<std::string_view>) ===";
  LOG(INFO) << "--- Zipfian 70/30 (Normalized std::hash) ---";
  TimeRouteLookups("std::unordered_map (std::hash)", m_std,
                   ds.queries_zipf_70_30, iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("absl::flat_hash_map (std::hash)", m_absl,
                   ds.queries_zipf_70_30, iters, find_generic);
  TimeRouteLookups("folly::F14FastMap (std::hash)", m_f14,
                   ds.queries_zipf_70_30, iters, find_generic);
  TimeRouteLookups("boost::unordered_flat_map (std::hash)", m_boost,
                   ds.queries_zipf_70_30, iters, find_generic);
  LOG(INFO) << "--- 100% 404 Misses (Normalized std::hash) ---";
  TimeRouteLookups("std::unordered_map (std::hash)", m_std, ds.queries_100_miss,
                   iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("absl::flat_hash_map (std::hash)", m_absl,
                   ds.queries_100_miss, iters, find_generic);
  TimeRouteLookups("folly::F14FastMap (std::hash)", m_f14, ds.queries_100_miss,
                   iters, find_generic);
  TimeRouteLookups("boost::unordered_flat_map (std::hash)", m_boost,
                   ds.queries_100_miss, iters, find_generic);
}

// -----------------------------------------------------------------------------
// Story 2: Per-Request Small Maps Crossover Sweep (N = 4, 8, 16, 32, 64, 128)
// -----------------------------------------------------------------------------

/**
 * @brief Times repeated per-request small-map lifecycles and logs throughput.
 * @tparam Fn Callable executing one build + 12 lookup + destroy cycle.
 * @param label Formatted row label.
 * @param requests Total request lifecycles to execute.
 * @param fn Lifecycle callable returning a verification checksum.
 */
template <typename Fn>
void TimeSmallLifecycle(std::string_view label, std::size_t requests, Fn&& fn) {
  std::uint64_t sink = fn();
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < requests; ++i) {
    sink += fn();
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  LogRateRow(label, ms, requests, sink);
}

/**
 * @brief Runs Story 2's per-request HTTP/2 header crossover sweep (`N=4..128`).
 * @param requests Number of full request lifecycles per container and size.
 */
void RunStory2All(std::size_t requests) {
  LOG(INFO) << "=== Story 2: Per-Request HTTP/2 Header Maps (Build + 12 "
               "Lookups + Destroy) ===";
  for (std::size_t n : {4U, 8U, 16U, 32U, 64U, 128U}) {
    const HttpHeaderDataset ds = BuildHttpHeaderDataset(n);
    LOG(INFO) << absl::StrFormat(
        "--- N = %u Headers (%u full request lifecycles) ---", n, requests);
    TimeSmallLifecycle(
        "FlatInlinedSmallMap<16> (Stack Linear Scan)", requests, [&] {
          return RunSmallFlatMapRequestLifecycle<
              FlatInlinedSmallMap<std::string_view, std::string_view, 16>>(ds);
        });
    TimeSmallLifecycle(
        "StdVectorSmallMap (std::vector Linear Scan)", requests, [&] {
          return RunSmallFlatMapRequestLifecycle<
              StdVectorSmallMap<std::string_view, std::string_view>>(ds);
        });
    TimeSmallLifecycle(
        "std::unordered_map (reserve + 12 finds)", requests, [&] {
          return RunSmallHashMapRequestLifecycle<
              std::unordered_map<std::string_view, std::string_view>>(ds);
        });
    TimeSmallLifecycle(
        "absl::flat_hash_map (reserve + 12 finds)", requests, [&] {
          return RunSmallHashMapRequestLifecycle<
              absl::flat_hash_map<std::string_view, std::string_view>>(ds);
        });
    TimeSmallLifecycle("folly::F14FastMap (sub-chunk 2/6/14)", requests, [&] {
      return RunSmallHashMapRequestLifecycle<
          folly::F14FastMap<std::string_view, std::string_view>>(ds);
    });
    TimeSmallLifecycle(
        "boost::unordered_flat_map (reserve + 12)", requests, [&] {
          return RunSmallHashMapRequestLifecycle<
              boost::unordered_flat_map<std::string_view, std::string_view>>(
              ds);
        });
  }
}

// -----------------------------------------------------------------------------
// Story 3 & Story 5: Isolated VmRSS, Lookups & Dense Iteration (32B/128B/256B)
// -----------------------------------------------------------------------------

/**
 * @brief Measures isolated fresh-process `VmRSS` delta for a by-value map.
 * @tparam MapT Target hash map storing `ValueT` inline or in nodes.
 * @tparam FactoryFn Callable mapping `uint64_t -> ValueT`.
 * @param label Human-readable row label.
 * @param count Number of entries to populate.
 * @param make_val Deterministic value synthesizer.
 */
template <typename MapT, typename FactoryFn>
void MeasureIsolatedRss(std::string_view label, std::size_t count,
                        FactoryFn&& make_val) {
  const LargeValueDataset ds = BuildLargeValueDataset(count, 1024);
  std::uint64_t rss0 = 0;
  std::uint64_t hwm0 = 0;
  ReadProcessMemoryBytes(rss0, hwm0);

  MapT map;
  map.reserve(ds.keys.size());
  for (std::uint64_t k : ds.keys) {
    map.emplace(k, make_val(k));
  }
  std::size_t sz = map.size();
  DoNotOptimize(sz);

  std::uint64_t rss1 = 0;
  std::uint64_t hwm1 = 0;
  ReadProcessMemoryBytes(rss1, hwm1);
  const std::uint64_t delta = rss1 > rss0 ? (rss1 - rss0) : 0ULL;
  const double mib = static_cast<double>(delta) / (1024.0 * 1024.0);
  const double b_per_entry =
      static_cast<double>(delta) / static_cast<double>(count);
  LOG(INFO) << absl::StrFormat("%-48s | RSS=%6.1f MiB | %6.1f B/entry", label,
                               mib, b_per_entry);
}

/**
 * @brief Measures isolated fresh-process `VmRSS` delta for
 *        `absl::flat_hash_map<uint64_t, std::unique_ptr<ValueT>>`.
 * @tparam ValueT Target value struct type.
 * @tparam FactoryFn Callable mapping `uint64_t -> ValueT`.
 * @param label Human-readable row label.
 * @param count Number of entries to populate.
 * @param make_val Deterministic value synthesizer.
 */
template <typename ValueT, typename FactoryFn>
void MeasureIsolatedUniquePtrRss(std::string_view label, std::size_t count,
                                 FactoryFn&& make_val) {
  const LargeValueDataset ds = BuildLargeValueDataset(count, 1024);
  std::uint64_t rss0 = 0;
  std::uint64_t hwm0 = 0;
  ReadProcessMemoryBytes(rss0, hwm0);

  absl::flat_hash_map<std::uint64_t, std::unique_ptr<ValueT>> map;
  map.reserve(ds.keys.size());
  for (std::uint64_t k : ds.keys) {
    map.emplace(k, std::make_unique<ValueT>(make_val(k)));
  }
  std::size_t sz = map.size();
  DoNotOptimize(sz);

  std::uint64_t rss1 = 0;
  std::uint64_t hwm1 = 0;
  ReadProcessMemoryBytes(rss1, hwm1);
  const std::uint64_t delta = rss1 > rss0 ? (rss1 - rss0) : 0ULL;
  const double mib = static_cast<double>(delta) / (1024.0 * 1024.0);
  const double b_per_entry =
      static_cast<double>(delta) / static_cast<double>(count);
  LOG(INFO) << absl::StrFormat("%-48s | RSS=%6.1f MiB | %6.1f B/entry", label,
                               mib, b_per_entry);
}

/**
 * @brief Times repeated batches of large-value lookups and logs throughput.
 * @tparam MapT Target hash map container type.
 * @tparam FindFn Lookup kernel taking `(const MapT&, absl::Span<const
 * uint64_t>)`.
 * @param label Formatted row label.
 * @param map Populated map.
 * @param queries Span of 64-bit lookup keys.
 * @param iters Number of timed batch passes.
 * @param find_fn Lookup kernel callable.
 */
template <typename MapT, typename FindFn>
void TimeLargeLookup(std::string_view label, const MapT& map,
                     absl::Span<const std::uint64_t> queries, std::size_t iters,
                     FindFn&& find_fn) {
  const std::uint64_t* q_ptr = queries.data();
  const std::size_t q_len = queries.size();
  std::uint64_t sink = find_fn(map, queries);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += find_fn(map, absl::MakeConstSpan(q_ptr, q_len));
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  LogRateRow(label, ms, iters * q_len, sink);
}

/**
 * @brief Times repeated full-table iteration sweeps and logs latency/rate.
 * @tparam MapT Target hash map container type.
 * @tparam IterFn Sweep kernel taking `const MapT&`.
 * @param label Formatted row label.
 * @param map Populated map.
 * @param iters Number of timed sweeps.
 * @param iter_fn Sweep kernel callable.
 */
template <typename MapT, typename IterFn>
void TimeLargeIter(std::string_view label, const MapT& map, std::size_t iters,
                   IterFn&& iter_fn) {
  std::uint64_t sink = iter_fn(map);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    const auto* ptr = &map;
    DoNotOptimize(ptr);
    sink += iter_fn(*ptr);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  LogSweepRow(label, ms, iters, map.size(), sink);
}

/**
 * @brief Runs Story 3 & Story 5 1M-entry lookup and 256B full-table flush
 *        benchmarks across flat, vector, node, and `unique_ptr` containers.
 * @param iters Number of timed passes per container.
 */
void RunStory3And5Perf(std::size_t iters) {
  const LargeValueDataset ds =
      BuildLargeValueDataset(kNumLargeEntriesPostDouble, kNumLargeQueries);
  LOG(INFO) << "=== Story 3 & 5: 1M ConnectionMetadata (128B) Lookups & "
               "Pointer Stability ===";

  auto run_conn = [&]<typename MapT>(std::string_view name) {
    const auto map = BuildAgedValueMap<MapT>(ds, MakeConnectionMetadata);
    TimeLargeLookup(absl::StrCat(name, " (80/20)"), map, ds.queries_80_20,
                    iters, FindConnectionBatch<MapT>);
    TimeLargeLookup(absl::StrCat(name, " (100% Miss)"), map,
                    ds.queries_100_miss, iters, FindConnectionBatch<MapT>);
  };

  run_conn.template
  operator()<std::unordered_map<std::uint64_t, ConnectionMetadata>>(
      "std::unordered_map");
  run_conn.template
  operator()<absl::flat_hash_map<std::uint64_t, ConnectionMetadata>>(
      "absl::flat_hash_map");
  run_conn.template
  operator()<folly::F14FastMap<std::uint64_t, ConnectionMetadata>>(
      "folly::F14FastMap (Vector)");
  run_conn.template
  operator()<folly::F14ValueMap<std::uint64_t, ConnectionMetadata>>(
      "folly::F14ValueMap (Inline)");
  run_conn.template
  operator()<boost::unordered_flat_map<std::uint64_t, ConnectionMetadata>>(
      "boost::unordered_flat_map");
  run_conn.template
  operator()<absl::node_hash_map<std::uint64_t, ConnectionMetadata>>(
      "absl::node_hash_map (Stable)");
  run_conn.template
  operator()<folly::F14NodeMap<std::uint64_t, ConnectionMetadata>>(
      "folly::F14NodeMap (Stable)");
  run_conn.template
  operator()<boost::unordered_node_map<std::uint64_t, ConnectionMetadata>>(
      "boost::unordered_node_map (Stable)");
  {
    const auto map = BuildAgedUniquePtrFlatMap<ConnectionMetadata>(
        ds, MakeConnectionMetadata);
    TimeLargeLookup("absl::flat<K, unique_ptr<V>> (80/20)", map,
                    ds.queries_80_20, iters, FindConnectionUniquePtrBatch);
    TimeLargeLookup("absl::flat<K, unique_ptr<V>> (100% Miss)", map,
                    ds.queries_100_miss, iters, FindConnectionUniquePtrBatch);
  }

  LOG(INFO) << "=== Story 3B: 1M UpstreamEndpointProfile (256B) Full-Table "
               "Flush & Lookups ===";
  auto run_ep = [&]<typename MapT>(std::string_view name) {
    const auto map = BuildAgedValueMap<MapT>(ds, MakeEndpointProfile);
    TimeLargeIter(absl::StrCat(name, " (Iter 1M x 256B)"), map, iters,
                  IterEndpointMap<MapT>);
    TimeLargeLookup(absl::StrCat(name, " (80/20 256B)"), map, ds.queries_80_20,
                    iters, FindEndpointBatch<MapT>);
  };
  run_ep.template
  operator()<std::unordered_map<std::uint64_t, UpstreamEndpointProfile>>(
      "std::unordered_map");
  run_ep.template
  operator()<absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
      "absl::flat_hash_map");
  run_ep.template
  operator()<folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>>(
      "folly::F14FastMap (Vector)");
  run_ep.template
  operator()<folly::F14ValueMap<std::uint64_t, UpstreamEndpointProfile>>(
      "folly::F14ValueMap (Inline)");
  run_ep.template
  operator()<boost::unordered_flat_map<std::uint64_t, UpstreamEndpointProfile>>(
      "boost::unordered_flat_map");
  {
    const auto map = BuildAgedUniquePtrFlatMap<UpstreamEndpointProfile>(
        ds, MakeEndpointProfile);
    TimeLargeIter("absl::flat<K, unique_ptr<V>> (Iter 1M x 256B)", map, iters,
                  IterEndpointUniquePtrMap);
    TimeLargeLookup("absl::flat<K, unique_ptr<V>> (80/20 256B)", map,
                    ds.queries_80_20, iters, FindEndpointUniquePtrBatch);
  }
}

// -----------------------------------------------------------------------------
// Story 4: Steady-State Sliding-Window Eviction Churn (780K & 640K Bands)
// -----------------------------------------------------------------------------

/**
 * @brief Executes steady-state sliding-window `erase(oldest) + emplace(newest)`
 *        churn on one hash-map engine, capturing pre/post-churn lookup drift
 *        and optional per-operation tail latency percentiles.
 * @tparam MapT Target hash map type mapping `uint64_t` to `CompactFlowStats`.
 * @param label Human-readable engine label.
 * @param ds Pre-built `ChurnWorkloadDataset`.
 * @param lookup_iters Number of lookup passes before and after churn.
 * @param print_full_summary True to record per-cycle latency percentiles.
 */
template <typename MapT>
void RunSingleChurnEngine(std::string_view label,
                          const ChurnWorkloadDataset& ds,
                          std::size_t lookup_iters, bool print_full_summary) {
  MapT map;
  map.reserve(ds.live_count);
  for (std::size_t i = 0; i < ds.live_count; ++i) {
    map.emplace(ds.key_stream[i], MakeCompactFlow(ds.key_stream[i]));
  }

  // Pre-churn baseline lookup & 100% negative miss throughput.
  auto measure_queries = [&](absl::Span<const std::uint64_t> q) {
    std::uint64_t sink = FindCompactFlowBatch(map, q);
    const auto t0 = Clock::now();
    for (std::size_t i = 0; i < lookup_iters; ++i) {
      sink += FindCompactFlowBatch(map, q);
      DoNotOptimize(sink);
    }
    const auto t1 = Clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double mops =
        static_cast<double>(lookup_iters * q.size()) / (ms * 1'000.0);
    return std::pair<double, std::uint64_t>{mops, sink};
  };

  const auto [pre_80_mops, pre_80_ck] = measure_queries(ds.pre_churn_80_20);
  const auto [pre_miss_mops, pre_miss_ck] =
      measure_queries(ds.queries_100_miss);
  (void)pre_80_ck;
  (void)pre_miss_ck;

  // Execute steady-state sliding-window erase(oldest) + emplace(newest).
  std::vector<std::uint64_t> samples;
  if (print_full_summary) {
    samples.resize(ds.churn_ops);
  }

  const auto c_t0 = Clock::now();
  if (print_full_summary) {
    for (std::size_t step = 0; step < ds.churn_ops; ++step) {
      const std::uint64_t victim = ds.key_stream[step];
      const std::uint64_t incoming = ds.key_stream[ds.live_count + step];
      const CompactFlowStats val = MakeCompactFlow(incoming);
      const auto s0 = Clock::now();
      map.erase(victim);
      map.emplace(incoming, val);
      const auto s1 = Clock::now();
      samples[step] = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(s1 - s0)
              .count());
    }
  } else {
    for (std::size_t step = 0; step < ds.churn_ops; ++step) {
      const std::uint64_t victim = ds.key_stream[step];
      const std::uint64_t incoming = ds.key_stream[ds.live_count + step];
      map.erase(victim);
      map.emplace(incoming, MakeCompactFlow(incoming));
    }
  }
  const auto c_t1 = Clock::now();
  const double churn_ms =
      std::chrono::duration<double, std::milli>(c_t1 - c_t0).count();

  const auto [post_80_mops, post_80_ck] = measure_queries(ds.post_churn_80_20);
  const auto [post_miss_mops, post_miss_ck] =
      measure_queries(ds.queries_100_miss);
  (void)post_miss_ck;

  const double churn_mops =
      static_cast<double>(ds.churn_ops) / (churn_ms * 1'000.0);

  if (print_full_summary) {
    const TailPercentiles pct =
        ComputePercentiles(samples, churn_ms, post_80_ck);
    LOG(INFO) << absl::StrFormat(
        "%-32s | churn=%5.2f M/s | p50=%4.0fns | p99=%4.0fns | "
        "p99.9=%5.0fns | p99.99=%6.0fns | max=%9.0fns",
        label, churn_mops, pct.p50_ns, pct.p99_ns, pct.p999_ns, pct.p9999_ns,
        pct.max_ns);
    LOG(INFO) << absl::StrFormat(
        "    -> bucket_count=%u load_factor=%.3f | 80/20: %.2f -> %.2f M/s | "
        "100%% Miss: %.2f -> %.2f M/s | ck=0x%x",
        map.bucket_count(), map.load_factor(), pre_80_mops, post_80_mops,
        pre_miss_mops, post_miss_mops, post_80_ck);
  } else {
    LOG(INFO) << absl::StrFormat(
        "%-32s | churn=%6.2f M cyc/s | total=%.2f ms | ck=0x%x", label,
        churn_mops, churn_ms, post_80_ck);
  }
}

/**
 * @brief Runs Story 4's dual-band steady-state eviction churn suite across
 *        both 780K (Band A) and 640K (Band B: F14Vector 81.4% fill).
 * @param churn_ops Number of continuous `erase + emplace` cycles per band.
 */
void RunStory4All(std::size_t churn_ops) {
  auto run_band = [&](std::size_t live_count, std::string_view band_label) {
    const ChurnWorkloadDataset ds =
        BuildChurnDataset(live_count, churn_ops, kNumChurnPostQueries);
    LOG(INFO) << absl::StrFormat(
        "=== Story 4 (%s): %u Live x 32B, %u Sliding-Window Erase+Insert "
        "Cycles ===",
        band_label, live_count, churn_ops);

    RunSingleChurnEngine<std::unordered_map<std::uint64_t, CompactFlowStats>>(
        "std::unordered_map", ds, 8, true);
    RunSingleChurnEngine<absl::flat_hash_map<std::uint64_t, CompactFlowStats>>(
        "absl::flat_hash_map (Tombstones)", ds, 8, true);
    RunSingleChurnEngine<folly::F14ValueMap<std::uint64_t, CompactFlowStats>>(
        "folly::F14ValueMap (RefCnt Ofw)", ds, 8, true);
    RunSingleChurnEngine<folly::F14FastMap<std::uint64_t, CompactFlowStats>>(
        "folly::F14FastMap (Vector+Ofw)", ds, 8, true);
    RunSingleChurnEngine<
        boost::unordered_flat_map<std::uint64_t, CompactFlowStats>>(
        "boost::unordered_flat_map (Ofw)", ds, 8, true);
  };

  // Band A: 780K live -> absl 74.4%, boost 79.3%, F14Value 85.0% (F14Vector
  // 50%).
  run_band(780'000, "Band A: 780K High-Fill for absl/boost/F14Value");
  // Band B: 640K live -> F14Vector packed tight at 81.4% (9.8/12 slots/chunk).
  run_band(640'000, "Band B: 640K High-Fill for F14FastMap/Vector (81.4%)");
}

/**
 * @brief Keeps `extern "C"` disassembly reference kernels linked inside the
 *        compiled binary for `llvm-objdump` inspection.
 */
void TouchDisasmKernels() {
  std::unordered_map<std::uint64_t, CompactFlowStats> m_std;
  absl::flat_hash_map<std::uint64_t, CompactFlowStats> m_absl;
  folly::F14ValueMap<std::uint64_t, CompactFlowStats> m_fval;
  folly::F14FastMap<std::uint64_t, CompactFlowStats> m_ffast;
  boost::unordered_flat_map<std::uint64_t, CompactFlowStats> m_boost;
  m_std.emplace(1ULL, MakeCompactFlow(1ULL));
  m_absl.emplace(1ULL, MakeCompactFlow(1ULL));
  m_fval.emplace(1ULL, MakeCompactFlow(1ULL));
  m_ffast.emplace(1ULL, MakeCompactFlow(1ULL));
  m_boost.emplace(1ULL, MakeCompactFlow(1ULL));
  std::uint64_t sink = Ep16DisasmStdFindU64(m_std, 1ULL) ^
                       Ep16DisasmAbslFlatFindU64(m_absl, 1ULL) ^
                       Ep16DisasmFollyF14ValueFindU64(m_fval, 1ULL) ^
                       Ep16DisasmFollyF14FastFindU64(m_ffast, 1ULL) ^
                       Ep16DisasmBoostFlatFindU64(m_boost, 1ULL);
  DoNotOptimize(sink);
}

}  // namespace

/**
 * @brief Dispatches Episode 16 forensic challenge & `perf stat` modes via
 *        Abseil flags (`--mode`, `--iters`, `--count`, `--quiet`).
 * @param mode Selected forensic execution mode.
 * @param iters Optional iteration override (0 selects mode default).
 * @param count Entry count for isolated VmRSS or Story 4 churn workloads.
 * @return Process exit code (0 on success, 1 on unknown mode).
 */
int RunChallengeCli(std::string_view mode, std::size_t iters,
                    std::size_t count) {
  TouchDisasmKernels();

  if (mode == "all") {
    RunStory1All(iters > 0 ? iters : 8);
    RunStory2All(iters > 0 ? iters : 200'000);
    RunStory3And5Perf(iters > 0 ? iters : 8);
    RunStory4All(iters > 0 ? iters : kNumChurnOps);
    return 0;
  }

  if (mode == "s1_noexcept_demo") {
    RunNoexceptHashTrapDemo(iters > 0 ? iters : 4);
    return 0;
  }
  if (mode == "s1_shared_hash_control") {
    RunSharedHasherControl(iters > 0 ? iters : 6);
    return 0;
  }

  // Story 1 isolated modes for unmultiplexed Linux `perf stat`.
  if (absl::StartsWith(mode, "s1_")) {
    const RouteTableDataset ds =
        BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
    const std::size_t n_it = iters > 0 ? iters : 20;
    const bool miss_only = absl::StrContains(mode, "_miss");
    const auto& q = miss_only ? ds.queries_100_miss : ds.queries_zipf_70_30;
    if (absl::StrContains(mode, "std_def")) {
      const auto m = BuildAgedRouteMap<StdRouteMapDefault>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16StdDefaultFindRoute);
    } else if (absl::StrContains(mode, "std_tr")) {
      const auto m = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16StdTransparentFindRoute);
    } else if (absl::StrContains(mode, "pmr")) {
      const auto h = BuildPmrRouteMap(ds);
      TimeRouteLookups(mode, *h.map, q, n_it, Ep16PmrTransparentFindRoute);
    } else if (absl::StrContains(mode, "absl")) {
      const auto m = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16AbslFlatFindRoute);
    } else if (absl::StrContains(mode, "f14_fast")) {
      const auto m = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16FollyF14FastFindRoute);
    } else if (absl::StrContains(mode, "f14_val")) {
      const auto m = BuildAgedRouteMap<FollyF14ValueRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16FollyF14ValueFindRoute);
    } else if (absl::StrContains(mode, "boost")) {
      const auto m = BuildAgedRouteMap<BoostFlatRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16BoostFlatFindRoute);
    } else {
      LOG(ERROR) << "Unknown Story 1 mode: " << mode;
      return 1;
    }
    return 0;
  }

  // Story 3 isolated fresh-process VmRSS measurement modes.
  if (absl::StartsWith(mode, "rss_")) {
    const std::string tag = absl::StrCat("(", count / 1000, "K) ");
    if (mode == "rss_32_std") {
      MeasureIsolatedRss<std::unordered_map<std::uint64_t, CompactFlowStats>>(
          absl::StrCat(tag, "32B std::unordered_map"), count, MakeCompactFlow);
    } else if (mode == "rss_32_absl") {
      MeasureIsolatedRss<absl::flat_hash_map<std::uint64_t, CompactFlowStats>>(
          absl::StrCat(tag, "32B absl::flat_hash_map"), count, MakeCompactFlow);
    } else if (mode == "rss_32_f14_val") {
      MeasureIsolatedRss<folly::F14ValueMap<std::uint64_t, CompactFlowStats>>(
          absl::StrCat(tag, "32B folly::F14ValueMap"), count, MakeCompactFlow);
    } else if (mode == "rss_32_f14_fast") {
      MeasureIsolatedRss<folly::F14FastMap<std::uint64_t, CompactFlowStats>>(
          absl::StrCat(tag, "32B folly::F14FastMap (Vector)"), count,
          MakeCompactFlow);
    } else if (mode == "rss_32_boost") {
      MeasureIsolatedRss<
          boost::unordered_flat_map<std::uint64_t, CompactFlowStats>>(
          absl::StrCat(tag, "32B boost::unordered_flat_map"), count,
          MakeCompactFlow);
    } else if (mode == "rss_128_std") {
      MeasureIsolatedRss<std::unordered_map<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B std::unordered_map"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_absl") {
      MeasureIsolatedRss<
          absl::flat_hash_map<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B absl::flat_hash_map"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_f14_val") {
      MeasureIsolatedRss<folly::F14ValueMap<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B folly::F14ValueMap"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_f14_fast") {
      MeasureIsolatedRss<folly::F14FastMap<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B folly::F14FastMap (Vector)"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_boost") {
      MeasureIsolatedRss<
          boost::unordered_flat_map<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B boost::unordered_flat_map"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_absl_node") {
      MeasureIsolatedRss<
          absl::node_hash_map<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B absl::node_hash_map"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_f14_node") {
      MeasureIsolatedRss<folly::F14NodeMap<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B folly::F14NodeMap"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_boost_node") {
      MeasureIsolatedRss<
          boost::unordered_node_map<std::uint64_t, ConnectionMetadata>>(
          absl::StrCat(tag, "128B boost::unordered_node_map"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_absl_uptr") {
      MeasureIsolatedUniquePtrRss<ConnectionMetadata>(
          absl::StrCat(tag, "128B absl::flat<K, unique_ptr<V>>"), count,
          MakeConnectionMetadata);
    } else if (mode == "rss_256_std") {
      MeasureIsolatedRss<
          std::unordered_map<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B std::unordered_map"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_absl") {
      MeasureIsolatedRss<
          absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B absl::flat_hash_map"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_f14_val") {
      MeasureIsolatedRss<
          folly::F14ValueMap<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B folly::F14ValueMap"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_f14_fast") {
      MeasureIsolatedRss<
          folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B folly::F14FastMap (Vector)"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_boost") {
      MeasureIsolatedRss<
          boost::unordered_flat_map<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B boost::unordered_flat_map"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_absl_node") {
      MeasureIsolatedRss<
          absl::node_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B absl::node_hash_map"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_f14_node") {
      MeasureIsolatedRss<
          folly::F14NodeMap<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B folly::F14NodeMap"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_boost_node") {
      MeasureIsolatedRss<
          boost::unordered_node_map<std::uint64_t, UpstreamEndpointProfile>>(
          absl::StrCat(tag, "256B boost::unordered_node_map"), count,
          MakeEndpointProfile);
    } else if (mode == "rss_256_absl_uptr") {
      MeasureIsolatedUniquePtrRss<UpstreamEndpointProfile>(
          absl::StrCat(tag, "256B absl::flat<K, unique_ptr<V>>"), count,
          MakeEndpointProfile);
    } else {
      LOG(ERROR) << "Unknown RSS mode: " << mode;
      return 1;
    }
    return 0;
  }

  // Story 3 isolated 256B iteration modes for unmultiplexed `perf stat`.
  if (absl::StartsWith(mode, "s3_iter256_")) {
    const LargeValueDataset ds =
        BuildLargeValueDataset(kNumLargeEntriesPostDouble, 1024);
    const std::size_t n_it = iters > 0 ? iters : 30;
    if (mode == "s3_iter256_std") {
      const auto m = BuildAgedValueMap<
          std::unordered_map<std::uint64_t, UpstreamEndpointProfile>>(
          ds, MakeEndpointProfile);
      TimeLargeIter(
          mode, m, n_it,
          IterEndpointMap<
              std::unordered_map<std::uint64_t, UpstreamEndpointProfile>>);
    } else if (mode == "s3_iter256_absl") {
      const auto m = BuildAgedValueMap<
          absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
          ds, MakeEndpointProfile);
      TimeLargeIter(
          mode, m, n_it,
          IterEndpointMap<
              absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>>);
    } else if (mode == "s3_iter256_f14_fast") {
      const auto m = BuildAgedValueMap<
          folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>>(
          ds, MakeEndpointProfile);
      TimeLargeIter(
          mode, m, n_it,
          IterEndpointMap<
              folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>>);
    } else if (mode == "s3_iter256_boost") {
      const auto m = BuildAgedValueMap<
          boost::unordered_flat_map<std::uint64_t, UpstreamEndpointProfile>>(
          ds, MakeEndpointProfile);
      TimeLargeIter(
          mode, m, n_it,
          IterEndpointMap<boost::unordered_flat_map<std::uint64_t,
                                                    UpstreamEndpointProfile>>);
    } else {
      LOG(ERROR) << "Unknown s3_iter256 mode: " << mode;
      return 1;
    }
    return 0;
  }

  // Story 4 isolated churn modes for unmultiplexed `perf stat` (without clock
  // calls inside the tight loop).
  if (absl::StartsWith(mode, "s4_churn_")) {
    const std::size_t ops = iters > 0 ? iters : kNumChurnOps;
    const std::size_t live =
        (count != kNumLargeEntriesPostDouble) ? count : kNumChurnLiveEntries;
    const ChurnWorkloadDataset ds = BuildChurnDataset(live, ops, 100'000);
    if (mode == "s4_churn_std") {
      RunSingleChurnEngine<std::unordered_map<std::uint64_t, CompactFlowStats>>(
          mode, ds, 1, false);
    } else if (mode == "s4_churn_absl") {
      RunSingleChurnEngine<
          absl::flat_hash_map<std::uint64_t, CompactFlowStats>>(mode, ds, 1,
                                                                false);
    } else if (mode == "s4_churn_f14_val") {
      RunSingleChurnEngine<folly::F14ValueMap<std::uint64_t, CompactFlowStats>>(
          mode, ds, 1, false);
    } else if (mode == "s4_churn_f14_fast") {
      RunSingleChurnEngine<folly::F14FastMap<std::uint64_t, CompactFlowStats>>(
          mode, ds, 1, false);
    } else if (mode == "s4_churn_boost") {
      RunSingleChurnEngine<
          boost::unordered_flat_map<std::uint64_t, CompactFlowStats>>(mode, ds,
                                                                      1, false);
    } else {
      LOG(ERROR) << "Unknown s4_churn mode: " << mode;
      return 1;
    }
    return 0;
  }

  if (mode == "story1") {
    RunStory1All(iters > 0 ? iters : 8);
  } else if (mode == "story2") {
    RunStory2All(iters > 0 ? iters : 200'000);
  } else if (mode == "story3_5") {
    RunStory3And5Perf(iters > 0 ? iters : 8);
  } else if (mode == "story4") {
    RunStory4All(iters > 0 ? iters : kNumChurnOps);
  } else {
    LOG(ERROR) << "Unknown --mode=" << mode;
    return 1;
  }
  return 0;
}

}  // namespace hotpath::episode16

/**
 * @brief Entry point initializing Abseil flags and logging before dispatching
 *        Episode 16 forensic workloads.
 * @param argc Command-line argument count.
 * @param argv Command-line argument vector.
 * @return Process exit code (0 on success).
 */
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::GetFlag(FLAGS_quiet)
                               ? absl::LogSeverityAtLeast::kError
                               : absl::LogSeverityAtLeast::kInfo);
  return hotpath::episode16::RunChallengeCli(absl::GetFlag(FLAGS_mode),
                                             absl::GetFlag(FLAGS_iters),
                                             absl::GetFlag(FLAGS_count));
}
