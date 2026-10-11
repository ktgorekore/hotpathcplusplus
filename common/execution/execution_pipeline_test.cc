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

#include "common/execution/execution_pipeline.h"

#include <cmath>
#include <numeric>
#include <vector>

#include "gtest/gtest.h"

namespace hotpath::execution {
namespace {

TEST(ExecutionPipelineTest, IntegerSumEquivalence) {
  std::vector<uint64_t> data(1027);
  std::iota(data.begin(), data.end(), 1ULL);

  const uint64_t expected = DependentPipeline::Sum(data);
  const uint64_t result_4way = IndependentPipeline::Sum4Way(data);
  const uint64_t result_8way = IndependentPipeline::Sum8Way(data);

  EXPECT_EQ(result_4way, expected);
  EXPECT_EQ(result_8way, expected);
}

TEST(ExecutionPipelineTest, FloatSumEquivalence) {
  std::vector<double> data(1003);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<double>(i + 1) * 0.25;
  }

  const double expected = DependentPipeline::SumFloat(data);
  const double result_4way = IndependentPipeline::SumFloat4Way(data);

  EXPECT_NEAR(result_4way, expected, 1e-6);
}

} // namespace
} // namespace hotpath::execution
