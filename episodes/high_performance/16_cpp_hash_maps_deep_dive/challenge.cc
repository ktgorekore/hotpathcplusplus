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
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "common/cpp_hash_maps/cpp_hash_maps.h"

namespace hotpath::episode16 {
namespace {

using Clock = std::chrono::steady_clock;

template <typename T>
inline void DoNotOptimize(T& val) {
  asm volatile("" : "+r,m"(val) : : "memory");
}

void PrintRateRow(const std::string& label, double total_ms,
                  std::size_t total_ops, std::uint64_t checksum) {
  const double ns_per_op =
      (total_ms * 1'000'000.0) / static_cast<double>(total_ops);
  const double mops = static_cast<double>(total_ops) / (total_ms * 1'000.0);
  std::cout << std::left << std::setw(46) << label << " | " << std::right
            << std::setw(7) << std::fixed << std::setprecision(2) << ns_per_op
            << " ns/op | " << std::setw(7) << std::setprecision(2) << mops
            << " M/s | checksum=0x" << std::hex << checksum << std::dec << "\n";
}

void PrintSweepRow(const std::string& label, double total_ms,
                   std::size_t sweeps, std::size_t live_entries,
                   std::uint64_t checksum) {
  const double ms_per_sweep = total_ms / static_cast<double>(sweeps);
  const double mops =
      static_cast<double>(sweeps * live_entries) / (total_ms * 1'000.0);
  std::cout << std::left << std::setw(46) << label << " | " << std::right
            << std::setw(7) << std::fixed << std::setprecision(2)
            << ms_per_sweep << " ms/swp | " << std::setw(7) << mops
            << " M/s | checksum=0x" << std::hex << checksum << std::dec << "\n";
}

// -----------------------------------------------------------------------------
// Story 1: Envoy HTTP/RPC Route Lookup (Non-SSO Strings + Transparent Lookup)
// -----------------------------------------------------------------------------

template <typename MapT, typename FindFn>
void TimeRouteLookups(const std::string& label, const MapT& map,
                      const std::vector<std::string_view>& queries,
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
  PrintRateRow(label, ms, iters * q_len, sink);
}

void RunStory1All(std::size_t iters) {
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
  std::cout << "\n=== Story 1: Envoy HTTP/RPC Route Table (" << kNumRoutes
            << " Non-SSO 36-42B Routes, Aged Heap) ===\n";

  const auto m_std_def = BuildAgedRouteMap<StdRouteMapDefault>(ds);
  const auto m_std_tr = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto m_std_tr_noex =
      BuildAgedRouteMap<StdRouteMapTransparentNoexcept>(ds);
  const auto pmr = BuildPmrRouteMap(ds);
  const auto m_absl = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
  const auto m_f14_fast = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
  const auto m_f14_val = BuildAgedRouteMap<FollyF14ValueRouteMap>(ds);
  const auto m_boost = BuildAgedRouteMap<BoostFlatRouteMap>(ds);

  std::cout << "--- 1A. Zipfian s=0.99 Hot Skew (70% Hit / 30% 404 Miss) ---\n";
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

  std::cout << "--- 1B. Uniform Aged-Heap (70% Hit / 30% 404 Miss) ---\n";
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

  std::cout << "--- 1C. 100% Negative Misses (Bot Scans / 404 Flood) ---\n";
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

void RunNoexceptHashTrapDemo(std::size_t iters) {
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
  std::cout << "=== GCC libstdc++ noexcept Hash-Caching Audit (100K Non-SSO "
               "36-42B Routes, 404 Misses) ===\n";
  const auto m_std_def = BuildAgedRouteMap<StdRouteMapDefault>(ds);
  const auto m_std_tr = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto m_std_tr_noex =
      BuildAgedRouteMap<StdRouteMapTransparentNoexcept>(ds);
  // Warm CPU turbo P-state & steady-state cache identically across all 3 maps
  // (matching RunStory1All's Zipf/Uniform pass before 1C 404 Misses).
  std::uint64_t warm_sink = 0;
  for (int w = 0; w < 2; ++w) {
    warm_sink ^= Ep16StdDefaultFindRoute(
        m_std_def, ds.queries_zipf_70_30.data(), ds.queries_zipf_70_30.size());
    warm_sink ^= Ep16StdTransparentFindRoute(
        m_std_tr, ds.queries_zipf_70_30.data(), ds.queries_zipf_70_30.size());
    warm_sink ^= Ep16StdTransparentNoexceptFindRoute(
        m_std_tr_noex, ds.queries_zipf_70_30.data(),
        ds.queries_zipf_70_30.size());
  }
  DoNotOptimize(warm_sink);
  TimeRouteLookups("std::unordered_map (default, heap allocs)", m_std_def,
                   ds.queries_100_miss, iters, Ep16StdDefaultFindRoute);
  TimeRouteLookups("std::unordered_map (transparent, cached)", m_std_tr,
                   ds.queries_100_miss, iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("std::unordered_map (transparent + noexcept!)",
                   m_std_tr_noex, ds.queries_100_miss, iters,
                   Ep16StdTransparentNoexceptFindRoute);
}

void RunSharedHasherControl(std::size_t iters) {
  using AbslSharedHashMap =
      absl::flat_hash_map<std::string, RoutePolicy, TransparentStdStringHash,
                          std::equal_to<>>;
  using FollySharedHashMap =
      folly::F14FastMap<std::string, RoutePolicy, TransparentStdStringHash,
                        std::equal_to<>>;
  using BoostSharedHashMap =
      boost::unordered_flat_map<std::string, RoutePolicy,
                                TransparentStdStringHash, std::equal_to<>>;

  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
  const auto m_std = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto m_absl = BuildAgedRouteMap<AbslSharedHashMap>(ds);
  const auto m_f14 = BuildAgedRouteMap<FollySharedHashMap>(ds);
  const auto m_boost = BuildAgedRouteMap<BoostSharedHashMap>(ds);

  auto find_generic = [](const auto& map, const std::string_view* q,
                         std::size_t n) -> std::uint64_t {
    std::uint64_t sum = 0;
    std::uint64_t hits = 0;
    for (std::size_t i = 0; i < n; ++i) {
      const auto it = map.find(q[i]);
      if (it != map.end()) {
        sum += DigestRoutePolicy(it->second);
        ++hits;
      }
    }
    return FinalizeChecksum(sum, hits);
  };

  std::cout << "\n=== Story 1 Shared-Hasher Control (Identical "
               "std::hash<std::string_view>) ===\n";
  std::cout << "--- Zipfian 70/30 (Normalized std::hash) ---\n";
  TimeRouteLookups("std::unordered_map (std::hash)", m_std,
                   ds.queries_zipf_70_30, iters, Ep16StdTransparentFindRoute);
  TimeRouteLookups("absl::flat_hash_map (std::hash)", m_absl,
                   ds.queries_zipf_70_30, iters, find_generic);
  TimeRouteLookups("folly::F14FastMap (std::hash)", m_f14,
                   ds.queries_zipf_70_30, iters, find_generic);
  TimeRouteLookups("boost::unordered_flat_map (std::hash)", m_boost,
                   ds.queries_zipf_70_30, iters, find_generic);
  std::cout << "--- 100% 404 Misses (Normalized std::hash) ---\n";
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

template <typename Fn>
void TimeSmallLifecycle(const std::string& label, std::size_t requests,
                        Fn&& fn) {
  std::uint64_t sink = fn();
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < requests; ++i) {
    sink += fn();
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintRateRow(label, ms, requests, sink);
}

void RunStory2All(std::size_t requests) {
  std::cout << "\n=== Story 2: Per-Request HTTP/2 Header Maps (Build + 12 "
               "Lookups + Destroy) ===\n";
  for (std::size_t n : {4U, 8U, 16U, 32U, 64U, 128U}) {
    const HttpHeaderDataset ds = BuildHttpHeaderDataset(n);
    std::cout << "--- N = " << n << " Headers (" << requests
              << " full request lifecycles) ---\n";
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

template <typename MapT, typename FactoryFn>
void MeasureIsolatedRss(const std::string& label, std::size_t count,
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
  std::cout << std::left << std::setw(48) << label << " | RSS=" << std::right
            << std::setw(6) << std::fixed << std::setprecision(1) << mib
            << " MiB | " << std::setw(6) << std::setprecision(1) << b_per_entry
            << " B/entry\n";
}

template <typename ValueT, typename FactoryFn>
void MeasureIsolatedUniquePtrRss(const std::string& label, std::size_t count,
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
  std::cout << std::left << std::setw(48) << label << " | RSS=" << std::right
            << std::setw(6) << std::fixed << std::setprecision(1) << mib
            << " MiB | " << std::setw(6) << std::setprecision(1) << b_per_entry
            << " B/entry\n";
}

template <typename MapT, typename FindFn>
void TimeLargeLookup(const std::string& label, const MapT& map,
                     const std::vector<std::uint64_t>& queries,
                     std::size_t iters, FindFn&& find_fn) {
  const std::uint64_t* q_ptr = queries.data();
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
  PrintRateRow(label, ms, iters * q_len, sink);
}

template <typename MapT, typename IterFn>
void TimeLargeIter(const std::string& label, const MapT& map, std::size_t iters,
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
  PrintSweepRow(label, ms, iters, map.size(), sink);
}

void RunStory3And5Perf(std::size_t iters) {
  const LargeValueDataset ds =
      BuildLargeValueDataset(kNumLargeEntriesPostDouble, kNumLargeQueries);
  std::cout << "\n=== Story 3 & 5: 1M ConnectionMetadata (128B) Lookups & "
               "Pointer Stability ===\n";

  auto run_conn = [&]<typename MapT>(const std::string& name) {
    const auto map = BuildAgedValueMap<MapT>(ds, MakeConnectionMetadata);
    TimeLargeLookup(name + " (80/20)", map, ds.queries_80_20, iters,
                    FindConnectionBatch<MapT>);
    TimeLargeLookup(name + " (100% Miss)", map, ds.queries_100_miss, iters,
                    FindConnectionBatch<MapT>);
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

  std::cout << "\n=== Story 3B: 1M UpstreamEndpointProfile (256B) Full-Table "
               "Flush & Lookups ===\n";
  auto run_ep = [&]<typename MapT>(const std::string& name) {
    const auto map = BuildAgedValueMap<MapT>(ds, MakeEndpointProfile);
    TimeLargeIter(name + " (Iter 1M x 256B)", map, iters,
                  IterEndpointMap<MapT>);
    TimeLargeLookup(name + " (80/20 256B)", map, ds.queries_80_20, iters,
                    FindEndpointBatch<MapT>);
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
// Story 4: Steady-State Sliding-Window Eviction Churn (780K Live, 4M Cycles)
// -----------------------------------------------------------------------------

template <typename MapT>
void RunSingleChurnEngine(const std::string& label,
                          const ChurnWorkloadDataset& ds,
                          std::size_t lookup_iters, bool print_full_summary) {
  MapT map;
  map.reserve(ds.live_count);
  for (std::size_t i = 0; i < ds.live_count; ++i) {
    map.emplace(ds.key_stream[i], MakeCompactFlow(ds.key_stream[i]));
  }

  // Pre-churn baseline lookup & 100% negative miss throughput.
  auto measure_queries = [&](const std::vector<std::uint64_t>& q) {
    std::uint64_t sink = FindCompactFlowBatch(map, q.data(), q.size());
    const auto t0 = Clock::now();
    for (std::size_t i = 0; i < lookup_iters; ++i) {
      sink += FindCompactFlowBatch(map, q.data(), q.size());
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
    std::cout << std::left << std::setw(32) << label
              << " | churn=" << std::right << std::setw(6) << std::fixed
              << std::setprecision(2) << churn_mops << " M cyc/s"
              << " | p50=" << std::setw(4) << std::setprecision(0) << pct.p50_ns
              << " ns | p99=" << std::setw(5) << pct.p99_ns
              << " ns | p99.9=" << std::setw(5) << pct.p999_ns
              << " ns | p99.99=" << std::setw(6) << pct.p9999_ns
              << " ns | max=" << std::setw(10) << pct.max_ns << " ns\n";
    std::cout << "    -> bucket_count=" << map.bucket_count()
              << " load_factor=" << std::fixed << std::setprecision(3)
              << map.load_factor() << " | 80/20: " << std::setprecision(2)
              << pre_80_mops << " -> " << post_80_mops
              << " M/s | 100% Miss: " << pre_miss_mops << " -> "
              << post_miss_mops << " M/s | ck=0x" << std::hex << post_80_ck
              << std::dec << "\n";
  } else {
    std::cout << std::left << std::setw(32) << label
              << " | churn=" << std::right << std::setw(6) << std::fixed
              << std::setprecision(2) << churn_mops
              << " M cyc/s | total=" << churn_ms << " ms | ck=0x" << std::hex
              << post_80_ck << std::dec << "\n";
  }
}

void RunStory4All(std::size_t churn_ops) {
  auto run_band = [&](std::size_t live_count, const std::string& band_label) {
    const ChurnWorkloadDataset ds =
        BuildChurnDataset(live_count, churn_ops, kNumChurnPostQueries);
    std::cout << "\n=== Story 4 (" << band_label << "): " << live_count
              << " Live x 32B, " << churn_ops
              << " Sliding-Window Erase+Insert Cycles ===\n";

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
  // Band B: 640K live -> F14Vector packed tight at 81.4% (9.8/12 slots/chunk!).
  run_band(640'000, "Band B: 640K High-Fill for F14FastMap/Vector (81.4%)");
}

// Keep disassembly reference kernels linked and alive inside the binary.
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
 * @brief Dispatches Episode 16 forensic challenge & `perf stat` modes via CLI.
 * @param argc Argument count.
 * @param argv Argument vector (`--mode=...`, `--iters=...`, `--count=...`).
 * @return Process exit code (0 on success).
 */
int RunChallengeCli(int argc, char** argv) {
  std::string mode = "all";
  std::size_t iters = 0;
  std::size_t count = kNumLargeEntriesPostDouble;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.rfind("--mode=", 0) == 0) {
      mode = arg.substr(7);
    } else if (arg.rfind("--iters=", 0) == 0) {
      iters = static_cast<std::size_t>(std::stoull(arg.substr(8)));
    } else if (arg.rfind("--count=", 0) == 0) {
      count = static_cast<std::size_t>(std::stoull(arg.substr(8)));
    }
  }

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
  if (mode.rfind("s1_", 0) == 0) {
    const RouteTableDataset ds =
        BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);
    const std::size_t n_it = iters > 0 ? iters : 20;
    const bool miss_only = (mode.find("_miss") != std::string::npos);
    const auto& q = miss_only ? ds.queries_100_miss : ds.queries_zipf_70_30;
    if (mode.find("std_def") != std::string::npos) {
      const auto m = BuildAgedRouteMap<StdRouteMapDefault>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16StdDefaultFindRoute);
    } else if (mode.find("std_tr") != std::string::npos) {
      const auto m = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16StdTransparentFindRoute);
    } else if (mode.find("pmr") != std::string::npos) {
      const auto h = BuildPmrRouteMap(ds);
      TimeRouteLookups(mode, *h.map, q, n_it, Ep16PmrTransparentFindRoute);
    } else if (mode.find("absl") != std::string::npos) {
      const auto m = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16AbslFlatFindRoute);
    } else if (mode.find("f14_fast") != std::string::npos) {
      const auto m = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16FollyF14FastFindRoute);
    } else if (mode.find("f14_val") != std::string::npos) {
      const auto m = BuildAgedRouteMap<FollyF14ValueRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16FollyF14ValueFindRoute);
    } else if (mode.find("boost") != std::string::npos) {
      const auto m = BuildAgedRouteMap<BoostFlatRouteMap>(ds);
      TimeRouteLookups(mode, m, q, n_it, Ep16BoostFlatFindRoute);
    } else {
      std::cerr << "Unknown Story 1 mode: " << mode << "\n";
      return 1;
    }
    return 0;
  }

  // Story 3 isolated fresh-process VmRSS measurement modes.
  if (mode.rfind("rss_", 0) == 0) {
    const std::string tag = "(" + std::to_string(count / 1000) + "K) ";
    if (mode == "rss_32_std") {
      MeasureIsolatedRss<std::unordered_map<std::uint64_t, CompactFlowStats>>(
          tag + "32B std::unordered_map", count, MakeCompactFlow);
    } else if (mode == "rss_32_absl") {
      MeasureIsolatedRss<absl::flat_hash_map<std::uint64_t, CompactFlowStats>>(
          tag + "32B absl::flat_hash_map", count, MakeCompactFlow);
    } else if (mode == "rss_32_f14_val") {
      MeasureIsolatedRss<folly::F14ValueMap<std::uint64_t, CompactFlowStats>>(
          tag + "32B folly::F14ValueMap", count, MakeCompactFlow);
    } else if (mode == "rss_32_f14_fast") {
      MeasureIsolatedRss<folly::F14FastMap<std::uint64_t, CompactFlowStats>>(
          tag + "32B folly::F14FastMap (Vector)", count, MakeCompactFlow);
    } else if (mode == "rss_32_boost") {
      MeasureIsolatedRss<
          boost::unordered_flat_map<std::uint64_t, CompactFlowStats>>(
          tag + "32B boost::unordered_flat_map", count, MakeCompactFlow);
    } else if (mode == "rss_128_std") {
      MeasureIsolatedRss<std::unordered_map<std::uint64_t, ConnectionMetadata>>(
          tag + "128B std::unordered_map", count, MakeConnectionMetadata);
    } else if (mode == "rss_128_absl") {
      MeasureIsolatedRss<
          absl::flat_hash_map<std::uint64_t, ConnectionMetadata>>(
          tag + "128B absl::flat_hash_map", count, MakeConnectionMetadata);
    } else if (mode == "rss_128_f14_val") {
      MeasureIsolatedRss<folly::F14ValueMap<std::uint64_t, ConnectionMetadata>>(
          tag + "128B folly::F14ValueMap", count, MakeConnectionMetadata);
    } else if (mode == "rss_128_f14_fast") {
      MeasureIsolatedRss<folly::F14FastMap<std::uint64_t, ConnectionMetadata>>(
          tag + "128B folly::F14FastMap (Vector)", count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_boost") {
      MeasureIsolatedRss<
          boost::unordered_flat_map<std::uint64_t, ConnectionMetadata>>(
          tag + "128B boost::unordered_flat_map", count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_absl_node") {
      MeasureIsolatedRss<
          absl::node_hash_map<std::uint64_t, ConnectionMetadata>>(
          tag + "128B absl::node_hash_map", count, MakeConnectionMetadata);
    } else if (mode == "rss_128_f14_node") {
      MeasureIsolatedRss<folly::F14NodeMap<std::uint64_t, ConnectionMetadata>>(
          tag + "128B folly::F14NodeMap", count, MakeConnectionMetadata);
    } else if (mode == "rss_128_boost_node") {
      MeasureIsolatedRss<
          boost::unordered_node_map<std::uint64_t, ConnectionMetadata>>(
          tag + "128B boost::unordered_node_map", count,
          MakeConnectionMetadata);
    } else if (mode == "rss_128_absl_uptr") {
      MeasureIsolatedUniquePtrRss<ConnectionMetadata>(
          tag + "128B absl::flat<K, unique_ptr<V>>", count,
          MakeConnectionMetadata);
    } else if (mode == "rss_256_std") {
      MeasureIsolatedRss<
          std::unordered_map<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B std::unordered_map", count, MakeEndpointProfile);
    } else if (mode == "rss_256_absl") {
      MeasureIsolatedRss<
          absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B absl::flat_hash_map", count, MakeEndpointProfile);
    } else if (mode == "rss_256_f14_val") {
      MeasureIsolatedRss<
          folly::F14ValueMap<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B folly::F14ValueMap", count, MakeEndpointProfile);
    } else if (mode == "rss_256_f14_fast") {
      MeasureIsolatedRss<
          folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B folly::F14FastMap (Vector)", count, MakeEndpointProfile);
    } else if (mode == "rss_256_boost") {
      MeasureIsolatedRss<
          boost::unordered_flat_map<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B boost::unordered_flat_map", count, MakeEndpointProfile);
    } else if (mode == "rss_256_absl_node") {
      MeasureIsolatedRss<
          absl::node_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B absl::node_hash_map", count, MakeEndpointProfile);
    } else if (mode == "rss_256_f14_node") {
      MeasureIsolatedRss<
          folly::F14NodeMap<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B folly::F14NodeMap", count, MakeEndpointProfile);
    } else if (mode == "rss_256_boost_node") {
      MeasureIsolatedRss<
          boost::unordered_node_map<std::uint64_t, UpstreamEndpointProfile>>(
          tag + "256B boost::unordered_node_map", count, MakeEndpointProfile);
    } else if (mode == "rss_256_absl_uptr") {
      MeasureIsolatedUniquePtrRss<UpstreamEndpointProfile>(
          tag + "256B absl::flat<K, unique_ptr<V>>", count,
          MakeEndpointProfile);
    } else {
      std::cerr << "Unknown RSS mode: " << mode << "\n";
      return 1;
    }
    return 0;
  }

  // Story 3 isolated 256B iteration modes for unmultiplexed `perf stat`.
  if (mode.rfind("s3_iter256_", 0) == 0) {
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
      std::cerr << "Unknown s3_iter256 mode: " << mode << "\n";
      return 1;
    }
    return 0;
  }

  // Story 4 isolated churn modes for unmultiplexed `perf stat` (without clock
  // calls inside the tight loop!).
  if (mode.rfind("s4_churn_", 0) == 0) {
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
      std::cerr << "Unknown s4_churn mode: " << mode << "\n";
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
    std::cerr << "Unknown --mode=" << mode << "\n";
    return 1;
  }
  return 0;
}

}  // namespace hotpath::episode16

int main(int argc, char** argv) {
  return hotpath::episode16::RunChallengeCli(argc, argv);
}
