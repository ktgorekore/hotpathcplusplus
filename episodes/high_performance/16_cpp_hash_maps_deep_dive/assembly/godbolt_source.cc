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
 * @brief Self-contained Compiler Explorer (Godbolt) comparison file for
 *        Episode 16 (`-std=c++20 -O3 -march=znver4`), contrasting:
 *   1. Story 1: Default `std::unordered_map<std::string, uint64_t>::find(const
 *      char*)` silently allocating a temporary `std::string` (`operator new` +
 *      `memcpy` + `operator delete`) vs. C++20 `is_transparent` heterogeneous
 *      lookup (zero allocations, cached 8B node hash) vs. GCC `noexcept`
 *      hash-caching trap (re-hashing stored heap strings via
 * `_ZSt11_Hash_bytes` across single-linked-list bucket boundaries).
 *   2. Story 1 / 4 Silicon SIMD Probing Mechanics across the three industrial
 *      C++ open-addressing architectures:
 *      - Abseil SwissTable: 16-slot control strip (`vpcmpeqb` + `vpmovmskb`,
 *        `0x80` EMPTY sentinel, `0xFE` DELETED tombstone).
 *      - Meta Folly F14: 14-slot chunk (`14` 1-byte tags + `scale` + `ofw`
 *        ref-counted overflow byte, masked by `0x3FFF`).
 *      - Boost.Unordered (`group15`): 15-slot metadata strip + byte 15
 *        8-bit overflow bitmask (`ofw & (1 << (h & 7))`), zero tombstones.
 */

#include <immintrin.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace hotpath::godbolt_ep16 {

/**
 * @struct TransparentStdStringHash
 * @brief C++20 transparent string hasher omitting `noexcept` so GCC libstdc++
 *        preserves `__cache_hash_code = true` on `std::unordered_map` nodes.
 */
struct TransparentStdStringHash {
  using is_transparent = void;

  /**
   * @brief Computes standard library hash over a `std::string_view`.
   * @param sv Zero-copy string view to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(std::string_view sv) const {
    return std::hash<std::string_view>{}(sv);
  }

  /**
   * @brief Computes standard library hash over an owning `std::string`.
   * @param s Owning string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const std::string& s) const {
    return std::hash<std::string_view>{}(s);
  }

  /**
   * @brief Computes standard library hash over a null-terminated C-string.
   * @param s Null-terminated C-string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const char* s) const {
    return std::hash<std::string_view>{}(s);
  }
};

/**
 * @struct TransparentStdStringHashNoexcept
 * @brief GCC libstdc++ gotcha variant: adding `noexcept` drops the 8-byte
 *        cached node hash and forces `_ZSt11_Hash_bytes` re-hashing on misses.
 */
struct TransparentStdStringHashNoexcept {
  using is_transparent = void;

