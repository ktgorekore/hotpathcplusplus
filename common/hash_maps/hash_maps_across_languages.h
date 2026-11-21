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

#ifndef HOTPATH_COMMON_HASH_MAPS_HASH_MAPS_ACROSS_LANGUAGES_H_
#define HOTPATH_COMMON_HASH_MAPS_HASH_MAPS_ACROSS_LANGUAGES_H_

/**
 * @file hash_maps_across_languages.h
 * @brief Episode 15 ("Hash Maps Across Languages") C++20 core workloads.
 *
 * Models an Edge API Gateway Rate-Limiter & Session Directory across:
 *   - Battle 1: Warm L1 (1,024 accounts) vs. Aged Post-Churn (2,000,000
 *     accounts) 80/20 hit/miss and 100% negative-miss lookups.
 *   - Battle 2: Resident memory footprint (VmRSS delta, bytes/entry) and
 *     unreserved vs. reserved streaming insert tail latency (p50..Max).
 *   - Battle 3: Adversarial HashDoS collision resilience on prime-bucket
 *     multiples against libstdc++ std::unordered_map vs. absl::flat_hash_map.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"

namespace hotpath::episode15 {

inline constexpr std::uint64_t kSeedKeys = 0xC0FFEE15ULL;
inline constexpr std::uint64_t kSeedQueries = 0xFACE0015ULL;
inline constexpr std::uint64_t kSeedMisses = 0xBAD00015ULL;
inline constexpr std::uint64_t kSeedChurn = 0x99990015ULL;

inline constexpr std::size_t kNumL1Entries = 1024;
inline constexpr std::size_t kNumAgedEntries = 2'000'000;
inline constexpr std::size_t kNumQueries = 2'000'000;
inline constexpr std::size_t kNumHashDosKeys = 40'000;

/**
 * @brief Realistic 24-byte Edge Gateway SessionState stored by value in C++
 *        maps (or boxed on the heap in reference-oriented runtimes).
 */
struct alignas(8) SessionState {
  std::uint64_t quota_remaining;
  std::uint64_t last_seen_ns;
  std::uint32_t tier_flags;
  std::uint32_t rate_window_hits;
};
static_assert(sizeof(SessionState) == 24, "SessionState must remain 24 bytes");

/**
 * @brief Stateful SplitMix64 PRNG shared bit-for-bit across C++, Rust, Go,
 *        Java, and Python implementations.
 * @param state Mutable 64-bit PRNG state.
 * @return Next uniformly distributed 64-bit unsigned integer.
 */
