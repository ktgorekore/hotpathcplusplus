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

#include "common/hash_maps/hash_maps_across_languages.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace hotpath::episode15 {
namespace {

TEST(HashMapsAcrossLanguagesTest, PresentAndAbsentKeysAreDisjoint) {
  const GatewayDataset ds = BuildDataset(16'384, 32'768);
  const auto std_map = BuildAgedStdUnorderedPacked(ds, true);
  const auto absl_map = BuildAgedAbslFlatPacked(ds, true);

  const std::uint64_t std_80_20 = ep15_std_unordered_find_packed(
      std_map, ds.queries_80_20.data(), ds.queries_80_20.size());
  const std::uint64_t absl_80_20 = ep15_absl_flat_find_packed(
      absl_map, ds.queries_80_20.data(), ds.queries_80_20.size());
  EXPECT_EQ(std_80_20, absl_80_20);

  const std::uint64_t std_miss = ep15_std_unordered_find_packed(
      std_map, ds.queries_100_miss.data(), ds.queries_100_miss.size());
  const std::uint64_t absl_miss = ep15_absl_flat_find_packed(
      absl_map, ds.queries_100_miss.data(), ds.queries_100_miss.size());
  EXPECT_EQ(std_miss, 0ULL);
  EXPECT_EQ(absl_miss, 0ULL);
}

TEST(HashMapsAcrossLanguagesTest, SessionStateLookupParity) {
  const GatewayDataset ds = BuildDataset(8'192, 16'384);
  const auto std_map = BuildAgedStdUnorderedSession(ds);
  const auto absl_map = BuildAgedAbslFlatSession(ds);

  const std::uint64_t std_sum = ep15_std_unordered_find_session(
      std_map, ds.queries_80_20.data(), ds.queries_80_20.size());
  const std::uint64_t absl_sum = ep15_absl_flat_find_session(
      absl_map, ds.queries_80_20.data(), ds.queries_80_20.size());
  EXPECT_NE(std_sum, 0ULL);
  EXPECT_EQ(std_sum, absl_sum);
}

TEST(HashMapsAcrossLanguagesTest, PrimeMultiplesTriggerBucketZeroHashDos) {
  constexpr std::size_t kDosCount = 4'096;
  std::uint64_t prime = 0;
  const std::vector<std::uint64_t> keys =
      BuildLibstdcxxHashDosKeys(kDosCount, prime);
  ASSERT_GT(prime, kDosCount);

  std::unordered_map<std::uint64_t, std::uint64_t> target;
  target.reserve(kDosCount);
  ASSERT_EQ(static_cast<std::uint64_t>(target.bucket_count()), prime);
  for (std::size_t i = 0; i < kDosCount; ++i) {
    target.emplace(keys[i], static_cast<std::uint64_t>(i));
  }
  EXPECT_EQ(target.bucket_size(0), kDosCount);
}

TEST(HashMapsAcrossLanguagesTest, PercentilesAreMonotonic) {
  std::vector<std::uint64_t> samples = {10, 50, 20, 40, 1000, 30, 80, 90};
  const TailPercentiles p = ComputePercentiles(samples, 1.0, 42ULL);
  EXPECT_LE(p.p50_ns, p.p99_ns);
  EXPECT_LE(p.p99_ns, p.p999_ns);
  EXPECT_LE(p.p999_ns, p.p9999_ns);
  EXPECT_LE(p.p9999_ns, p.max_ns);
  EXPECT_DOUBLE_EQ(p.max_ns, 1000.0);
}

}  // namespace
}  // namespace hotpath::episode15
