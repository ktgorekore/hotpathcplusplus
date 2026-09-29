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

#ifndef COMMON_MEASUREMENT_MEASUREMENT_HARNESS_H_
#define COMMON_MEASUREMENT_MEASUREMENT_HARNESS_H_

#include <x86intrin.h>

#include <cstddef>
#include <cstdint>

namespace cognitas::trading {

/**
 * @brief Reads the Time Stamp Counter (TSC) with serializing instruction
 * barrier.
 *
 * Uses `__rdtscp` which forces instruction serialization and returns both the
 * 64-bit cycle count and the CPU processor core ID.
 *
 * @param[out] chip_id Optional pointer to receive processor core ID.
 * @return 64-bit CPU cycle timestamp.
 */
inline uint64_t ReadTscSerialized(uint32_t* chip_id = nullptr) {
  uint32_t aux = 0;
  uint64_t tsc = __rdtscp(&aux);
  if (chip_id != nullptr) {
    *chip_id = aux;
  }
  return tsc;
}

/**
 * @brief Flushes a contiguous memory buffer from the entire CPU cache
 * hierarchy.
 *
 * Iterates through the buffer in 64-byte cache line strides, issuing
 * `_mm_clflushopt` (or `_mm_clflush`) followed by `_mm_mfence` to guarantee
 * eviction to DRAM.
 *
 * @param data Pointer to the memory buffer.
 * @param bytes Size of the buffer in bytes.
 */
inline void FlushCacheRange(const void* data, size_t bytes) {
  const auto* ptr = static_cast<const char*>(data);
  for (size_t offset = 0; offset < bytes; offset += 64) {
    _mm_clflush(ptr + offset);
  }
  _mm_mfence();
}

/**
 * @brief Explicit compiler optimization barrier for values.
 *
 * Informs the compiler that the value is accessed by external assembly,
 * forcing it to be materialized in memory without adding actual CPU
 * instructions.
 *
 * @tparam T Type of the value to protect.
 * @param val Reference to the value.
 */
template <typename T>
inline void ForceOptimizationBarrier(T& val) {
  asm volatile("" : "+m"(val) : : "memory");
}

template <typename T>
inline void ForceOptimizationBarrier(const T& val) {
  asm volatile("" : : "m"(val) : "memory");
}

}  // namespace cognitas::trading

#endif  // COMMON_MEASUREMENT_MEASUREMENT_HARNESS_H_