[[nodiscard]] inline std::uint64_t SplitMix64(std::uint64_t& state) noexcept {
  state += 0x9e3779b97f4a7c15ULL;
  std::uint64_t z = state;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

/**
 * @brief Generates a deterministic positive 62-bit account ID guaranteed
 *        disjoint from absent-key streams (bit 62 == 0).
 * @param state Mutable 64-bit PRNG state.
 * @return Non-zero 62-bit positive account ID.
 */
[[nodiscard]] inline std::uint64_t MakePresentKey(
    std::uint64_t& state) noexcept {
  return (SplitMix64(state) & 0x3FFFFFFFFFFFFFFFULL) | 1ULL;
}

/**
 * @brief Generates a deterministic positive 63-bit absent key guaranteed
 *        never to collide with any MakePresentKey output (bit 62 == 1).
 * @param state Mutable 64-bit PRNG state.
 * @return Non-zero 63-bit key with bit 62 set.
 */
[[nodiscard]] inline std::uint64_t MakeAbsentKey(
    std::uint64_t& state) noexcept {
  return (SplitMix64(state) & 0x3FFFFFFFFFFFFFFFULL) | (1ULL << 62) | 1ULL;
}

/**
 * @brief Packs timestamp and token-bucket quota into a positive 63-bit word.
 * @param raw High-entropy 64-bit PRNG sample.
 * @return Packed token-bucket state word.
 */
[[nodiscard]] inline std::uint64_t PackTokenBucketState(
    std::uint64_t raw) noexcept {
  const std::uint64_t last_seen_ms = (raw >> 24) & 0x7FFFFFFFFFULL;
  const std::uint64_t quota = (raw & 0xFFFFULL) + 1ULL;
  return (last_seen_ms << 24) | quota;
}

/**
 * @brief Builds the 24-byte SessionState corresponding to a packed account.
 * @param key Account key.
 * @param packed Packed token-bucket word from PackTokenBucketState().
 * @return Deterministic SessionState struct.
 */
[[nodiscard]] inline SessionState MakeSessionState(
    std::uint64_t key, std::uint64_t packed) noexcept {
  const std::uint64_t quota = packed & 0xFFFFFFULL;
  const std::uint64_t last_seen_ms = (packed >> 24) & 0x7FFFFFFFFFULL;
  return SessionState{
      quota,
      last_seen_ms * 1'000'000ULL,
      static_cast<std::uint32_t>(key & 0xFFFFULL),
      static_cast<std::uint32_t>((key >> 16) & 0xFFULL),
  };
}

/**
 * @brief Computes deterministic fold value for SessionState verification.
 * @param s SessionState record.
 * @return 64-bit digest matching Rust, Go, Java, and Python implementations.
 */
[[nodiscard]] inline std::uint64_t DigestSessionState(
    const SessionState& s) noexcept {
  return s.quota_remaining ^ s.last_seen_ns ^
         static_cast<std::uint64_t>(s.tier_flags) ^
         static_cast<std::uint64_t>(s.rate_window_hits);
}

/**
 * @brief Deterministic rate-limiter dataset shared identically across all five
 *        languages.
 */
struct GatewayDataset {
  std::vector<std::uint64_t> keys;
  std::vector<std::uint64_t> packed_vals;
  std::vector<SessionState> session_vals;
  std::vector<std::uint64_t> queries_80_20;
  std::vector<std::uint64_t> queries_100_miss;
};

/**
 * @brief Builds a deterministic Edge Gateway dataset with N entries and Q
 *        queries.
 * @param num_entries Number of active accounts in the directory.
 * @param num_queries Number of lookup queries per pass.
 * @return Populated GatewayDataset.
 */
[[nodiscard]] inline GatewayDataset BuildDataset(std::size_t num_entries,
                                                 std::size_t num_queries) {
  GatewayDataset ds;
  ds.keys.reserve(num_entries);
  ds.packed_vals.reserve(num_entries);
  ds.session_vals.reserve(num_entries);

  std::uint64_t key_rng = kSeedKeys;
  for (std::size_t i = 0; i < num_entries; ++i) {
    const std::uint64_t k = MakePresentKey(key_rng);
    const std::uint64_t raw = SplitMix64(key_rng);
    const std::uint64_t packed = PackTokenBucketState(raw);
    ds.keys.push_back(k);
    ds.packed_vals.push_back(packed);
    ds.session_vals.push_back(MakeSessionState(k, packed));
  }

  ds.queries_80_20.reserve(num_queries);
  std::uint64_t q_rng = kSeedQueries;
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(q_rng);
    if ((r % 5ULL) != 0ULL) {
      const std::size_t idx = static_cast<std::size_t>((r >> 8) % num_entries);
      ds.queries_80_20.push_back(ds.keys[idx]);
    } else {
      ds.queries_80_20.push_back((r & 0x3FFFFFFFFFFFFFFFULL) | (1ULL << 62) |
                                 1ULL);
    }
  }

  ds.queries_100_miss.reserve(num_queries);
  std::uint64_t m_rng = kSeedMisses;
  for (std::size_t i = 0; i < num_queries; ++i) {
    ds.queries_100_miss.push_back(MakeAbsentKey(m_rng));
  }

  return ds;
}

/**
 * @brief Mixed-size heap arena guard simulating realistic long-running backend
 *        memory churn so newly allocated hash-table nodes do not sit
 *        artificially contiguous on a virgin startup heap.
 */
class HeapChurnGuard {
 public:
  /**
   * @brief Allocates variable-sized 32B..256B buffers and frees alternating
   *        slots in checkerboard pattern, leaving holes in tcache/bins.
   * @param chunk_count Number of initial churn chunks to allocate.
   */
  explicit HeapChurnGuard(std::size_t chunk_count = 300'000) {
    std::uint64_t rng = kSeedChurn;
    chunks_.reserve(chunk_count);
    for (std::size_t i = 0; i < chunk_count; ++i) {
      const std::size_t sz =
          32U + static_cast<std::size_t>((SplitMix64(rng) & 7ULL) * 32U);
      auto buf = std::make_unique<std::uint8_t[]>(sz);
      buf[0] = static_cast<std::uint8_t>(i & 0xFFU);
      chunks_.push_back(std::move(buf));
    }
    // Checkerboard free alternating slots to fragment allocator bins.
    for (std::size_t i = 0; i < chunks_.size(); i += 2) {
      chunks_[i].reset();
    }
  }

  /** @brief Releases remaining noise chunks once the target map is built. */
  void ReleaseRemainingNoise() { chunks_.clear(); }

 private:
  std::vector<std::unique_ptr<std::uint8_t[]>> chunks_;
};

