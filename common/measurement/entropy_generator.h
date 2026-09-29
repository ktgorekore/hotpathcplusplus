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

#ifndef COMMON_MEASUREMENT_ENTROPY_GENERATOR_H_
#define COMMON_MEASUREMENT_ENTROPY_GENERATOR_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace cognitas::trading {

/**
 * @brief Fast, reproducible dataset generator for empirical performance
 * testing.
 */
class DatasetGenerator {
 public:
  /**
   * @brief Generates a deterministic sequence of 32-bit integers.
   *
   * @param count Number of elements.
   * @param seed Random seed for reproducibility.
   * @return std::vector<int32_t> Vector of pseudorandom numbers.
   */
  static std::vector<int32_t> GenerateUniformRandom(size_t count,
                                                    uint32_t seed = 1337) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int32_t> dist(-100000, 100000);
    std::vector<int32_t> data(count);
    for (size_t i = 0; i < count; ++i) {
      data[i] = dist(rng);
    }
    return data;
  }

  /**
   * @brief Generates a sorted dataset for predictable branch testing.
   *
   * @param count Number of elements.
   * @param seed Random seed for reproducibility.
   * @return std::vector<int32_t> Sorted vector.
   */
  static std::vector<int32_t> GenerateSorted(size_t count,
                                             uint32_t seed = 1337) {
    auto data = GenerateUniformRandom(count, seed);
    std::sort(data.begin(), data.end());
    return data;
  }

  /**
   * @brief Generates a pointer-chasing index ring buffer of given size in
   * bytes.
   *
   * @param size_bytes Total size in bytes of the buffer.
   * @param randomized If true, creates a single random Hamiltonian cycle to
   * defeat hardware prefetchers.
   * @return std::vector<uint32_t> Next-index pointer chasing buffer.
   */
  static std::vector<uint32_t> GeneratePointerChaseRing(size_t size_bytes,
                                                        bool randomized) {
    size_t num_elements = size_bytes / sizeof(uint32_t);
    if (num_elements < 2) num_elements = 2;

    std::vector<uint32_t> indices(num_elements);
    std::iota(indices.begin(), indices.end(), 0);

    if (randomized) {
      // Create a single pseudo-random cycle
      std::mt19937 rng(42);
      std::shuffle(indices.begin() + 1, indices.end(), rng);
    }

    std::vector<uint32_t> next_ptrs(num_elements);
    for (size_t i = 0; i < num_elements - 1; ++i) {
      next_ptrs[indices[i]] = indices[i + 1];
    }
    next_ptrs[indices[num_elements - 1]] = indices[0];

    return next_ptrs;
  }
};

}  // namespace cognitas::trading

#endif  // COMMON_MEASUREMENT_ENTROPY_GENERATOR_H_
