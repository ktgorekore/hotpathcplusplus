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
 * @file cpp_hash_maps_test.cc
 * @brief GoogleTest cross-container parity and pointer-stability verification
 *        suite for Episode 16 ("Production C++ Hash Maps").
 */

#include "common/cpp_hash_maps/cpp_hash_maps.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hotpath::episode16 {
namespace {

TEST(CppHashMapsTest, Story1RouteTableParityAcrossAllEngines) {
  const RouteTableDataset ds = BuildRouteTableDataset(4'096, 16'384);
  for (const std::string& k : ds.keys) {
    ASSERT_GT(k.size(), 15U) << "Keys must exceed GCC libstdc++ 15B SSO";
  }

  const auto std_def = BuildAgedRouteMap<StdRouteMapDefault>(ds);
  const auto std_tr = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
  const auto std_tr_noex =
      BuildAgedRouteMap<StdRouteMapTransparentNoexcept>(ds);
  const auto pmr_holder = BuildPmrRouteMap(ds);
  const auto absl_flat = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
  const auto folly_fast = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
  const auto folly_val = BuildAgedRouteMap<FollyF14ValueRouteMap>(ds);
  const auto boost_flat = BuildAgedRouteMap<BoostFlatRouteMap>(ds);

  const auto check_stream = [&](const std::vector<std::string_view>& q,
                                bool expect_zero) {
    const std::uint64_t c_std_def =
        Ep16StdDefaultFindRoute(std_def, q.data(), q.size());
    const std::uint64_t c_std_tr =
        Ep16StdTransparentFindRoute(std_tr, q.data(), q.size());
    const std::uint64_t c_std_tr_noex =
        Ep16StdTransparentNoexceptFindRoute(std_tr_noex, q.data(), q.size());
    const std::uint64_t c_pmr =
        Ep16PmrTransparentFindRoute(*pmr_holder.map, q.data(), q.size());
    const std::uint64_t c_absl =
        Ep16AbslFlatFindRoute(absl_flat, q.data(), q.size());
    const std::uint64_t c_folly_fast =
        Ep16FollyF14FastFindRoute(folly_fast, q.data(), q.size());
    const std::uint64_t c_folly_val =
        Ep16FollyF14ValueFindRoute(folly_val, q.data(), q.size());
    const std::uint64_t c_boost =
        Ep16BoostFlatFindRoute(boost_flat, q.data(), q.size());

    if (expect_zero) {
      EXPECT_EQ(c_std_def, 0ULL);
    } else {
      EXPECT_NE(c_std_def, 0ULL);
    }
    EXPECT_EQ(c_std_def, c_std_tr);
    EXPECT_EQ(c_std_def, c_std_tr_noex);
    EXPECT_EQ(c_std_def, c_pmr);
    EXPECT_EQ(c_std_def, c_absl);
    EXPECT_EQ(c_std_def, c_folly_fast);
    EXPECT_EQ(c_std_def, c_folly_val);
    EXPECT_EQ(c_std_def, c_boost);
  };

  check_stream(ds.queries_zipf_70_30, false);
  check_stream(ds.queries_uniform_70_30, false);
  check_stream(ds.queries_100_miss, true);
}

TEST(CppHashMapsTest, Story2SmallHeaderMapsLifecycleParity) {
  for (std::size_t n : {4U, 8U, 16U, 32U, 64U}) {
    const HttpHeaderDataset ds = BuildHttpHeaderDataset(n);
    const std::uint64_t c_inlined = RunSmallFlatMapRequestLifecycle<
        FlatInlinedSmallMap<std::string_view, std::string_view, 16>>(ds);
    const std::uint64_t c_vec = RunSmallFlatMapRequestLifecycle<
        StdVectorSmallMap<std::string_view, std::string_view>>(ds);
    const std::uint64_t c_std = RunSmallHashMapRequestLifecycle<
        std::unordered_map<std::string_view, std::string_view>>(ds);
    const std::uint64_t c_absl = RunSmallHashMapRequestLifecycle<
        absl::flat_hash_map<std::string_view, std::string_view>>(ds);
    const std::uint64_t c_folly = RunSmallHashMapRequestLifecycle<
        folly::F14FastMap<std::string_view, std::string_view>>(ds);
    const std::uint64_t c_boost = RunSmallHashMapRequestLifecycle<
        boost::unordered_flat_map<std::string_view, std::string_view>>(ds);

    EXPECT_NE(c_inlined, 0ULL);
    EXPECT_EQ(c_inlined, c_vec);
    EXPECT_EQ(c_inlined, c_std);
    EXPECT_EQ(c_inlined, c_absl);
    EXPECT_EQ(c_inlined, c_folly);
    EXPECT_EQ(c_inlined, c_boost);
  }
}

TEST(CppHashMapsTest, Story3LargeValuesLookupAndIterationParity) {
  const LargeValueDataset ds = BuildLargeValueDataset(4'096, 8'192);

  // 128B ConnectionMetadata parity across flat, F14Vector, node, and unique_ptr
  const auto m_std =
      BuildAgedValueMap<std::unordered_map<std::uint64_t, ConnectionMetadata>>(
          ds, MakeConnectionMetadata);
  const auto m_absl_flat =
      BuildAgedValueMap<absl::flat_hash_map<std::uint64_t, ConnectionMetadata>>(
          ds, MakeConnectionMetadata);
  const auto m_absl_node =
      BuildAgedValueMap<absl::node_hash_map<std::uint64_t, ConnectionMetadata>>(
          ds, MakeConnectionMetadata);
  const auto m_folly_fast =
      BuildAgedValueMap<folly::F14FastMap<std::uint64_t, ConnectionMetadata>>(
          ds, MakeConnectionMetadata);
  const auto m_folly_node =
      BuildAgedValueMap<folly::F14NodeMap<std::uint64_t, ConnectionMetadata>>(
          ds, MakeConnectionMetadata);
  const auto m_boost_flat = BuildAgedValueMap<
      boost::unordered_flat_map<std::uint64_t, ConnectionMetadata>>(
      ds, MakeConnectionMetadata);
  const auto m_boost_node = BuildAgedValueMap<
      boost::unordered_node_map<std::uint64_t, ConnectionMetadata>>(
      ds, MakeConnectionMetadata);
  const auto m_unique_ptr =
      BuildAgedUniquePtrFlatMap<ConnectionMetadata>(ds, MakeConnectionMetadata);

  const std::uint64_t ref = FindConnectionBatch(m_std, ds.queries_80_20.data(),
                                                ds.queries_80_20.size());
  EXPECT_NE(ref, 0ULL);
  EXPECT_EQ(ref, FindConnectionBatch(m_absl_flat, ds.queries_80_20.data(),
                                     ds.queries_80_20.size()));
  EXPECT_EQ(ref, FindConnectionBatch(m_absl_node, ds.queries_80_20.data(),
                                     ds.queries_80_20.size()));
  EXPECT_EQ(ref, FindConnectionBatch(m_folly_fast, ds.queries_80_20.data(),
                                     ds.queries_80_20.size()));
  EXPECT_EQ(ref, FindConnectionBatch(m_folly_node, ds.queries_80_20.data(),
                                     ds.queries_80_20.size()));
  EXPECT_EQ(ref, FindConnectionBatch(m_boost_flat, ds.queries_80_20.data(),
                                     ds.queries_80_20.size()));
  EXPECT_EQ(ref, FindConnectionBatch(m_boost_node, ds.queries_80_20.data(),
                                     ds.queries_80_20.size()));
  EXPECT_EQ(ref,
            FindConnectionUniquePtrBatch(m_unique_ptr, ds.queries_80_20.data(),
                                         ds.queries_80_20.size()));

  // 256B UpstreamEndpointProfile iteration parity
  const auto e_absl = BuildAgedValueMap<
      absl::flat_hash_map<std::uint64_t, UpstreamEndpointProfile>>(
      ds, MakeEndpointProfile);
  const auto e_folly = BuildAgedValueMap<
      folly::F14FastMap<std::uint64_t, UpstreamEndpointProfile>>(
      ds, MakeEndpointProfile);
  const auto e_boost = BuildAgedValueMap<
      boost::unordered_flat_map<std::uint64_t, UpstreamEndpointProfile>>(
      ds, MakeEndpointProfile);
  const auto e_uptr = BuildAgedUniquePtrFlatMap<UpstreamEndpointProfile>(
      ds, MakeEndpointProfile);

  const std::uint64_t iter_ref = IterEndpointMap(e_absl);
  EXPECT_NE(iter_ref, 0ULL);
  EXPECT_EQ(iter_ref, IterEndpointMap(e_folly));
  EXPECT_EQ(iter_ref, IterEndpointMap(e_boost));
  EXPECT_EQ(iter_ref, IterEndpointUniquePtrMap(e_uptr));
}

TEST(CppHashMapsTest, Story4SlidingWindowChurnParity) {
  const ChurnWorkloadDataset ds = BuildChurnDataset(2'048, 8'192, 4'096);

  auto run_churn = [&](auto& map) -> std::uint64_t {
    map.reserve(ds.live_count);
    for (std::size_t i = 0; i < ds.live_count; ++i) {
      map.emplace(ds.key_stream[i], MakeCompactFlow(ds.key_stream[i]));
    }
    for (std::size_t step = 0; step < ds.churn_ops; ++step) {
      map.erase(ds.key_stream[step]);
      const std::uint64_t incoming = ds.key_stream[ds.live_count + step];
      map.emplace(incoming, MakeCompactFlow(incoming));
    }
    EXPECT_EQ(map.size(), ds.live_count);
    return FindCompactFlowBatch(map, ds.post_churn_80_20.data(),
                                ds.post_churn_80_20.size());
  };

  std::unordered_map<std::uint64_t, CompactFlowStats> m_std;
  absl::flat_hash_map<std::uint64_t, CompactFlowStats> m_absl;
  folly::F14ValueMap<std::uint64_t, CompactFlowStats> m_folly_val;
  folly::F14FastMap<std::uint64_t, CompactFlowStats> m_folly_fast;
  boost::unordered_flat_map<std::uint64_t, CompactFlowStats> m_boost;

  const std::uint64_t ref = run_churn(m_std);
  EXPECT_NE(ref, 0ULL);
  EXPECT_EQ(ref, run_churn(m_absl));
  EXPECT_EQ(ref, run_churn(m_folly_val));
  EXPECT_EQ(ref, run_churn(m_folly_fast));
  EXPECT_EQ(ref, run_churn(m_boost));
}

TEST(CppHashMapsTest, Story5NodeMapsPreservePointerStabilityAcrossRehash) {
  auto verify_stable = [](auto& map) {
    map.emplace(1ULL, MakeConnectionMetadata(1ULL));
    const ConnectionMetadata* before = &map.at(1ULL);
    for (std::uint64_t k = 2; k <= 4096; ++k) {
      map.emplace(k, MakeConnectionMetadata(k));
    }
    const ConnectionMetadata* after = &map.at(1ULL);
    EXPECT_EQ(before, after);
    EXPECT_EQ(after->connection_id, 1ULL);
  };

  std::unordered_map<std::uint64_t, ConnectionMetadata> m_std;
  absl::node_hash_map<std::uint64_t, ConnectionMetadata> m_absl_node;
  folly::F14NodeMap<std::uint64_t, ConnectionMetadata> m_folly_node;
  boost::unordered_node_map<std::uint64_t, ConnectionMetadata> m_boost_node;

  verify_stable(m_std);
  verify_stable(m_absl_node);
  verify_stable(m_folly_node);
  verify_stable(m_boost_node);

  // Value-only stability via flat_hash_map<K, std::unique_ptr<V>>
  absl::flat_hash_map<std::uint64_t, std::unique_ptr<ConnectionMetadata>>
      m_uptr;
  m_uptr.emplace(
      1ULL, std::make_unique<ConnectionMetadata>(MakeConnectionMetadata(1ULL)));
  const ConnectionMetadata* val_before = m_uptr.at(1ULL).get();
  for (std::uint64_t k = 2; k <= 4096; ++k) {
    m_uptr.emplace(
        k, std::make_unique<ConnectionMetadata>(MakeConnectionMetadata(k)));
  }
  const ConnectionMetadata* val_after = m_uptr.at(1ULL).get();
  EXPECT_EQ(val_before, val_after);
}

}  // namespace
}  // namespace hotpath::episode16