/**
 * @brief Reads Linux process resident set size (VmRSS) and high-water mark
 *        (VmHWM) in bytes from /proc/self/status.
 * @param rss_bytes Out parameter receiving VmRSS in bytes (0 on failure).
 * @param hwm_bytes Out parameter receiving VmHWM in bytes (0 on failure).
 */
inline void ReadProcessMemoryBytes(std::uint64_t& rss_bytes,
                                   std::uint64_t& hwm_bytes) noexcept {
  rss_bytes = 0;
  hwm_bytes = 0;
  std::ifstream in("/proc/self/status");
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      rss_bytes = std::stoull(line.substr(6)) * 1024ULL;
    } else if (line.rfind("VmHWM:", 0) == 0) {
      hwm_bytes = std::stoull(line.substr(6)) * 1024ULL;
    }
  }
}

/** @brief Summary percentiles for streaming insert latency telemetry. */
struct TailPercentiles {
  double total_ms;
  double p50_ns;
  double p99_ns;
  double p999_ns;
  double p9999_ns;
  double max_ns;
  std::uint64_t checksum;
};

/**
 * @brief Computes exact percentiles from a vector of nanosecond samples.
 * @param samples Mutable sample vector (sorted in place).
 * @param total_ms Elapsed wall-clock time across all inserts in milliseconds.
 * @param checksum Final checksum preventing dead-code elimination.
 * @return Computed TailPercentiles structure.
 */
[[nodiscard]] inline TailPercentiles ComputePercentiles(
    std::vector<std::uint64_t>& samples, double total_ms,
    std::uint64_t checksum) {
  if (samples.empty()) {
    return TailPercentiles{total_ms, 0.0, 0.0, 0.0, 0.0, 0.0, checksum};
  }
  std::sort(samples.begin(), samples.end());
  const auto at_pct = [&](double pct) -> double {
    const std::size_t idx = std::min(
        samples.size() - 1, static_cast<std::size_t>(
                                pct * static_cast<double>(samples.size() - 1)));
    return static_cast<double>(samples[idx]);
  };
  return TailPercentiles{
      total_ms,      at_pct(0.50),   at_pct(0.99),
      at_pct(0.999), at_pct(0.9999), static_cast<double>(samples.back()),
      checksum,
  };
}

/**
 * @brief Discovers GCC libstdc++'s exact prime bucket count for `count` keys
 *        and generates `count` colliding multiples `(i + 1) * B_prime` that all
 *        map to bucket 0 under libstdc++'s identity `std::hash<uint64_t>`.
 * @param count Number of adversarial colliding keys to generate.
 * @param discovered_prime Out parameter receiving the prime bucket count.
 * @return Vector of `count` keys colliding into bucket 0.
 */
[[nodiscard]] inline std::vector<std::uint64_t> BuildLibstdcxxHashDosKeys(
    std::size_t count, std::uint64_t& discovered_prime) {
  std::unordered_map<std::uint64_t, std::uint64_t> probe;
  probe.reserve(count);
  discovered_prime = static_cast<std::uint64_t>(probe.bucket_count());

  std::vector<std::uint64_t> keys;
  keys.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    keys.push_back(static_cast<std::uint64_t>(i + 1) * discovered_prime);
  }
  return keys;
}

/**
 * @brief Combines accumulated value sum and hit count into a cross-language
 *        verification digest.
 * @param sum Accumulated hash/value sum across probe/iteration passes.
 * @param hits Total successful key hits encountered during the pass.
 * @return 64-bit cross-language verification digest.
 */
[[nodiscard]] inline std::uint64_t FinalizeChecksum(
    std::uint64_t sum, std::uint64_t hits) noexcept {
  return sum ^ (hits * 0x9e3779b97f4a7c15ULL);
}

/**
 * @brief Builds an aged std::unordered_map<uint64_t, uint64_t> after heap
 *        fragmentation churn so nodes span dispersed memory pages.
 * @param ds Pre-built deterministic Edge Gateway dataset.
 * @param reserve_capacity Whether to pre-allocate buckets via reserve().
 * @return Populated std::unordered_map<uint64_t, uint64_t>.
 */
