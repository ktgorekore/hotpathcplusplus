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
 * @brief Episode 16 official Google Benchmark suite covering all 5 production
 *        stories across `std::unordered_map`, `std::pmr`, Abseil SwissTable,
 *        Meta Folly F14, Boost.Unordered, normalized shared-hasher controls,
 *        and stack-inlined flat small maps.
 */

#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/types/span.h"
#include "common/cpp_hash_maps/cpp_hash_maps.h"

namespace hotpath::episode16 {
namespace {

// -----------------------------------------------------------------------------
// Shared Lazy-Initialized Datasets & Aged Maps
// -----------------------------------------------------------------------------

/**
 * @brief Returns lazily constructed deterministic Story 1 route dataset.
 * @return Const reference to process-lifetime `RouteTableDataset`.
 */
const RouteTableDataset& GetRouteDataset() {
  static const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, 200'000);
  return ds;
}

/**
 * @brief Returns aged default `std::unordered_map` route table.
 * @return Const reference to populated `StdRouteMapDefault`.
 */
const StdRouteMapDefault& GetStdRouteDefault() {
  static const auto map =
      BuildAgedRouteMap<StdRouteMapDefault>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns aged C++20 transparent `std::unordered_map` route table.
 * @return Const reference to populated `StdRouteMapTransparent`.
 */
const StdRouteMapTransparent& GetStdRouteTransparent() {
  static const auto map =
      BuildAgedRouteMap<StdRouteMapTransparent>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns aged GCC `noexcept`-trap `std::unordered_map` route table.
 * @return Const reference to populated `StdRouteMapTransparentNoexcept`.
 */
const StdRouteMapTransparentNoexcept& GetStdRouteTransparentNoexcept() {
  static const auto map =
      BuildAgedRouteMap<StdRouteMapTransparentNoexcept>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns monotonic-arena `std::pmr::unordered_map` route table holder.
 * @return Const reference to populated `PmrRouteTableHolder`.
 */
const PmrRouteTableHolder& GetPmrRouteHolder() {
  static const auto holder = BuildPmrRouteMap(GetRouteDataset());
  return holder;
}

/**
 * @brief Returns aged `absl::flat_hash_map` route table.
 * @return Const reference to populated `AbslFlatRouteMap`.
 */
const AbslFlatRouteMap& GetAbslFlatRoute() {
  static const auto map =
      BuildAgedRouteMap<AbslFlatRouteMap>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns aged `folly::F14FastMap` route table.
 * @return Const reference to populated `FollyF14FastRouteMap`.
 */
const FollyF14FastRouteMap& GetFollyF14FastRoute() {
  static const auto map =
      BuildAgedRouteMap<FollyF14FastRouteMap>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns aged `folly::F14ValueMap` route table.
 * @return Const reference to populated `FollyF14ValueRouteMap`.
 */
const FollyF14ValueRouteMap& GetFollyF14ValueRoute() {
  static const auto map =
      BuildAgedRouteMap<FollyF14ValueRouteMap>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns aged transparent `boost::unordered_flat_map` route table.
 * @return Const reference to populated `BoostFlatRouteMap`.
 */
const BoostFlatRouteMap& GetBoostFlatRoute() {
  static const auto map =
      BuildAgedRouteMap<BoostFlatRouteMap>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns shared-hasher control `absl::flat_hash_map` (`std::hash`).
 * @return Const reference to populated `AbslFlatRouteMapSharedHash`.
 */
const AbslFlatRouteMapSharedHash& GetAbslSharedHashRoute() {
  static const auto map =
      BuildAgedRouteMap<AbslFlatRouteMapSharedHash>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns shared-hasher control `folly::F14FastMap` (`std::hash`).
 * @return Const reference to populated `FollyF14FastRouteMapSharedHash`.
 */
const FollyF14FastRouteMapSharedHash& GetFollySharedHashRoute() {
  static const auto map =
      BuildAgedRouteMap<FollyF14FastRouteMapSharedHash>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns shared-hasher control `boost::unordered_flat_map`
 *        (`std::hash`).
 * @return Const reference to populated `BoostFlatRouteMapSharedHash`.
 */
const BoostFlatRouteMapSharedHash& GetBoostSharedHashRoute() {
  static const auto map =
      BuildAgedRouteMap<BoostFlatRouteMapSharedHash>(GetRouteDataset());
  return map;
}

/**
 * @brief Returns lazily constructed deterministic Story 3/5 large dataset.
 * @return Const reference to process-lifetime `LargeValueDataset`.
 */
const LargeValueDataset& GetLargeDataset() {
  static const LargeValueDataset ds =
      BuildLargeValueDataset(kNumLargeEntriesPostDouble, 250'000);
  return ds;
}

// -----------------------------------------------------------------------------
// Story 1: Envoy HTTP/RPC Route Lookup (Zipfian 70/30 & 100% Negative Misses)
// -----------------------------------------------------------------------------

/**
 * @brief Story 1 (Zipfian 70/30): default `std::unordered_map` constructing a
 *        temporary `std::string` on every `std::string_view` route query.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_StdDefault_Alloc(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetStdRouteDefault();
  for (auto _ : state) {
    std::uint64_t r = Ep16StdDefaultFindRoute(map, ds.queries_zipf_70_30.data(),
                                              ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_StdDefault_Alloc);

/**
 * @brief Story 1 (Zipfian 70/30): zero-dependency C++20 `is_transparent`
 *        `std::unordered_map` with preserved libstdc++ hash-code caching.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_StdTransparent_Cpp20(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetStdRouteTransparent();
  for (auto _ : state) {
    std::uint64_t r = Ep16StdTransparentFindRoute(
        map, ds.queries_zipf_70_30.data(), ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_StdTransparent_Cpp20);

/**
 * @brief Story 1 (Zipfian 70/30): zero-dependency `std::pmr::unordered_map`
 *        backed by a contiguous `std::pmr::monotonic_buffer_resource` arena.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_PmrMonotonic(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& holder = GetPmrRouteHolder();
  for (auto _ : state) {
    std::uint64_t r =
        Ep16PmrTransparentFindRoute(*holder.map, ds.queries_zipf_70_30.data(),
                                    ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_PmrMonotonic);

/**
 * @brief Story 1 (Zipfian 70/30): `absl::flat_hash_map` 16-way SwissTable SIMD
 *        route lookup with built-in transparent `std::string_view` support.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_AbslFlat(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetAbslFlatRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16AbslFlatFindRoute(map, ds.queries_zipf_70_30.data(),
                                            ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_AbslFlat);

/**
 * @brief Story 1 (Zipfian 70/30): `folly::F14FastMap` (auto-selected
 *        `F14VectorMap` 12-slot cache-line chunks) route lookup.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_FollyF14Fast(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetFollyF14FastRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16FollyF14FastFindRoute(
        map, ds.queries_zipf_70_30.data(), ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_FollyF14Fast);

/**
 * @brief Story 1 (Zipfian 70/30): `folly::F14ValueMap` 14-way inline chunk
 *        route lookup.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_FollyF14Value(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetFollyF14ValueRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16FollyF14ValueFindRoute(
        map, ds.queries_zipf_70_30.data(), ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_FollyF14Value);

/**
 * @brief Story 1 (Zipfian 70/30): `boost::unordered_flat_map` 15-way `group15`
 *        route lookup with explicit transparent avalanching string hash.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_RouteZipf_BoostFlat(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetBoostFlatRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16BoostFlatFindRoute(map, ds.queries_zipf_70_30.data(),
                                             ds.queries_zipf_70_30.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(
      state.iterations() *
      static_cast<std::int64_t>(ds.queries_zipf_70_30.size()));
}
BENCHMARK(BM_Story1_RouteZipf_BoostFlat);

/**
 * @brief Story 1 (100% 404 Miss): default `std::unordered_map` allocating a
 *        temporary `std::string` on every negative miss.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_StdDefault_Alloc(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetStdRouteDefault();
  for (auto _ : state) {
    std::uint64_t r = Ep16StdDefaultFindRoute(map, ds.queries_100_miss.data(),
                                              ds.queries_100_miss.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_StdDefault_Alloc);

/**
 * @brief Story 1 (100% 404 Miss): zero-dependency C++20 `is_transparent`
 *        `std::unordered_map` negative miss scan (8B hash cached).
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_StdTransparent(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetStdRouteTransparent();
  for (auto _ : state) {
    std::uint64_t r = Ep16StdTransparentFindRoute(
        map, ds.queries_100_miss.data(), ds.queries_100_miss.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_StdTransparent);

/**
 * @brief Story 1 (100% 404 Miss): GCC `libstdc++` `noexcept` trap dropping
 *        the 8B node hash cache and re-hashing stored heap strings on misses.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_StdNoexceptTrap(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetStdRouteTransparentNoexcept();
  for (auto _ : state) {
    std::uint64_t r = Ep16StdTransparentNoexceptFindRoute(
        map, ds.queries_100_miss.data(), ds.queries_100_miss.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_StdNoexceptTrap);

/**
 * @brief Story 1 (100% 404 Miss): `absl::flat_hash_map` SIMD `kEmpty`
 *        early-exit negative miss scan.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_AbslFlat(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetAbslFlatRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16AbslFlatFindRoute(map, ds.queries_100_miss.data(),
                                            ds.queries_100_miss.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_AbslFlat);

/**
 * @brief Story 1 (100% 404 Miss): `folly::F14FastMap` SIMD overflow-zero
 *        early-exit negative miss scan.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_FollyF14Fast(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetFollyF14FastRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16FollyF14FastFindRoute(map, ds.queries_100_miss.data(),
                                                ds.queries_100_miss.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_FollyF14Fast);

/**
 * @brief Story 1 (100% 404 Miss): `boost::unordered_flat_map` SIMD overflow
 *        bitmask early-exit negative miss scan.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_BoostFlat(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetBoostFlatRoute();
  for (auto _ : state) {
    std::uint64_t r = Ep16BoostFlatFindRoute(map, ds.queries_100_miss.data(),
                                             ds.queries_100_miss.size());
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_BoostFlat);

/**
 * @brief Story 1 Shared-Hasher Control (100% 404 Miss): `absl::flat_hash_map`
 *        normalized onto identical `std::hash<std::string_view>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_SharedStdHash_AbslFlat(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetAbslSharedHashRoute();
  for (auto _ : state) {
    std::uint64_t r = FindTransparentRouteBatch(map, ds.queries_100_miss);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_SharedStdHash_AbslFlat);

/**
 * @brief Story 1 Shared-Hasher Control (100% 404 Miss): `folly::F14FastMap`
 *        normalized onto identical `std::hash<std::string_view>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_SharedStdHash_FollyF14Fast(
    benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetFollySharedHashRoute();
  for (auto _ : state) {
    std::uint64_t r = FindTransparentRouteBatch(map, ds.queries_100_miss);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_SharedStdHash_FollyF14Fast);

/**
 * @brief Story 1 Shared-Hasher Control (100% 404 Miss):
 *        `boost::unordered_flat_map` normalized onto identical
 *        `std::hash<std::string_view>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story1_Route100Miss_SharedStdHash_BoostFlat(benchmark::State& state) {
  const auto& ds = GetRouteDataset();
  const auto& map = GetBoostSharedHashRoute();
  for (auto _ : state) {
    std::uint64_t r = FindTransparentRouteBatch(map, ds.queries_100_miss);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Story1_Route100Miss_SharedStdHash_BoostFlat);

// -----------------------------------------------------------------------------
// Story 2: Per-Request Small HTTP/2 Header Maps (N = 4, 8, 16, 32, 64, 128)
// -----------------------------------------------------------------------------

/**
 * @brief Story 2: full per-request lifecycle (build + 12 lookups + destroy)
 *        on a flat linear-scan small map (`FlatInlinedSmallMap` /
 *        `StdVectorSmallMap`).
 * @tparam FlatMapT Contiguous flat small-map container type.
 * @param state Google Benchmark runner state (`state.range(0)` = header count).
 */
template <typename FlatMapT>
void BM_Story2_SmallFlatLifecycle(benchmark::State& state) {
  const HttpHeaderDataset ds =
      BuildHttpHeaderDataset(static_cast<std::size_t>(state.range(0)));
  for (auto _ : state) {
    std::uint64_t r = RunSmallFlatMapRequestLifecycle<FlatMapT>(ds);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations());
}

/**
 * @brief Story 2: full per-request lifecycle (build + 12 lookups + destroy)
 *        on a hash-table container (`std`, `absl`, `folly`, `boost`).
 * @tparam HashMapT Hash-map container type keyed by `std::string_view`.
 * @param state Google Benchmark runner state (`state.range(0)` = header count).
 */
template <typename HashMapT>
void BM_Story2_SmallHashLifecycle(benchmark::State& state) {
  const HttpHeaderDataset ds =
      BuildHttpHeaderDataset(static_cast<std::size_t>(state.range(0)));
  for (auto _ : state) {
    std::uint64_t r = RunSmallHashMapRequestLifecycle<HashMapT>(ds);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Story2_SmallFlatLifecycle,
                   FlatInlinedSmallMap<std::string_view, std::string_view, 16>)
    ->Name("BM_Story2_Headers_InlinedFlat16")
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128);

BENCHMARK_TEMPLATE(BM_Story2_SmallFlatLifecycle,
                   StdVectorSmallMap<std::string_view, std::string_view>)
    ->Name("BM_Story2_Headers_StdVectorFlat")
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128);

BENCHMARK_TEMPLATE(BM_Story2_SmallHashLifecycle,
                   std::unordered_map<std::string_view, std::string_view>)
    ->Name("BM_Story2_Headers_StdUnordered")
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128);

BENCHMARK_TEMPLATE(BM_Story2_SmallHashLifecycle,
                   absl::flat_hash_map<std::string_view, std::string_view>)
    ->Name("BM_Story2_Headers_AbslFlat")
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128);

BENCHMARK_TEMPLATE(BM_Story2_SmallHashLifecycle,
                   folly::F14FastMap<std::string_view, std::string_view>)
    ->Name("BM_Story2_Headers_FollyF14Fast")
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128);

BENCHMARK_TEMPLATE(
    BM_Story2_SmallHashLifecycle,
    boost::unordered_flat_map<std::string_view, std::string_view>)
    ->Name("BM_Story2_Headers_BoostFlat")
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128);

// -----------------------------------------------------------------------------
// Story 3 & Story 5: 1M Large Values (128B Connection & 256B Endpoint)
// -----------------------------------------------------------------------------

/**
 * @brief Story 3 & 5: 80/20 hit/miss lookup on 1M aged `128B`
 *        `ConnectionMetadata` entries across flat, vector, and stable node
 *        maps.
 * @tparam MapT Hash map storing `ConnectionMetadata` by value.
 * @param state Google Benchmark runner state.
 */
template <typename MapT>
void BM_Story3_Conn128B_Lookup80_20(benchmark::State& state) {
  const auto& ds = GetLargeDataset();
  static const auto map = BuildAgedValueMap<MapT>(ds, MakeConnectionMetadata);
  for (auto _ : state) {
    std::uint64_t r = FindConnectionBatch(map, ds.queries_80_20);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}

BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   std::unordered_map<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story3_Conn128B_80_20_StdUnordered");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   absl::flat_hash_map<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story3_Conn128B_80_20_AbslFlat");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   folly::F14FastMap<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story3_Conn128B_80_20_FollyF14Fast");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   folly::F14ValueMap<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story3_Conn128B_80_20_FollyF14Value");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   boost::unordered_flat_map<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story3_Conn128B_80_20_BoostFlat");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   absl::node_hash_map<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story5_Conn128B_80_20_AbslNode");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   folly::F14NodeMap<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story5_Conn128B_80_20_FollyF14Node");
BENCHMARK_TEMPLATE(BM_Story3_Conn128B_Lookup80_20,
                   boost::unordered_node_map<std::uint64_t, ConnectionMetadata>)
    ->Name("BM_Story5_Conn128B_80_20_BoostNode");

/**
 * @brief Story 5: 80/20 hit/miss lookup on 1M aged `128B` `ConnectionMetadata`
 *        entries stored via `absl::flat_hash_map<uint64_t, unique_ptr<V>>`.
 * @param state Google Benchmark runner state.
 */
void BM_Story5_Conn128B_80_20_AbslFlatUniquePtr(benchmark::State& state) {
  const auto& ds = GetLargeDataset();
  static const auto map =
      BuildAgedUniquePtrFlatMap<ConnectionMetadata>(ds, MakeConnectionMetadata);
  for (auto _ : state) {
    std::uint64_t r = FindConnectionUniquePtrBatch(map, ds.queries_80_20);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Story5_Conn128B_80_20_AbslFlatUniquePtr);

/**
 * @brief Story 3: full-table 1M-entry iteration sweep over `256B`
 *        `UpstreamEndpointProfile` entries (dense vector vs. sparse flat holes
 *        vs. fragmented linked nodes).
 * @tparam MapT Hash map storing `UpstreamEndpointProfile` by value.
 * @param state Google Benchmark runner state.
 */
template <typename MapT>
void BM_Story3_Endpoint256B_IterFull1M(benchmark::State& state) {
  const auto& ds = GetLargeDataset();
  static const auto map = BuildAgedValueMap<MapT>(ds, MakeEndpointProfile);
  for (auto _ : state) {
    std::uint64_t r = IterEndpointMap(map);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(map.size()));
}

BENCHMARK_TEMPLATE(BM_Story3_Endpoint256B_IterFull1M,
                   std::unordered_map<std::uint64_t, UpstreamEndpointProfile>)
    ->Name("BM_Story3_Endpoint256B_Iter_StdUnordered")
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Story3_Endpoint256B_IterFull1M,
                   absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>)
    ->Name("BM_Story3_Endpoint256B_Iter_AbslFlat")
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Story3_Endpoint256B_IterFull1M,
                   folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>)
    ->Name("BM_Story3_Endpoint256B_Iter_FollyF14Fast")
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(
    BM_Story3_Endpoint256B_IterFull1M,
    boost::unordered_flat_map<std::uint64_t, UpstreamEndpointProfile>)
    ->Name("BM_Story3_Endpoint256B_Iter_BoostFlat")
    ->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace hotpath::episode16
