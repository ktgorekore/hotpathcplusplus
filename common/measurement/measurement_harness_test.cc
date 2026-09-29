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

#include "common/measurement/measurement_harness.h"

#include <vector>

#include "gtest/gtest.h"

namespace cognitas::trading {
namespace {

TEST(MeasurementHarnessTest, ReadTscSerializedMonotonic) {
  uint32_t cpu_id1 = 0;
  uint32_t cpu_id2 = 0;
  uint64_t tsc1 = ReadTscSerialized(&cpu_id1);
  uint64_t tsc2 = ReadTscSerialized(&cpu_id2);

  EXPECT_GT(tsc1, 0ULL);
  EXPECT_GT(tsc2, 0ULL);
  EXPECT_LE(tsc1, tsc2);
}

TEST(MeasurementHarnessTest, FlushCacheRangePreservesContent) {
  std::vector<uint8_t> buffer(4096, 0xAA);
  FlushCacheRange(buffer.data(), buffer.size());

  for (size_t i = 0; i < buffer.size(); ++i) {
    ASSERT_EQ(buffer[i], 0xAA);
  }
}

TEST(MeasurementHarnessTest, ForceOptimizationBarrierPreservesValue) {
  uint64_t val = 42;
  ForceOptimizationBarrier(val);
  EXPECT_EQ(val, 42ULL);
}

}  // namespace
}  // namespace cognitas::trading