[[nodiscard]] inline std::unordered_map<std::uint64_t, std::uint64_t>
BuildAgedStdUnorderedPacked(const GatewayDataset& ds, bool reserve_capacity) {
  HeapChurnGuard churn;
  std::unordered_map<std::uint64_t, std::uint64_t> m;
  if (reserve_capacity) {
    m.reserve(ds.keys.size());
  }
  std::vector<std::unique_ptr<std::uint8_t[]>> temp_reqs;
  temp_reqs.reserve(256);
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    m.emplace(ds.keys[i], ds.packed_vals[i]);
    // Interleave transient 64B request scratch allocations during table build.
    if ((i & 7ULL) == 0ULL) {
      temp_reqs.push_back(std::make_unique<std::uint8_t[]>(64));
      if (temp_reqs.size() >= 128U) {
        temp_reqs.clear();
      }
    }
  }
  churn.ReleaseRemainingNoise();
  return m;
}

/**
 * @brief Builds an aged absl::flat_hash_map<uint64_t, uint64_t> under the
 *        exact same heap-churn harness.
 * @param ds Pre-built deterministic Edge Gateway dataset.
 * @param reserve_capacity Whether to pre-allocate slots via reserve().
 * @return Populated absl::flat_hash_map<uint64_t, uint64_t>.
 */
[[nodiscard]] inline absl::flat_hash_map<std::uint64_t, std::uint64_t>
BuildAgedAbslFlatPacked(const GatewayDataset& ds, bool reserve_capacity) {
  HeapChurnGuard churn;
  absl::flat_hash_map<std::uint64_t, std::uint64_t> m;
  if (reserve_capacity) {
    m.reserve(ds.keys.size());
  }
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    m.emplace(ds.keys[i], ds.packed_vals[i]);
  }
  churn.ReleaseRemainingNoise();
  return m;
}

/**
 * @brief Builds an aged std::unordered_map<uint64_t, SessionState> (`24B`
 *        value stored inline inside heap node).
 * @param ds Pre-built deterministic Edge Gateway dataset.
 * @return Populated std::unordered_map<uint64_t, SessionState>.
 */
[[nodiscard]] inline std::unordered_map<std::uint64_t, SessionState>
BuildAgedStdUnorderedSession(const GatewayDataset& ds) {
  HeapChurnGuard churn;
  std::unordered_map<std::uint64_t, SessionState> m;
  m.reserve(ds.keys.size());
  std::vector<std::unique_ptr<std::uint8_t[]>> temp_reqs;
  temp_reqs.reserve(256);
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    m.emplace(ds.keys[i], ds.session_vals[i]);
    if ((i & 7ULL) == 0ULL) {
      temp_reqs.push_back(std::make_unique<std::uint8_t[]>(80));
      if (temp_reqs.size() >= 128U) {
        temp_reqs.clear();
      }
    }
  }
  churn.ReleaseRemainingNoise();
  return m;
}

/**
 * @brief Builds an aged absl::flat_hash_map<uint64_t, SessionState> (`24B`
 *        value stored inline in flat slot array).
 * @param ds Pre-built deterministic Edge Gateway dataset.
 * @return Populated absl::flat_hash_map<uint64_t, SessionState>.
 */
[[nodiscard]] inline absl::flat_hash_map<std::uint64_t, SessionState>
BuildAgedAbslFlatSession(const GatewayDataset& ds) {
  HeapChurnGuard churn;
  absl::flat_hash_map<std::uint64_t, SessionState> m;
  m.reserve(ds.keys.size());
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    m.emplace(ds.keys[i], ds.session_vals[i]);
  }
  churn.ReleaseRemainingNoise();
  return m;
}

/**
 * @brief Evicts 90% of keys (retaining i % 10 == 0) without shrinking
 *        allocated capacity, modeling post-peak TTL expiration.
 * @tparam MapT Associative map type supporting erase(key).
 * @param map Mutable populated hash map.
 * @param ds Dataset whose keys were inserted into `map`.
 */
template <typename MapT>
inline void EvictNinetyPercent(MapT& map, const GatewayDataset& ds) {
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    if ((i % 10U) != 0U) {
      map.erase(ds.keys[i]);
    }
  }
}

/**
 * @brief Builds a post-peak sparse std::unordered_map with 90% entries evicted
 *        and full 2M bucket array retained.
 * @param ds Pre-built deterministic Edge Gateway dataset.
 * @return Populated sparse std::unordered_map<uint64_t, uint64_t>.
 */
[[nodiscard]] inline std::unordered_map<std::uint64_t, std::uint64_t>
BuildAgedStdUnorderedPackedSparse(const GatewayDataset& ds) {
  auto m = BuildAgedStdUnorderedPacked(ds, true);
  EvictNinetyPercent(m, ds);
  return m;
}

/**
 * @brief Builds a post-peak sparse absl::flat_hash_map with 90% entries evicted
 *        as tombstones inside the retained 4.19M-slot control array.
 * @param ds Pre-built deterministic Edge Gateway dataset.
 * @return Populated sparse absl::flat_hash_map<uint64_t, uint64_t>.
 */