  /**
   * @brief Computes hash over a `std::string_view` while marked `noexcept`.
   * @param sv Zero-copy string view to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }

  /**
   * @brief Computes hash over an owning `std::string` while marked `noexcept`.
   * @param s Owning string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }

  /**
   * @brief Computes hash over a null-terminated C-string while `noexcept`.
   * @param s Null-terminated C-string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const char* s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
};

/**
 * @brief Story 1A: default `std::unordered_map<std::string, uint64_t>::find`
 *        with a C-boundary `const char*` silently constructing a temporary
 *        `std::string` (calling `operator new` + `memcpy` + `operator delete`
 *        for routes exceeding GCC's 15B SSO buffer).
 * @param routes Route table keyed by owning `std::string`.
 * @param path Null-terminated incoming request route path.
 * @return Route identifier if found, or 0 otherwise.
 */
[[nodiscard]] std::uint64_t LookupRoute_DefaultAlloc(
    const std::unordered_map<std::string, std::uint64_t>& routes,
    const char* path) {
  const auto it = routes.find(path);
  return it != routes.end() ? it->second : 0ULL;
}

/**
 * @brief Story 1B: C++20 `P0919R3`/`P1690R1` transparent lookup with zero heap
 *        allocations and preserved libstdc++ 8-byte node hash caching.
 * @param routes Transparent route table keyed by `std::string`.
 * @param path Null-terminated incoming request route path.
 * @return Route identifier if found, or 0 otherwise.
 */
[[nodiscard]] std::uint64_t LookupRoute_Cpp20Transparent(
    const std::unordered_map<std::string, std::uint64_t,
                             TransparentStdStringHash, std::equal_to<>>& routes,
    const char* path) {
  const auto it = routes.find(path);
  return it != routes.end() ? it->second : 0ULL;
}

/**
 * @brief Story 1C: GCC libstdc++ `noexcept` trap — adding `noexcept` drops the
 *        8-byte node hash cache and forces `_ZSt11_Hash_bytes` re-hashing of
 *        stored heap strings during bucket traversal.
 * @param routes Transparent route table with `noexcept` hasher.
 * @param path Incoming zero-copy request route slice.
 * @return Route identifier if found, or 0 otherwise.
 */
[[nodiscard]] std::uint64_t LookupRoute_GccNoexceptTrap(
    const std::unordered_map<std::string, std::uint64_t,
                             TransparentStdStringHashNoexcept, std::equal_to<>>&
        routes,
    std::string_view path) noexcept {
  const auto it = routes.find(path);
  return it != routes.end() ? it->second : 0ULL;
}

/**
 * @struct SwissProbe
 * @brief Result of probing a 16-byte Abseil SwissTable control group.
 */
struct SwissProbe {
  std::uint32_t match_mask;
  bool has_empty;
};

/**
 * @brief Story 2A: probes 16 Abseil SwissTable control tags in parallel via
 *        SSE2 and checks for `kEmpty` (`0x80`) early termination.
 * @param ctrl16 Pointer to a 16-byte SwissTable control strip.
 * @param h2_tag 7-bit H2 fingerprint tag.
 * @return Candidate match bitmask and empty-slot indicator.
 */
[[nodiscard]] SwissProbe ProbeAbseil16(const std::int8_t* ctrl16,
                                       std::int8_t h2_tag) noexcept {
  const __m128i ctrl =
      _mm_loadu_si128(reinterpret_cast<const __m128i*>(ctrl16));
  const __m128i m = _mm_cmpeq_epi8(ctrl, _mm_set1_epi8(h2_tag));
  const __m128i e = _mm_cmpeq_epi8(ctrl, _mm_set1_epi8(-128));
  return SwissProbe{
      static_cast<std::uint32_t>(_mm_movemask_epi8(m)),
      _mm_movemask_epi8(e) != 0,
  };
}

/**
 * @struct F14ChunkHeader
 * @brief 16-byte Meta Folly F14 chunk metadata header (14 tags + scale +
 *        reference-counted overflow byte).
 */
struct alignas(16) F14ChunkHeader {
  std::uint8_t tags[14];
  std::uint8_t hosted_scale;
  std::uint8_t overflow_count;
};

/**
 * @struct F14Probe
 * @brief Result of probing a 14-slot Meta Folly F14 chunk header.
 */
struct F14Probe {
  std::uint32_t match_mask;
  bool stop_probing;
};

/**
 * @brief Story 2B: probes 14 Folly F14 tags via SSE2 (`& 0x3FFF`) and checks
 *        the reference-counted `overflow_count == 0` early-exit condition.
 * @param chunk Pointer to a 16-byte aligned `F14ChunkHeader`.
 * @param tag 8-bit F14 slot tag (`0x80..0xFF` for occupied slots).
 * @return 14-bit candidate match mask and probe-stop flag.
 */
[[nodiscard]] F14Probe ProbeFollyF14(const F14ChunkHeader* chunk,
                                     std::uint8_t tag) noexcept {
  const __m128i raw = _mm_load_si128(reinterpret_cast<const __m128i*>(chunk));
  const __m128i m = _mm_cmpeq_epi8(raw, _mm_set1_epi8(static_cast<char>(tag)));
  return F14Probe{
      static_cast<std::uint32_t>(_mm_movemask_epi8(m)) & 0x3FFFU,
      chunk->overflow_count == 0U,
  };
}

/**
 * @struct BoostGroup15
 * @brief 16-byte Boost.Unordered `group15` metadata strip (15 slot tags +
 *        byte 15 8-bit overflow bitmask).
 */
struct alignas(16) BoostGroup15 {
  std::uint8_t tags[15];
  std::uint8_t ofw_mask;
};

/**
 * @struct BoostProbe
 * @brief Result of probing a 15-slot Boost.Unordered `group15` metadata strip.
 */
struct BoostProbe {
  std::uint32_t match_mask;
  bool stop_probing;
};

/**
 * @brief Story 2C: probes 15 Boost.Unordered `group15` tags via SSE2
 *        (`& 0x7FFF`) and tests bit `(full_hash & 7)` of the overflow byte.
 * @param grp Pointer to a 16-byte aligned `BoostGroup15` header.
 * @param reduced_hash 8-bit reduced tag byte.
 * @param full_hash Full 64-bit avalanched key hash selecting the overflow bit.
 * @return 15-bit candidate match mask and probe-stop flag.
 */
[[nodiscard]] BoostProbe ProbeBoostGroup15(const BoostGroup15* grp,
                                           std::uint8_t reduced_hash,
                                           std::size_t full_hash) noexcept {
  const __m128i raw = _mm_load_si128(reinterpret_cast<const __m128i*>(grp));
  const __m128i m =
      _mm_cmpeq_epi8(raw, _mm_set1_epi8(static_cast<char>(reduced_hash)));
  return BoostProbe{
      static_cast<std::uint32_t>(_mm_movemask_epi8(m)) & 0x7FFFU,
      (grp->ofw_mask & static_cast<std::uint8_t>(1U << (full_hash & 7U))) == 0U,
  };
}

}  // namespace hotpath::godbolt_ep16
