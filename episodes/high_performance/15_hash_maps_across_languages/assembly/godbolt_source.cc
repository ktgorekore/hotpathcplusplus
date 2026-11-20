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
 * @file godbolt_source.cc
 * @brief Self-contained Compiler Explorer (Godbolt) comparison file
 * contrasting x86-64 codegen (`-std=c++20 -O3 -march=znver4`) for:
 *   1. `std::unordered_map<uint64_t, uint64_t>::find` (identity hash, prime
 *      modulo / libdivide, and linked-node pointer chasing),
 *   2. SwissTable 16-slot SIMD control-byte group probing (`vpcmpeqb` +
 *      `vpmovmskb` + `tzcnt`), and
 *   3. `FxHash` single-multiply vs. SipHash-1-3 ARX compression round.
 */

#include <immintrin.h>

#include <bit>
#include <cstdint>
#include <unordered_map>

namespace hotpath::godbolt_ep15 {

// 1. Standard C++20 node-chaining lookup (GCC libstdc++ uses identity hash).
std::uint64_t FindStdUnorderedMap(
    const std::unordered_map<std::uint64_t, std::uint64_t>& table,
    std::uint64_t key) noexcept {
  const auto it = table.find(key);
  return (it != table.end()) ? it->second : 0ULL;
}

// 2. SwissTable / hashbrown 16-slot SSE2 control-byte probe:
//    Checks 16 H2 tags in parallel and tests for EMPTY (0x80) early exit.
struct ProbeResult {
  std::uint32_t match_mask;
  bool has_empty;
};

[[nodiscard]] ProbeResult ProbeSwissGroup16(const std::int8_t* ctrl_group,
                                            std::int8_t h2_tag) noexcept {
  const __m128i ctrl =
      _mm_loadu_si128(reinterpret_cast<const __m128i*>(ctrl_group));
  const __m128i match_vec = _mm_cmpeq_epi8(ctrl, _mm_set1_epi8(h2_tag));
  const __m128i empty_vec = _mm_cmpeq_epi8(ctrl, _mm_set1_epi8(-128));
  return ProbeResult{
      static_cast<std::uint32_t>(_mm_movemask_epi8(match_vec)),
      _mm_movemask_epi8(empty_vec) != 0,
  };
}

[[nodiscard]] int NextCandidateSlot(std::uint32_t& mask) noexcept {
  const int bit = std::countr_zero(mask);
  mask &= (mask - 1U);
  return bit;
}

// 3. Hashing trade-off: 1-cycle FxHash multiply vs. SipHash-1-3 ARX round.
[[nodiscard]] std::uint64_t HashFxU64(std::uint64_t key) noexcept {
  return key * 0x517cc1b727220a95ULL;
}

[[nodiscard]] std::uint64_t SipRoundMix(std::uint64_t v0, std::uint64_t v1,
                                        std::uint64_t v2,
                                        std::uint64_t v3) noexcept {
  v0 += v1;
  v1 = std::rotl(v1, 13) ^ v0;
  v0 = std::rotl(v0, 32);
  v2 += v3;
  v3 = std::rotl(v3, 16) ^ v2;
  v0 += v3;
  v3 = std::rotl(v3, 21) ^ v0;
  v2 += v1;
  v1 = std::rotl(v1, 17) ^ v2;
  v2 = std::rotl(v2, 32);
  return v0 ^ v1 ^ v2 ^ v3;
}

}  // namespace hotpath::godbolt_ep15