[[nodiscard]] inline absl::flat_hash_map<std::uint64_t, std::uint64_t>
BuildAgedAbslFlatPackedSparse(const GatewayDataset& ds) {
  auto m = BuildAgedAbslFlatPacked(ds, true);
  EvictNinetyPercent(m, ds);
  return m;
}

}  // namespace hotpath::episode15

// Clean non-mangled C-linkage lookup kernels for llvm-objdump / perf annotate.
extern "C" {

/**
 * @brief Timed lookup batch against std::unordered_map<uint64_t, uint64_t>.
 * @param map Populated standard unordered map.
 * @param queries Pointer to contiguous 64-bit lookup query keys.
 * @param n Number of query keys in `queries`.
 * @return SplitMix-finalized cross-language verification checksum.
 */
[[gnu::noinline]] inline std::uint64_t Ep15StdUnorderedFindPacked(
    const std::unordered_map<std::uint64_t, std::uint64_t>& map,
    const std::uint64_t* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += it->second;
      ++hits;
    }
  }
  return hotpath::episode15::FinalizeChecksum(sum, hits);
}

/**
 * @brief Timed lookup batch against absl::flat_hash_map<uint64_t, uint64_t>.
 * @param map Populated Abseil flat SwissTable map.
 * @param queries Pointer to contiguous 64-bit lookup query keys.
 * @param n Number of query keys in `queries`.
 * @return SplitMix-finalized cross-language verification checksum.
 */
[[gnu::noinline]] inline std::uint64_t Ep15AbslFlatFindPacked(
    const absl::flat_hash_map<std::uint64_t, std::uint64_t>& map,
    const std::uint64_t* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += it->second;
      ++hits;
    }
  }
  return hotpath::episode15::FinalizeChecksum(sum, hits);
}

/**
 * @brief Timed lookup batch against std::unordered_map<uint64_t, SessionState>.
 * @param map Populated session map keyed by 64-bit account ID.
 * @param queries Pointer to contiguous 64-bit lookup query keys.
 * @param n Number of query keys in `queries`.
 * @return SplitMix-finalized cross-language verification checksum.
 */
[[gnu::noinline]] inline std::uint64_t Ep15StdUnorderedFindSession(
    const std::unordered_map<std::uint64_t, hotpath::episode15::SessionState>&
        map,
    const std::uint64_t* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode15::DigestSessionState(it->second);
      ++hits;
    }
  }
  return hotpath::episode15::FinalizeChecksum(sum, hits);
}

/**
 * @brief Timed lookup batch against absl::flat_hash_map<uint64_t,
 *        SessionState>.
 * @param map Populated Abseil SwissTable session map.
 * @param queries Pointer to contiguous 64-bit lookup query keys.
 * @param n Number of query keys in `queries`.
 * @return SplitMix-finalized cross-language verification checksum.
 */
[[gnu::noinline]] inline std::uint64_t Ep15AbslFlatFindSession(
    const absl::flat_hash_map<std::uint64_t, hotpath::episode15::SessionState>&
        map,
    const std::uint64_t* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode15::DigestSessionState(it->second);
      ++hits;
    }
  }
  return hotpath::episode15::FinalizeChecksum(sum, hits);
}

/**
 * @brief Timed full/sparse sweep across std::unordered_map<uint64_t, uint64_t>.
 * @param map Populated standard unordered map.
 * @return SplitMix-finalized sweep checksum over all live entries.
 */
[[gnu::noinline]] inline std::uint64_t Ep15StdUnorderedIterPacked(
    const std::unordered_map<std::uint64_t, std::uint64_t>& map) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t count = 0;
  for (const auto& [k, v] : map) {
    sum += (k ^ v);
    ++count;
  }
  return hotpath::episode15::FinalizeChecksum(sum, count);
}

/**
 * @brief Timed full/sparse sweep across absl::flat_hash_map<uint64_t,
 *        uint64_t>.
 * @param map Populated Abseil flat SwissTable map.
 * @return SplitMix-finalized sweep checksum over all live entries.
 */
[[gnu::noinline]] inline std::uint64_t Ep15AbslFlatIterPacked(
    const absl::flat_hash_map<std::uint64_t, std::uint64_t>& map) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t count = 0;
  for (const auto& [k, v] : map) {
    sum += (k ^ v);
    ++count;
  }
  return hotpath::episode15::FinalizeChecksum(sum, count);
}

}  // extern "C"

#endif  // HOTPATH_COMMON_HASH_MAPS_HASH_MAPS_ACROSS_LANGUAGES_H_
