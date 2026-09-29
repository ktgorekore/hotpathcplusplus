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

#include "common/measurement/entropy_generator.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"

namespace cognitas::trading {
namespace {

TEST(EntropyGeneratorTest, UniformRandomReproducibilityAndSize) {
  constexpr size_t kCount = 1000;
  auto data1 = DatasetGenerator::GenerateUniformRandom(kCount, 42);
  auto data2 = DatasetGenerator::GenerateUniformRandom(kCount, 42);
  auto data3 = DatasetGenerator::GenerateUniformRandom(kCount, 99);

  ASSERT_EQ(data1.size(), kCount);
  ASSERT_EQ(data2.size(), kCount);
  ASSERT_EQ(data3.size(), kCount);

  EXPECT_EQ(data1, data2);
  EXPECT_NE(data1, data3);
}

TEST(EntropyGeneratorTest, SortedArrayIsMonotonicallyIncreasing) {
  constexpr size_t kCount = 500;
  auto sorted_data = DatasetGenerator::GenerateSorted(kCount, 123);

  ASSERT_EQ(sorted_data.size(), kCount);
  EXPECT_TRUE(std::is_sorted(sorted_data.begin(), sorted_data.end()));
}

TEST(EntropyGeneratorTest, PointerChaseRingFormsValidHamiltonianCycle) {
  constexpr size_t kSizeBytes = 1024 * sizeof(uint32_t);  // 1024 elements
  const size_t num_elements = kSizeBytes / sizeof(uint32_t);

  for (bool randomized : {false, true}) {
    auto ring =
        DatasetGenerator::GeneratePointerChaseRing(kSizeBytes, randomized);
    ASSERT_EQ(ring.size(), num_elements);

    std::unordered_set<uint32_t> visited;
    uint32_t curr = 0;
    for (size_t i = 0; i < num_elements; ++i) {
      visited.insert(curr);
      curr = ring[curr];
    }

    // Must have visited every unique element and returned to the start
    EXPECT_EQ(visited.size(), num_elements);
    EXPECT_EQ(curr, 0u);
  }
}

}  // namespace
}  // namespace cognitas::trading
