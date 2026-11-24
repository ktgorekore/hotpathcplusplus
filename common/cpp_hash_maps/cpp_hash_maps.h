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

#ifndef HOTPATH_COMMON_CPP_HASH_MAPS_CPP_HASH_MAPS_H_
#define HOTPATH_COMMON_CPP_HASH_MAPS_CPP_HASH_MAPS_H_

/**
 * @file cpp_hash_maps.h
 * @brief Episode 16 ("Production C++ Hash Maps") domain workloads & kernels.
 *
 * Models five real-world production hash-map workloads inside an Envoy-style
 * Edge HTTP/RPC Gateway and Connection Router:
 *   - Story 1: HTTP/RPC Route Table (`std::string` 36-44B non-SSO keys queried
 *     via `std::string_view`, Zipfian s=0.99 hot skew + 30% negative misses on
 *     an aged heap), contrasting C++20 `is_transparent` heterogeneous lookup,
 *     GCC libstdc++ `noexcept` hash-caching mechanics,
 *     `std::pmr::monotonic_buffer_resource`, Abseil SwissTable, Meta Folly F14,
 *     and Boost.Unordered `unordered_flat_map`.
 *   - Story 2: Per-Request Small Maps (`N = 4..128` HTTP/2 request headers)
 *     measuring full construct + insert + lookup + destroy lifecycles across
 *     stack-inlined flat scans (`absl::InlinedVector`), `std::vector`,
 *     `std::unordered_map`, `absl::flat_hash_map`, `folly::F14FastMap`, and
 *     `boost::unordered_flat_map`.
 *   - Story 3: Large-Value RAM Cliff & Dense Iteration (`1,000,000` entries
 *     post-doubling & `800,000` entries pre-doubling across `32B`
 *     `CompactFlowStats`, `128B` `ConnectionMetadata`, and `256B`
 *     `UpstreamEndpointProfile`), comparing flat maps, `folly::F14FastMap`
 *     (`F14VectorMap` auto-switch at `>= 24B`), `F14ValueMap`, node maps, and
 *     `absl::flat_hash_map<K, std::unique_ptr<V>>`.
 *   - Story 4: Steady-State Sliding-Window Eviction Churn (`780,000` and
 *     `640,000` live entries under `4,000,000` continuous `erase + emplace`
 *     cycles), comparing Abseil tombstones (`kDeleted`), Folly F14
 *     reference-counted overflow bytes, Boost.Unordered overflow bitmask +
 *     anti-drift, and `std::unordered_map` heap churn.
 *   - Story 5: Pointer-Stability Migration Landmine (silent Use-After-Free in
 *     flat maps vs. address stability and throughput of `absl::node_hash_map`,
 *     `folly::F14NodeMap`, `boost::unordered_node_map`, and
 *     `absl::flat_hash_map<K, std::unique_ptr<V>>`).
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/container/node_hash_map.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "boost/container_hash/hash.hpp"
#include "boost/unordered/unordered_flat_map.hpp"
#include "boost/unordered/unordered_node_map.hpp"
#include "folly/container/F14Map.h"

namespace hotpath::episode16 {

/** @brief Deterministic PRNG seed for Story 1 HTTP/RPC route table dataset. */
inline constexpr std::uint64_t kSeedRoutes = 0x524F555416ULL;
/** @brief Deterministic PRNG seed for Story 2 HTTP/2 header dataset. */
inline constexpr std::uint64_t kSeedHeaders = 0x4845414416ULL;
/** @brief Deterministic PRNG seed for Story 3/5 connection/endpoint dataset. */
inline constexpr std::uint64_t kSeedConns = 0x434F4E4E16ULL;
/** @brief Deterministic PRNG seed for Story 4 eviction-churn dataset. */
inline constexpr std::uint64_t kSeedChurn = 0x4348555216ULL;
/** @brief Deterministic PRNG seed for `HeapChurnGuard` fragmentation noise. */
inline constexpr std::uint64_t kSeedHeapNoise = 0x99990016ULL;

/** @brief Default route table cardinality (100K active gateway routes). */
inline constexpr std::size_t kNumRoutes = 100'000;
/** @brief Default lookup query batch size for Story 1 forensic runs. */
inline constexpr std::size_t kNumRouteQueries = 1'000'000;
/** @brief Post-doubling entry count for Story 3 RAM cliff (~48% flat fill). */
inline constexpr std::size_t kNumLargeEntriesPostDouble = 1'000'000;
/** @brief Pre-doubling entry count for Story 3 best-case (~76% flat fill). */
inline constexpr std::size_t kNumLargeEntriesPreDouble = 800'000;
/** @brief Default lookup query batch size for Story 3/5 forensic runs. */
inline constexpr std::size_t kNumLargeQueries = 1'000'000;
/** @brief Band A steady-state active count for Story 4 eviction churn. */
inline constexpr std::size_t kNumChurnLiveEntries = 780'000;
/** @brief Total sliding-window `erase + emplace` cycles in Story 4. */
inline constexpr std::size_t kNumChurnOps = 4'000'000;
/** @brief Pre- and post-churn lookup probe batch size in Story 4. */
inline constexpr std::size_t kNumChurnPostQueries = 1'000'000;

/**
 * @brief Stateful SplitMix64 PRNG for deterministic reproducible workloads.
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
 * @brief Generates a positive 62-bit present key disjoint from absent keys.
 * @param state Mutable 64-bit PRNG state.
 * @return Present key with bit 62 cleared and bit 0 set.
 */
[[nodiscard]] inline std::uint64_t MakePresentKey(
    std::uint64_t& state) noexcept {
  return (SplitMix64(state) & 0x3FFFFFFFFFFFFFFFULL) | 1ULL;
}

/**
 * @brief Generates a positive 63-bit absent key disjoint from present keys.
 * @param state Mutable 64-bit PRNG state.
 * @return Absent key with bit 62 set and bit 0 set.
 */
[[nodiscard]] inline std::uint64_t MakeAbsentKey(
    std::uint64_t& state) noexcept {
  return (SplitMix64(state) & 0x3FFFFFFFFFFFFFFFULL) | (1ULL << 62) | 1ULL;
}

/**
 * @brief Combines an accumulated digest and hit count into a verification hash.
 * @param sum Accumulated value digest across queries or iterations.
 * @param hits Total successful hits or live entries visited.
 * @return 64-bit verification checksum.
 */
[[nodiscard]] inline std::uint64_t FinalizeChecksum(
    std::uint64_t sum, std::uint64_t hits) noexcept {
  return sum ^ (hits * 0x9e3779b97f4a7c15ULL);
}

// -----------------------------------------------------------------------------
// Domain Value Structs (Realistic Envoy / Service-Mesh Production Structures)
// -----------------------------------------------------------------------------

/**
 * @struct RoutePolicy
 * @brief 24-byte HTTP/RPC routing policy returned by an edge gateway lookup.
 */
struct alignas(8) RoutePolicy {
  std::uint32_t cluster_id;
  std::uint32_t timeout_ms;
  std::uint32_t retry_budget;
  std::uint32_t rate_limit_rps;
  std::uint64_t policy_flags;
};
static_assert(sizeof(RoutePolicy) == 24, "RoutePolicy must be 24 bytes");

/**
 * @brief Computes a deterministic 64-bit digest over a `RoutePolicy` record.
 * @param policy Routing policy record.
 * @return 64-bit folded digest.
 */
[[nodiscard]] inline std::uint64_t DigestRoutePolicy(
    const RoutePolicy& policy) noexcept {
  return policy.policy_flags ^
         (static_cast<std::uint64_t>(policy.cluster_id) << 32) ^
         (static_cast<std::uint64_t>(policy.timeout_ms) << 16) ^
         static_cast<std::uint64_t>(policy.retry_budget +
                                    policy.rate_limit_rps);
}

/**
 * @struct CompactFlowStats
 * @brief 24-byte per-flow telemetry record (32B total entry with `uint64_t`
 *        key), continuing Episode 15's 32-byte memory cliff analysis.
 */
struct alignas(8) CompactFlowStats {
  std::uint64_t bytes_transferred;
  std::uint64_t last_packet_ns;
  std::uint32_t packet_count;
  std::uint32_t congestion_flags;
};
static_assert(sizeof(CompactFlowStats) == 24,
              "CompactFlowStats must be 24 bytes");
static_assert(sizeof(std::pair<const std::uint64_t, CompactFlowStats>) == 32,
              "CompactFlowStats map entry must be 32 bytes");

/**
 * @brief Computes a deterministic 64-bit digest over `CompactFlowStats`.
 * @param stats Compact flow telemetry record.
 * @return 64-bit folded digest.
 */
[[nodiscard]] inline std::uint64_t DigestCompactFlow(
    const CompactFlowStats& stats) noexcept {
  return stats.bytes_transferred ^ stats.last_packet_ns ^
         (static_cast<std::uint64_t>(stats.packet_count) << 32) ^
         static_cast<std::uint64_t>(stats.congestion_flags);
}

/**
 * @struct ConnectionMetadata
 * @brief 120-byte active TLS/HTTP2 connection record (128B entry with
 *        `uint64_t` key), modeling edge proxy connection tracking state.
 */
struct alignas(8) ConnectionMetadata {
  std::uint64_t connection_id;
  std::uint64_t peer_ip_hi;
  std::uint64_t peer_ip_lo;
  std::uint64_t bytes_rx;
  std::uint64_t bytes_tx;
  std::uint64_t requests_served;
  std::uint64_t last_activity_ns;
  std::uint32_t peer_port;
  std::uint32_t tls_cipher_suite;
  std::uint32_t circuit_breaker_state;
  std::uint32_t active_streams;
  std::array<char, 48> peer_sni;
};
static_assert(sizeof(ConnectionMetadata) == 120,
              "ConnectionMetadata must be 120 bytes");
static_assert(sizeof(std::pair<const std::uint64_t, ConnectionMetadata>) == 128,
              "ConnectionMetadata map entry must be 128 bytes");

/**
 * @brief Computes a deterministic 64-bit digest over `ConnectionMetadata`.
 * @param conn Active connection metadata record.
 * @return 64-bit folded digest.
 */
[[nodiscard]] inline std::uint64_t DigestConnection(
    const ConnectionMetadata& conn) noexcept {
  return conn.connection_id ^ conn.bytes_rx ^ conn.bytes_tx ^
         conn.requests_served ^ conn.last_activity_ns ^
         static_cast<std::uint64_t>(conn.peer_port) ^
         static_cast<std::uint64_t>(conn.tls_cipher_suite) ^
         static_cast<std::uint64_t>(
             static_cast<unsigned char>(conn.peer_sni[0]));
}

/**
 * @struct UpstreamEndpointProfile
 * @brief 248-byte service-mesh upstream endpoint profile (256B entry with
 *        `uint64_t` key), holding latency histogram buckets and locality.
 */
struct alignas(8) UpstreamEndpointProfile {
  std::uint64_t endpoint_id;
  std::uint64_t cluster_hash;
  std::uint32_t routing_weight;
  std::uint32_t health_score;
  std::uint32_t active_connections;
  std::uint32_t pending_requests;
  std::uint64_t flags;
  std::array<std::uint64_t, 16> latency_histogram_us;
  std::array<char, 32> locality_zone;
  std::array<char, 48> host_name;
};
static_assert(sizeof(UpstreamEndpointProfile) == 248,
              "UpstreamEndpointProfile must be 248 bytes");
static_assert(sizeof(std::pair<const std::uint64_t, UpstreamEndpointProfile>) ==
                  256,
              "UpstreamEndpointProfile map entry must be 256 bytes");

/**
 * @brief Computes a deterministic 64-bit digest over `UpstreamEndpointProfile`.
 * @param profile Upstream endpoint profile record.
 * @return 64-bit folded digest.
 */
[[nodiscard]] inline std::uint64_t DigestEndpointProfile(
    const UpstreamEndpointProfile& profile) noexcept {
  return profile.endpoint_id ^ profile.cluster_hash ^ profile.flags ^
         profile.latency_histogram_us[0] ^ profile.latency_histogram_us[15] ^
         static_cast<std::uint64_t>(profile.routing_weight +
                                    profile.health_score) ^
         static_cast<std::uint64_t>(
             static_cast<unsigned char>(profile.locality_zone[0]));
}

// -----------------------------------------------------------------------------
// C++20 Heterogeneous (Transparent) Hashers
// -----------------------------------------------------------------------------

/**
 * @struct TransparentStdStringHash
 * @brief Zero-dependency C++20 transparent string hasher (WG21 P0919R3/P1690R1)
 *        eliminating temporary `std::string` heap allocations during `.find()`.
 *
 * Intentionally omits `noexcept` on `operator()` so GCC libstdc++'s
 * `__cache_default` caches the 8-byte string hash inside each hash node (since
 * the ISO C++ Standard forbids `erase()` from throwing) instead of re-hashing
 * stored heap strings across single-linked-list bucket boundaries.
 */
struct TransparentStdStringHash {
  using is_transparent = void;

  /**
   * @brief Hashes a zero-copy `std::string_view` route key.
   * @param sv Zero-copy string view to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(std::string_view sv) const {
    return std::hash<std::string_view>{}(sv);
  }

  /**
   * @brief Hashes an owning `std::string` route key.
   * @param s Owning string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const std::string& s) const {
    return std::hash<std::string_view>{}(s);
  }

  /**
   * @brief Hashes an arena-backed `std::pmr::string` route key.
   * @param s PMR string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const std::pmr::string& s) const {
    return std::hash<std::string_view>{}(s);
  }

  /**
   * @brief Hashes a null-terminated C-string (`const char*`) route key.
   * @param s Null-terminated C-string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const char* s) const {
    return std::hash<std::string_view>{}(s);
  }
};

/**
 * @struct TransparentStdStringHashNoexcept
 * @brief GCC libstdc++ gotcha variant: marking a custom string hasher
 *        `noexcept` satisfies `__is_fast_hash == true &&
 *        __is_nothrow_invocable`, silently disabling 8-byte hash-code caching
 *        inside `std::unordered_map` nodes.
 */
struct TransparentStdStringHashNoexcept {
  using is_transparent = void;

  /**
   * @brief Hashes a `std::string_view` while marked `noexcept`.
   * @param sv Zero-copy string view to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }

  /**
   * @brief Hashes an owning `std::string` while marked `noexcept`.
   * @param s Owning string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }

  /**
   * @brief Hashes a null-terminated C-string (`const char*`) while `noexcept`.
   * @param s Null-terminated C-string key to hash.
   * @return 64-bit hash digest.
   */
  [[nodiscard]] std::size_t operator()(const char* s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
};

/**
 * @struct TransparentBoostStringHash
 * @brief Idiomatic transparent avalanching string hasher for Boost.Unordered
 *        (`unordered_flat_map` / `unordered_node_map`), preserving Boost's
 *        native avalanching fast path without extra post-mixing.
 */
struct TransparentBoostStringHash {
  using is_transparent = void;
  using is_avalanching = void;

  /**
   * @brief Hashes a `std::string_view` using `boost::hash`.
   * @param sv Zero-copy string view to hash.
   * @return 64-bit avalanched hash digest.
   */
  [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
    return boost::hash<std::string_view>{}(sv);
  }

  /**
   * @brief Hashes an owning `std::string` using `boost::hash`.
   * @param s Owning string key to hash.
   * @return 64-bit avalanched hash digest.
   */
  [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept {
    return boost::hash<std::string_view>{}(s);
  }

  /**
   * @brief Hashes a null-terminated C-string (`const char*`) via `boost::hash`.
   * @param s Null-terminated C-string key to hash.
   * @return 64-bit avalanched hash digest.
   */
  [[nodiscard]] std::size_t operator()(const char* s) const noexcept {
    return boost::hash<std::string_view>{}(s);
  }
};

/** @brief Default non-transparent `std::unordered_map` route table. */
using StdRouteMapDefault = std::unordered_map<std::string, RoutePolicy>;
/** @brief Zero-dependency C++20 transparent `std::unordered_map` (cached). */
using StdRouteMapTransparent =
    std::unordered_map<std::string, RoutePolicy, TransparentStdStringHash,
                       std::equal_to<>>;
/** @brief C++20 transparent `std::unordered_map` exposing GCC noexcept trap. */
using StdRouteMapTransparentNoexcept =
    std::unordered_map<std::string, RoutePolicy,
                       TransparentStdStringHashNoexcept, std::equal_to<>>;
/** @brief Contiguous monotonic-arena `std::pmr::unordered_map` route table. */
using PmrRouteMapTransparent =
    std::pmr::unordered_map<std::pmr::string, RoutePolicy,
                            TransparentStdStringHash, std::equal_to<>>;
/** @brief Production Abseil SwissTable route table (transparent by default). */
using AbslFlatRouteMap = absl::flat_hash_map<std::string, RoutePolicy>;
/** @brief Production Meta Folly F14FastMap route table (F14VectorMap). */
using FollyF14FastRouteMap = folly::F14FastMap<std::string, RoutePolicy>;
/** @brief Production Meta Folly F14ValueMap inline route table. */
using FollyF14ValueRouteMap = folly::F14ValueMap<std::string, RoutePolicy>;
/** @brief Production Boost.Unordered flat map with transparent hash. */
using BoostFlatRouteMap =
    boost::unordered_flat_map<std::string, RoutePolicy,
                              TransparentBoostStringHash, std::equal_to<>>;
/** @brief Default Boost.Unordered flat map (non-transparent opt-in demo). */
using BoostFlatRouteMapDefault =
    boost::unordered_flat_map<std::string, RoutePolicy>;
/** @brief Shared-hasher control Abseil map normalized onto `std::hash`. */
using AbslFlatRouteMapSharedHash =
    absl::flat_hash_map<std::string, RoutePolicy, TransparentStdStringHash,
                        std::equal_to<>>;
/** @brief Shared-hasher control Folly F14FastMap normalized onto `std::hash`.
 */
using FollyF14FastRouteMapSharedHash =
    folly::F14FastMap<std::string, RoutePolicy, TransparentStdStringHash,
                      std::equal_to<>>;
/** @brief Shared-hasher control Boost flat map normalized onto `std::hash`. */
using BoostFlatRouteMapSharedHash =
    boost::unordered_flat_map<std::string, RoutePolicy,
                              TransparentStdStringHash, std::equal_to<>>;

// Verify Folly F14FastMap's automatic F14VectorMap switch for entries >= 24B.
static_assert(sizeof(std::pair<const std::string, RoutePolicy>) >= 24,
              "Route entries must trigger F14FastMap -> F14VectorMap");
static_assert(sizeof(std::pair<const std::uint64_t, CompactFlowStats>) >= 24,
              "32B flow entries must trigger F14FastMap -> F14VectorMap");

// -----------------------------------------------------------------------------
// Heap Fragmentation / Process Memory / Percentile Utilities
// -----------------------------------------------------------------------------

/**
 * @class HeapChurnGuard
 * @brief Mixed-size heap arena guard simulating long-running service heap
 *        fragmentation so node-based maps do not land artificially contiguous.
 */
class HeapChurnGuard {
 public:
  /**
   * @brief Allocates variable-sized 32B..256B buffers and checkerboard-frees
   *        half of them to fragment allocator bins before map population.
   * @param chunk_count Number of noise allocations to seed.
   */
  explicit HeapChurnGuard(std::size_t chunk_count = 250'000) {
    std::uint64_t rng = kSeedHeapNoise;
    chunks_.reserve(chunk_count);
    for (std::size_t i = 0; i < chunk_count; ++i) {
      const std::size_t sz =
          32U + static_cast<std::size_t>((SplitMix64(rng) & 7ULL) * 32U);
      auto buf = std::make_unique<std::uint8_t[]>(sz);
      buf[0] = static_cast<std::uint8_t>(i & 0xFFU);
      chunks_.push_back(std::move(buf));
    }
    for (std::size_t i = 0; i < chunks_.size(); i += 2) {
      chunks_[i].reset();
    }
  }

  /**
   * @brief Releases remaining noise chunks once target containers are built.
   */
  void ReleaseRemainingNoise() { chunks_.clear(); }

 private:
  std::vector<std::unique_ptr<std::uint8_t[]>> chunks_;
};

/**
 * @brief Reads Linux process resident memory (VmRSS) and high-water mark
 *        (VmHWM) in bytes from `/proc/self/status`.
 * @param rss_bytes Out parameter receiving VmRSS in bytes.
 * @param hwm_bytes Out parameter receiving VmHWM in bytes.
 */
inline void ReadProcessMemoryBytes(std::uint64_t& rss_bytes,
                                   std::uint64_t& hwm_bytes) noexcept {
  rss_bytes = 0;
  hwm_bytes = 0;
  std::ifstream in("/proc/self/status");
  std::string line;
  while (std::getline(in, line)) {
    if (absl::StartsWith(line, "VmRSS:")) {
      rss_bytes = std::stoull(line.substr(6)) * 1024ULL;
    } else if (absl::StartsWith(line, "VmHWM:")) {
      hwm_bytes = std::stoull(line.substr(6)) * 1024ULL;
    }
  }
}

/**
 * @struct TailPercentiles
 * @brief Summary percentiles for streaming operation latency telemetry.
 */
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
 * @brief Computes exact percentiles from a mutable nanosecond sample vector.
 * @param samples Sample latencies in nanoseconds (sorted in place).
 * @param total_ms Elapsed total wall-clock milliseconds.
 * @param checksum Verification checksum preventing dead-code elimination.
 * @return Populated `TailPercentiles` structure.
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

// -----------------------------------------------------------------------------
// Story 1 Dataset: Envoy HTTP/RPC Route Table (Non-SSO Strings, Zipfian + Miss)
// -----------------------------------------------------------------------------

/**
 * @brief Formats a realistic 78-82 byte multi-tenant Envoy HTTP/RPC route key
 *        strictly exceeding GCC libstdc++'s 15-byte Short String Optimization
 *        (SSO) threshold.
 * @param index Deterministic route index.
 * @param absent True to format a guaranteed-absent unmapped 404 path.
 * @return Formatted non-SSO route path string.
 */
[[nodiscard]] inline std::string FormatRoutePath(std::size_t index,
                                                 bool absent) {
  static constexpr std::array<std::string_view, 4> kDomains = {
      "/api/v2/payments/prod-us-east-1/merchants/tenant-",
      "/api/v2/checkout/prod-eu-west-1/clusters/region-",
      "/rpc/v1/ledger/prod-us-west-2/partition/account-",
      "/edge/v3/catalog/prod-ap-east-1/service/shard-",
  };
  static constexpr std::array<std::string_view, 4> kSuffixes = {
      "/v1/transactions/authorize",
      "/v1/batch-clear/settlement",
      "/v1/metrics-agg/telemetry",
      "/v1/edge-policy/rate-quota",
  };
  char digits[7];
  std::size_t val = index;
  for (int pos = 6; pos >= 0; --pos) {
    digits[pos] = static_cast<char>('0' + (val % 10U));
    val /= 10U;
  }
  return absl::StrCat(
      absent ? "/api/v2/unmapped/prod-us-east-1/scanner/404-req-"
             : kDomains[index & 3U],
      std::string_view(digits, sizeof(digits)), kSuffixes[(index >> 2) & 3U]);
}

/**
 * @struct RouteTableDataset
 * @brief Pre-built HTTP/RPC route table dataset with contiguous socket-buffer
 *        backing storage for zero-copy `std::string_view` request slices.
 */
struct RouteTableDataset {
  std::vector<std::string> keys;
  std::vector<RoutePolicy> policies;
  std::string query_backing_zipf;
  std::vector<std::string_view> queries_zipf_70_30;
  std::string query_backing_uniform;
  std::vector<std::string_view> queries_uniform_70_30;
  std::string query_backing_miss;
  std::vector<std::string_view> queries_100_miss;
};

/**
 * @brief Generates a deterministic Envoy-style HTTP/RPC route dataset.
 * @param num_routes Number of active route entries in the table.
 * @param num_queries Number of lookup requests per workload stream.
 * @return Populated `RouteTableDataset` with zero-copy `std::string_view`
 *         slices.
 */
[[nodiscard]] inline RouteTableDataset BuildRouteTableDataset(
    std::size_t num_routes, std::size_t num_queries) {
  RouteTableDataset ds;
  ds.keys.reserve(num_routes);
  ds.policies.reserve(num_routes);

  std::uint64_t rng = kSeedRoutes;
  for (std::size_t i = 0; i < num_routes; ++i) {
    ds.keys.push_back(FormatRoutePath(i, false));
    const std::uint64_t r = SplitMix64(rng);
    ds.policies.push_back(RoutePolicy{
        static_cast<std::uint32_t>((r & 0xFFFFULL) + 1U),
        static_cast<std::uint32_t>(50U + ((r >> 16) % 2500U)),
        static_cast<std::uint32_t>((r >> 32) % 5U),
        static_cast<std::uint32_t>(100U + ((r >> 40) % 50000U)),
        r ^ 0xA5A55A5A12345678ULL,
    });
  }

  // Pre-build a fast inverse-CDF Zipfian sampler (s = 0.99) over active routes.
  std::vector<double> zipf_cdf(num_routes);
  double harmonic = 0.0;
  for (std::size_t i = 0; i < num_routes; ++i) {
    harmonic += 1.0 / std::pow(static_cast<double>(i + 1), 0.99);
    zipf_cdf[i] = harmonic;
  }
  const double inv_h = 1.0 / harmonic;
  for (double& v : zipf_cdf) {
    v *= inv_h;
  }

  // Build absent-path pool for realistic 404 negative misses.
  const std::size_t absent_pool_size = std::min<std::size_t>(num_routes, 50000);
  std::vector<std::string> absent_pool;
  absent_pool.reserve(absent_pool_size);
  for (std::size_t i = 0; i < absent_pool_size; ++i) {
    absent_pool.push_back(FormatRoutePath(i, true));
  }

  auto build_view_stream =
      [&](std::string& backing, std::vector<std::string_view>& views,
          const std::vector<std::pair<bool, std::size_t>>& picks) {
        std::size_t total_bytes = 0;
        for (const auto& [is_hit, idx] : picks) {
          total_bytes +=
              (is_hit ? ds.keys[idx].size() : absent_pool[idx].size());
        }
        backing.clear();
        backing.reserve(total_bytes);
        std::vector<std::pair<std::size_t, std::size_t>> spans;
        spans.reserve(picks.size());
        for (const auto& [is_hit, idx] : picks) {
          const std::string& src = is_hit ? ds.keys[idx] : absent_pool[idx];
          const std::size_t offset = backing.size();
          backing.append(src);
          spans.emplace_back(offset, src.size());
        }
        views.clear();
        views.reserve(spans.size());
        for (const auto& [offset, len] : spans) {
          views.emplace_back(backing.data() + offset, len);
        }
      };

  // Stream A: Zipfian s=0.99 hot skew (70% hits / 30% unmapped 404 misses).
  std::vector<std::pair<bool, std::size_t>> picks_zipf;
  picks_zipf.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    if ((r % 10ULL) < 7ULL) {
      const double u =
          static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);
      const auto it = std::lower_bound(zipf_cdf.begin(), zipf_cdf.end(), u);
      const std::size_t rank =
          it == zipf_cdf.end()
              ? (num_routes - 1)
              : static_cast<std::size_t>(it - zipf_cdf.begin());
      const std::size_t route_idx = (rank * 2654435761ULL) % num_routes;
      picks_zipf.emplace_back(true, route_idx);
    } else {
      picks_zipf.emplace_back(false,
                              static_cast<std::size_t>(r % absent_pool_size));
    }
  }
  build_view_stream(ds.query_backing_zipf, ds.queries_zipf_70_30, picks_zipf);

  // Stream B: Uniform aged-heap (70% hits / 30% misses).
  std::vector<std::pair<bool, std::size_t>> picks_unif;
  picks_unif.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    if ((r % 10ULL) < 7ULL) {
      picks_unif.emplace_back(true,
                              static_cast<std::size_t>((r >> 8) % num_routes));
    } else {
      picks_unif.emplace_back(
          false, static_cast<std::size_t>((r >> 8) % absent_pool_size));
    }
  }
  build_view_stream(ds.query_backing_uniform, ds.queries_uniform_70_30,
                    picks_unif);

  // Stream C: 100% negative misses (bot scans / unmapped endpoints).
  std::vector<std::pair<bool, std::size_t>> picks_miss;
  picks_miss.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    picks_miss.emplace_back(false,
                            static_cast<std::size_t>(r % absent_pool_size));
  }
  build_view_stream(ds.query_backing_miss, ds.queries_100_miss, picks_miss);

  return ds;
}

/**
 * @brief Generic builder populating any string-keyed route map under realistic
 *        interleaved heap fragmentation churn.
 * @tparam MapT Target associative container type.
 * @param ds Pre-built `RouteTableDataset`.
 * @return Populated aged route map.
 */
template <typename MapT>
[[nodiscard]] inline MapT BuildAgedRouteMap(const RouteTableDataset& ds) {
  HeapChurnGuard churn;
  MapT map;
  map.reserve(ds.keys.size());
  std::vector<std::unique_ptr<std::uint8_t[]>> temp_reqs;
  temp_reqs.reserve(256);
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    map.emplace(ds.keys[i], ds.policies[i]);
    if ((i & 7ULL) == 0ULL) {
      temp_reqs.push_back(std::make_unique<std::uint8_t[]>(64));
      if (temp_reqs.size() >= 128U) {
        temp_reqs.clear();
      }
    }
  }
  churn.ReleaseRemainingNoise();
  return map;
}

/**
 * @struct PmrRouteTableHolder
 * @brief Owns a monotonic PMR buffer alongside a transparent PMR
 * `unordered_map` storing both string buffers and hash nodes contiguously in
 * one arena.
 */
struct PmrRouteTableHolder {
  std::vector<std::byte> arena_storage;
  std::unique_ptr<std::pmr::monotonic_buffer_resource> arena;
  std::unique_ptr<PmrRouteMapTransparent> map;
};

/**
 * @brief Builds a zero-dependency C++17/20 `std::pmr::unordered_map` backed by
 *        a contiguous `std::pmr::monotonic_buffer_resource` arena.
 * @param ds Pre-built `RouteTableDataset`.
 * @return Owning `PmrRouteTableHolder` keeping arena alive longer than the map.
 */
[[nodiscard]] inline PmrRouteTableHolder BuildPmrRouteMap(
    const RouteTableDataset& ds) {
  PmrRouteTableHolder holder;
  // ~260 bytes per route covers bucket array + nodes + ~80B strings with
  // margin.
  holder.arena_storage.resize(ds.keys.size() * 260U + 1'048'576U);
  holder.arena = std::make_unique<std::pmr::monotonic_buffer_resource>(
      holder.arena_storage.data(), holder.arena_storage.size());
  holder.map = std::make_unique<PmrRouteMapTransparent>(holder.arena.get());
  holder.map->reserve(ds.keys.size());
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    std::pmr::string pmr_key(ds.keys[i].data(), ds.keys[i].size(),
                             holder.arena.get());
    holder.map->emplace(std::move(pmr_key), ds.policies[i]);
  }
  return holder;
}

/**
 * @brief Executes a transparent `std::string_view` lookup batch over any
 *        heterogeneous route map (used for normalized shared-hasher controls).
 * @tparam MapT Transparent route map supporting `.find(std::string_view)`.
 * @param map Populated transparent route table.
 * @param queries Contiguous span of zero-copy request route slices.
 * @return Deterministic verification checksum over route hits.
 */
template <typename MapT>
[[nodiscard]] inline std::uint64_t FindTransparentRouteBatch(
    const MapT& map, absl::Span<const std::string_view> queries) {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::string_view path : queries) {
    const auto it = map.find(path);
    if (it != map.end()) {
      sum += DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

// -----------------------------------------------------------------------------
// Story 2 Dataset & Flat Small-Map Container: Per-Request HTTP/2 Headers
// -----------------------------------------------------------------------------

/**
 * @class FlatInlinedSmallMap
 * @brief Stack-inlined flat key-value container using linear scan for tiny
 *        per-request maps (`N <= 16` stays 100% stack-resident, 0 heap allocs).
 * @tparam K Key type (e.g., `std::string_view`).
 * @tparam V Mapped value type.
 * @tparam InlineCap Inline stack capacity before spilling to heap.
 */
template <typename K, typename V, std::size_t InlineCap = 16>
class FlatInlinedSmallMap {
 public:
  using Entry = std::pair<K, V>;

  /**
   * @brief Pre-reserves storage when expected size exceeds inline capacity.
   * @param n Minimum capacity to reserve.
   */
  void Reserve(std::size_t n) { entries_.reserve(n); }

  /**
   * @brief Inserts a key-value pair if absent, updating if already present.
   * @param key Lookup key.
   * @param value Mapped value.
   */
  void Emplace(K key, V value) {
    for (auto& [k, v] : entries_) {
      if (k == key) {
        v = value;
        return;
      }
    }
    entries_.emplace_back(key, value);
  }

  /**
   * @brief Linearly scans contiguous entries for `key`.
   * @param key Lookup key.
   * @return Pointer to value if found, or `nullptr` if absent.
   */
  [[nodiscard]] const V* FindValue(K key) const noexcept {
    for (const auto& [k, v] : entries_) {
      if (k == key) {
        return &v;
      }
    }
    return nullptr;
  }

  /**
   * @brief Returns number of stored header entries.
   * @return Current element count.
   */
  [[nodiscard]] std::size_t Size() const noexcept { return entries_.size(); }

 private:
  absl::InlinedVector<Entry, InlineCap> entries_;
};

/**
 * @class StdVectorSmallMap
 * @brief Zero-dependency `std::vector`-backed flat small map for comparison.
 * @tparam K Key type.
 * @tparam V Mapped value type.
 */
template <typename K, typename V>
class StdVectorSmallMap {
 public:
  using Entry = std::pair<K, V>;

  /**
   * @brief Pre-reserves contiguous vector storage for `n` entries.
   * @param n Minimum capacity to reserve.
   */
  void Reserve(std::size_t n) { entries_.reserve(n); }

  /**
   * @brief Inserts a key-value pair if absent, updating if already present.
   * @param key Lookup key.
   * @param value Mapped value.
   */
  void Emplace(K key, V value) {
    for (auto& [k, v] : entries_) {
      if (k == key) {
        v = value;
        return;
      }
    }
    entries_.emplace_back(key, value);
  }

  /**
   * @brief Linearly scans contiguous vector entries for `key`.
   * @param key Lookup key.
   * @return Pointer to value if found, or `nullptr` if absent.
   */
  [[nodiscard]] const V* FindValue(K key) const noexcept {
    for (const auto& [k, v] : entries_) {
      if (k == key) {
        return &v;
      }
    }
    return nullptr;
  }

  /**
   * @brief Returns number of stored header entries.
   * @return Current element count.
   */
  [[nodiscard]] std::size_t Size() const noexcept { return entries_.size(); }

 private:
  std::vector<Entry> entries_;
};

/**
 * @struct HttpHeaderDataset
 * @brief Per-request HTTP/2 header dataset holding zero-copy `std::string_view`
 *        keys & values sliced from a contiguous receive buffer.
 */
struct HttpHeaderDataset {
  std::string buffer;
  std::vector<std::pair<std::string_view, std::string_view>> headers;
  std::vector<std::string_view> queries;  // 8 hits + 4 misses = 12 per request
};

/**
 * @brief Builds a realistic per-request HTTP/2 header dataset of size `n`.
 * @param num_headers Number of headers attached to the incoming HTTP request.
 * @return Populated `HttpHeaderDataset` with 12 mixed hit/miss lookup queries.
 */
[[nodiscard]] inline HttpHeaderDataset BuildHttpHeaderDataset(
    std::size_t num_headers) {
  static constexpr std::array<std::string_view, 16> kStandardHeaders = {
      ":method",
      ":path",
      ":scheme",
      ":authority",
      "user-agent",
      "accept-encoding",
      "content-type",
      "authorization",
      "x-request-id",
      "x-forwarded-for",
      "x-forwarded-proto",
      "x-envoy-upstream-rq-timeout-ms",
      "x-envoy-expected-rq-timeout-ms",
      "x-b3-traceid",
      "x-b3-spanid",
      "x-client-region",
  };
  static constexpr std::array<std::string_view, 4> kAbsentHeaders = {
      "x-envoy-force-trace",
      "x-canary-override-cluster",
      "x-debug-shadow-mirror",
      "x-internal-bypass-auth",
  };

  HttpHeaderDataset ds;
  std::vector<std::string> raw_keys;
  std::vector<std::string> raw_vals;
  raw_keys.reserve(num_headers);
  raw_vals.reserve(num_headers);
  for (std::size_t i = 0; i < num_headers; ++i) {
    if (i < kStandardHeaders.size()) {
      raw_keys.emplace_back(kStandardHeaders[i]);
    } else {
      raw_keys.push_back(absl::StrCat("x-custom-routing-attr-", i));
    }
    raw_vals.push_back(absl::StrCat("val-hdr-", (i * 131U) % 9973U));
  }

  // Pack all keys, values, and absent query keys into one contiguous buffer.
  std::vector<std::pair<std::size_t, std::size_t>> k_spans;
  std::vector<std::pair<std::size_t, std::size_t>> v_spans;
  std::vector<std::pair<std::size_t, std::size_t>> absent_spans;
  for (std::size_t i = 0; i < num_headers; ++i) {
    k_spans.emplace_back(ds.buffer.size(), raw_keys[i].size());
    ds.buffer.append(raw_keys[i]);
    v_spans.emplace_back(ds.buffer.size(), raw_vals[i].size());
    ds.buffer.append(raw_vals[i]);
  }
  for (std::string_view absent : kAbsentHeaders) {
    absent_spans.emplace_back(ds.buffer.size(), absent.size());
    ds.buffer.append(absent);
  }

  ds.headers.reserve(num_headers);
  for (std::size_t i = 0; i < num_headers; ++i) {
    ds.headers.emplace_back(
        std::string_view(ds.buffer.data() + k_spans[i].first,
                         k_spans[i].second),
        std::string_view(ds.buffer.data() + v_spans[i].first,
                         v_spans[i].second));
  }

  // 12 realistic request queries: 8 hits spread across table + 4 misses.
  ds.queries.reserve(12);
  std::uint64_t rng = kSeedHeaders ^ (num_headers * 0x9e3779b97f4a7c15ULL);
  for (std::size_t i = 0; i < 8; ++i) {
    const std::size_t idx = static_cast<std::size_t>(SplitMix64(rng)) %
                            std::max<std::size_t>(1, num_headers);
    ds.queries.push_back(ds.headers[idx].first);
  }
  for (std::size_t i = 0; i < 4; ++i) {
    ds.queries.emplace_back(ds.buffer.data() + absent_spans[i].first,
                            absent_spans[i].second);
  }
  return ds;
}

/**
 * @brief Executes one complete per-request small-map lifecycle (construct,
 *        populate `N` headers, execute 12 mixed lookups, destroy) on a hash
 *        map.
 * @tparam MapT Standard/Abseil/Folly/Boost hash map keyed by
 *         `std::string_view`.
 * @param ds Pre-built `HttpHeaderDataset`.
 * @return Deterministic verification digest over lookup hits.
 */
template <typename MapT>
[[nodiscard]] inline std::uint64_t RunSmallHashMapRequestLifecycle(
    const HttpHeaderDataset& ds) {
  MapT map;
  map.reserve(ds.headers.size());
  for (const auto& [k, v] : ds.headers) {
    map.emplace(k, v);
  }
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::string_view q : ds.queries) {
    const auto it = map.find(q);
    if (it != map.end()) {
      sum += static_cast<std::uint64_t>(it->second.size()) ^
             static_cast<std::uint64_t>(
                 static_cast<unsigned char>(it->second.back()));
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Executes one complete per-request lifecycle on a flat scan small map.
 * @tparam FlatMapT `FlatInlinedSmallMap` or `StdVectorSmallMap`.
 * @param ds Pre-built `HttpHeaderDataset`.
 * @return Deterministic verification digest matching hash-map lifecycles.
 */
template <typename FlatMapT>
[[nodiscard]] inline std::uint64_t RunSmallFlatMapRequestLifecycle(
    const HttpHeaderDataset& ds) {
  FlatMapT map;
  map.Reserve(ds.headers.size());
  for (const auto& [k, v] : ds.headers) {
    map.Emplace(k, v);
  }
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::string_view q : ds.queries) {
    if (const auto* val = map.FindValue(q)) {
      sum +=
          static_cast<std::uint64_t>(val->size()) ^
          static_cast<std::uint64_t>(static_cast<unsigned char>(val->back()));
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

// -----------------------------------------------------------------------------
// Story 3 & Story 5 Dataset: Connection / Endpoint Metadata (32B, 128B, 256B)
// -----------------------------------------------------------------------------

/**
 * @struct LargeValueDataset
 * @brief Deterministic dataset supporting Story 3 (32B/128B/256B RAM cliff &
 *        dense iteration) and Story 5 (SIMD node-stable maps).
 */
struct LargeValueDataset {
  std::vector<std::uint64_t> keys;
  std::vector<std::uint64_t> queries_80_20;
  std::vector<std::uint64_t> queries_100_miss;
};

/**
 * @brief Synthesizes a deterministic 24B `CompactFlowStats` value from `key`.
 * @param key 64-bit flow key.
 * @return Deterministic `CompactFlowStats` struct.
 */
[[nodiscard]] inline CompactFlowStats MakeCompactFlow(
    std::uint64_t key) noexcept {
  std::uint64_t s = key ^ 0x1111222233334444ULL;
  const std::uint64_t a = SplitMix64(s);
  const std::uint64_t b = SplitMix64(s);
  return CompactFlowStats{a, b, static_cast<std::uint32_t>(a & 0xFFFFFU),
                          static_cast<std::uint32_t>(b & 0xFFU)};
}

/**
 * @brief Synthesizes a deterministic 120B `ConnectionMetadata` value from
 *        `key`.
 * @param key 64-bit connection key.
 * @return Deterministic `ConnectionMetadata` struct.
 */
[[nodiscard]] inline ConnectionMetadata MakeConnectionMetadata(
    std::uint64_t key) noexcept {
  std::uint64_t s = key ^ 0x5555666677778888ULL;
  ConnectionMetadata meta{};
  meta.connection_id = key;
  meta.peer_ip_hi = SplitMix64(s);
  meta.peer_ip_lo = SplitMix64(s);
  meta.bytes_rx = SplitMix64(s) & 0xFFFFFFFFULL;
  meta.bytes_tx = SplitMix64(s) & 0xFFFFFFFFULL;
  meta.requests_served = SplitMix64(s) & 0xFFFFFULL;
  meta.last_activity_ns = SplitMix64(s);
  meta.peer_port = static_cast<std::uint32_t>(1024U + (key % 60000U));
  meta.tls_cipher_suite = static_cast<std::uint32_t>(0x1301U + (key & 3U));
  meta.circuit_breaker_state = static_cast<std::uint32_t>(key & 7U);
  meta.active_streams = static_cast<std::uint32_t>(1U + (key & 63U));
  meta.peer_sni[0] = static_cast<char>('a' + (key % 26U));
  meta.peer_sni[1] = static_cast<char>('0' + ((key >> 5) % 10U));
  return meta;
}

/**
 * @brief Synthesizes a deterministic 248B `UpstreamEndpointProfile` from `key`.
 * @param key 64-bit endpoint key.
 * @return Deterministic `UpstreamEndpointProfile` struct.
 */
[[nodiscard]] inline UpstreamEndpointProfile MakeEndpointProfile(
    std::uint64_t key) noexcept {
  std::uint64_t s = key ^ 0x9999AAAABBBBCCCCULL;
  UpstreamEndpointProfile profile{};
  profile.endpoint_id = key;
  profile.cluster_hash = SplitMix64(s);
  profile.routing_weight = static_cast<std::uint32_t>(1U + (key % 100U));
  profile.health_score = static_cast<std::uint32_t>(80U + (key % 21U));
  profile.active_connections = static_cast<std::uint32_t>(key & 1023U);
  profile.pending_requests = static_cast<std::uint32_t>((key >> 10) & 255U);
  profile.flags = SplitMix64(s);
  for (std::size_t i = 0; i < profile.latency_histogram_us.size(); ++i) {
    profile.latency_histogram_us[i] = SplitMix64(s) & 0xFFFFFULL;
  }
  profile.locality_zone[0] = static_cast<char>('a' + (key % 26U));
  profile.host_name[0] = static_cast<char>('h');
  return profile;
}

/**
 * @brief Builds a deterministic `LargeValueDataset` with `num_entries` keys and
 *        `num_queries` 80/20 hit/miss & 100% miss query streams.
 * @param num_entries Number of active connection/endpoint entries.
 * @param num_queries Number of queries per stream.
 * @return Populated `LargeValueDataset`.
 */
[[nodiscard]] inline LargeValueDataset BuildLargeValueDataset(
    std::size_t num_entries, std::size_t num_queries) {
  LargeValueDataset ds;
  ds.keys.reserve(num_entries);
  std::uint64_t rng = kSeedConns;
  for (std::size_t i = 0; i < num_entries; ++i) {
    ds.keys.push_back(MakePresentKey(rng));
  }

  ds.queries_80_20.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    if ((r % 5ULL) != 0ULL) {
      ds.queries_80_20.push_back(
          ds.keys[static_cast<std::size_t>((r >> 8) % num_entries)]);
    } else {
      ds.queries_80_20.push_back(MakeAbsentKey(rng));
    }
  }

  ds.queries_100_miss.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    ds.queries_100_miss.push_back(MakeAbsentKey(rng));
  }
  return ds;
}

/**
 * @brief Populates a `uint64_t`-keyed value map (by value) under aged heap
 *        churn.
 * @tparam MapT Target hash map storing `ValueT` inline or in nodes.
 * @tparam FactoryFn Callable mapping `uint64_t -> ValueT`.
 * @param ds Pre-built `LargeValueDataset`.
 * @param make_val Deterministic value synthesizer.
 * @return Populated aged map.
 */
template <typename MapT, typename FactoryFn>
[[nodiscard]] inline MapT BuildAgedValueMap(const LargeValueDataset& ds,
                                            FactoryFn&& make_val) {
  HeapChurnGuard churn;
  MapT map;
  map.reserve(ds.keys.size());
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    map.emplace(ds.keys[i], make_val(ds.keys[i]));
  }
  churn.ReleaseRemainingNoise();
  return map;
}

/**
 * @brief Populates an `absl::flat_hash_map<uint64_t, std::unique_ptr<ValueT>>`
 *        (Abseil's recommended hybrid for large values / value stability).
 * @tparam ValueT Target value struct type.
 * @tparam FactoryFn Callable mapping `uint64_t -> ValueT`.
 * @param ds Pre-built `LargeValueDataset`.
 * @param make_val Deterministic value synthesizer.
 * @return Populated hybrid flat-of-`unique_ptr` map.
 */
template <typename ValueT, typename FactoryFn>
[[nodiscard]] inline absl::flat_hash_map<std::uint64_t, std::unique_ptr<ValueT>>
BuildAgedUniquePtrFlatMap(const LargeValueDataset& ds, FactoryFn&& make_val) {
  HeapChurnGuard churn;
  absl::flat_hash_map<std::uint64_t, std::unique_ptr<ValueT>> map;
  map.reserve(ds.keys.size());
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    map.emplace(ds.keys[i], std::make_unique<ValueT>(make_val(ds.keys[i])));
  }
  churn.ReleaseRemainingNoise();
  return map;
}

// -----------------------------------------------------------------------------
// Story 4 Dataset: High-Occupancy Sliding-Window Eviction Churn
// -----------------------------------------------------------------------------

/**
 * @struct ChurnWorkloadDataset
 * @brief Deterministic sliding-window eviction dataset holding `live_count`
 *        initial keys, `churn_ops` replacement keys, and post-churn queries.
 */
struct ChurnWorkloadDataset {
  std::size_t live_count;
  std::size_t churn_ops;
  std::vector<std::uint64_t> key_stream;  // size = live_count + churn_ops
  std::vector<std::uint64_t> pre_churn_80_20;
  std::vector<std::uint64_t> post_churn_80_20;
  std::vector<std::uint64_t> queries_100_miss;
};

/**
 * @brief Builds a sliding-window churn dataset where a cache maintains a
 *        constant `live_count` entries while evicting oldest & inserting newest
 *        keys for `churn_ops` continuous cycles.
 * @param live_count Steady-state active key count (e.g., 780,000 ~78-82% load).
 * @param churn_ops Total sliding-window `erase + emplace` pairs to execute.
 * @param num_queries Number of queries for pre/post-churn lookup audits.
 * @return Populated `ChurnWorkloadDataset`.
 */
[[nodiscard]] inline ChurnWorkloadDataset BuildChurnDataset(
    std::size_t live_count, std::size_t churn_ops, std::size_t num_queries) {
  ChurnWorkloadDataset ds;
  ds.live_count = live_count;
  ds.churn_ops = churn_ops;
  ds.key_stream.reserve(live_count + churn_ops);

  std::uint64_t rng = kSeedChurn;
  for (std::size_t i = 0; i < live_count + churn_ops; ++i) {
    ds.key_stream.push_back(MakePresentKey(rng));
  }

  ds.pre_churn_80_20.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    if ((r % 5ULL) != 0ULL) {
      ds.pre_churn_80_20.push_back(
          ds.key_stream[static_cast<std::size_t>((r >> 8) % live_count)]);
    } else {
      ds.pre_churn_80_20.push_back(MakeAbsentKey(rng));
    }
  }

  ds.post_churn_80_20.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    if ((r % 5ULL) != 0ULL) {
      const std::size_t live_idx =
          churn_ops + static_cast<std::size_t>((r >> 8) % live_count);
      ds.post_churn_80_20.push_back(ds.key_stream[live_idx]);
    } else {
      ds.post_churn_80_20.push_back(MakeAbsentKey(rng));
    }
  }

  ds.queries_100_miss.reserve(num_queries);
  for (std::size_t i = 0; i < num_queries; ++i) {
    ds.queries_100_miss.push_back(MakeAbsentKey(rng));
  }
  return ds;
}

/**
 * @brief Executes a lookup pass over any `uint64_t -> CompactFlowStats` map.
 * @tparam MapT Hash map type mapping `uint64_t` to `CompactFlowStats`.
 * @param map Populated flow map.
 * @param queries Span of lookup keys.
 * @return Verification checksum over hits.
 */
template <typename MapT>
[[nodiscard]] inline std::uint64_t FindCompactFlowBatch(
    const MapT& map, absl::Span<const std::uint64_t> queries) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::uint64_t key : queries) {
    const auto it = map.find(key);
    if (it != map.end()) {
      sum += DigestCompactFlow(it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Executes a lookup pass over any `uint64_t -> ConnectionMetadata` map.
 * @tparam MapT Hash map type mapping `uint64_t` to `ConnectionMetadata`.
 * @param map Populated connection map.
 * @param queries Span of lookup keys.
 * @return Verification checksum over hits.
 */
template <typename MapT>
[[nodiscard]] inline std::uint64_t FindConnectionBatch(
    const MapT& map, absl::Span<const std::uint64_t> queries) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::uint64_t key : queries) {
    const auto it = map.find(key);
    if (it != map.end()) {
      sum += DigestConnection(it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Executes a lookup pass over `absl::flat_hash_map<uint64_t,
 *        std::unique_ptr<ConnectionMetadata>>`.
 * @param map Populated hybrid `unique_ptr` connection map.
 * @param queries Span of lookup keys.
 * @return Verification checksum matching by-value connection maps.
 */
[[nodiscard]] inline std::uint64_t FindConnectionUniquePtrBatch(
    const absl::flat_hash_map<std::uint64_t,
                              std::unique_ptr<ConnectionMetadata>>& map,
    absl::Span<const std::uint64_t> queries) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::uint64_t key : queries) {
    const auto it = map.find(key);
    if (it != map.end()) {
      sum += DigestConnection(*it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Executes a lookup pass over any `uint64_t -> UpstreamEndpointProfile`
 *        map.
 * @tparam MapT Hash map type mapping `uint64_t` to `UpstreamEndpointProfile`.
 * @param map Populated endpoint profile map.
 * @param queries Span of lookup keys.
 * @return Verification checksum over hits.
 */
template <typename MapT>
[[nodiscard]] inline std::uint64_t FindEndpointBatch(
    const MapT& map, absl::Span<const std::uint64_t> queries) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::uint64_t key : queries) {
    const auto it = map.find(key);
    if (it != map.end()) {
      sum += DigestEndpointProfile(it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Executes a lookup pass over `absl::flat_hash_map<uint64_t,
 *        std::unique_ptr<UpstreamEndpointProfile>>`.
 * @param map Populated hybrid `unique_ptr` endpoint map.
 * @param queries Span of lookup keys.
 * @return Verification checksum matching by-value endpoint maps.
 */
[[nodiscard]] inline std::uint64_t FindEndpointUniquePtrBatch(
    const absl::flat_hash_map<std::uint64_t,
                              std::unique_ptr<UpstreamEndpointProfile>>& map,
    absl::Span<const std::uint64_t> queries) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::uint64_t key : queries) {
    const auto it = map.find(key);
    if (it != map.end()) {
      sum += DigestEndpointProfile(*it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Full-table sweep across any `uint64_t -> UpstreamEndpointProfile` map.
 * @tparam MapT Target endpoint profile map.
 * @param map Populated endpoint map.
 * @return Verification checksum over all live entries.
 */
template <typename MapT>
[[nodiscard]] inline std::uint64_t IterEndpointMap(const MapT& map) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t count = 0;
  for (const auto& [k, v] : map) {
    sum += (k ^ DigestEndpointProfile(v));
    ++count;
  }
  return FinalizeChecksum(sum, count);
}

/**
 * @brief Full-table sweep across `absl::flat_hash_map<uint64_t,
 *        std::unique_ptr<UpstreamEndpointProfile>>`.
 * @param map Populated hybrid `unique_ptr` endpoint map.
 * @return Verification checksum matching by-value endpoint sweeps.
 */
[[nodiscard]] inline std::uint64_t IterEndpointUniquePtrMap(
    const absl::flat_hash_map<std::uint64_t,
                              std::unique_ptr<UpstreamEndpointProfile>>&
        map) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t count = 0;
  for (const auto& [k, v] : map) {
    sum += (k ^ DigestEndpointProfile(*v));
    ++count;
  }
  return FinalizeChecksum(sum, count);
}

}  // namespace hotpath::episode16

// -----------------------------------------------------------------------------
// Non-Mangled C-Linkage Kernels for Clean `llvm-objdump` & `perf annotate`
// -----------------------------------------------------------------------------
extern "C" {

/**
 * @brief Story 1 kernel: default `std::unordered_map<std::string, RoutePolicy>`
 *        constructing a temporary `std::string` per `std::string_view` lookup.
 * @param map Populated route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16StdDefaultFindRoute(
    const hotpath::episode16::StdRouteMapDefault& map,
    const std::string_view* queries, std::size_t n) {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(std::string(queries[i]));
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 kernel: C++20 transparent `std::unordered_map` (zero allocs).
 * @param map Populated transparent route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16StdTransparentFindRoute(
    const hotpath::episode16::StdRouteMapTransparent& map,
    const std::string_view* queries, std::size_t n) {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 gotcha kernel: `noexcept` transparent hasher that disables
 *        libstdc++ hash-code caching on bucket boundaries.
 * @param map Populated transparent route table with `noexcept` hasher.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16StdTransparentNoexceptFindRoute(
    const hotpath::episode16::StdRouteMapTransparentNoexcept& map,
    const std::string_view* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 kernel: C++17/20 `std::pmr::unordered_map` on monotonic arena.
 * @param map Populated PMR transparent route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16PmrTransparentFindRoute(
    const hotpath::episode16::PmrRouteMapTransparent& map,
    const std::string_view* queries, std::size_t n) {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 kernel: `absl::flat_hash_map<std::string, RoutePolicy>`
 *        (16-way SwissTable SIMD + default heterogeneous lookup).
 * @param map Populated Abseil route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16AbslFlatFindRoute(
    const hotpath::episode16::AbslFlatRouteMap& map,
    const std::string_view* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 kernel: `folly::F14FastMap<std::string, RoutePolicy>`
 *        (14-way F14 SIMD + F14VectorMap dense storage + default transparent).
 * @param map Populated Folly F14FastMap route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16FollyF14FastFindRoute(
    const hotpath::episode16::FollyF14FastRouteMap& map,
    const std::string_view* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 kernel: `folly::F14ValueMap<std::string, RoutePolicy>`
 *        (14-way F14 SIMD with inline value storage).
 * @param map Populated Folly F14ValueMap route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16FollyF14ValueFindRoute(
    const hotpath::episode16::FollyF14ValueRouteMap& map,
    const std::string_view* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Story 1 kernel: `boost::unordered_flat_map` with transparent
 *        avalanching hash (15-slot `group15` + overflow byte SIMD).
 * @param map Populated Boost flat route table.
 * @param queries Pointer to zero-copy request route slices.
 * @param n Number of lookup queries.
 * @return Verification checksum over route hits.
 */
[[gnu::noinline]] inline std::uint64_t Ep16BoostFlatFindRoute(
    const hotpath::episode16::BoostFlatRouteMap& map,
    const std::string_view* queries, std::size_t n) noexcept {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const auto it = map.find(queries[i]);
    if (it != map.end()) {
      sum += hotpath::episode16::DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return hotpath::episode16::FinalizeChecksum(sum, hits);
}

/**
 * @brief Clean single-key `uint64_t` lookup kernel for `absl::flat_hash_map`
 *        disassembly (exposes 16-byte SwissTable SIMD control-group probe).
 * @param map Populated flow map.
 * @param key 64-bit lookup key.
 * @return `bytes_transferred` if found, else 0.
 */
[[gnu::noinline]] inline std::uint64_t Ep16DisasmAbslFlatFindU64(
    const absl::flat_hash_map<std::uint64_t,
                              hotpath::episode16::CompactFlowStats>& map,
    std::uint64_t key) noexcept {
  const auto it = map.find(key);
  return it != map.end() ? it->second.bytes_transferred : 0ULL;
}

/**
 * @brief Clean single-key `uint64_t` lookup kernel for `folly::F14ValueMap`
 *        disassembly (exposes 14-slot F14 chunk SIMD probe & 14-bit mask).
 * @param map Populated flow map.
 * @param key 64-bit lookup key.
 * @return `bytes_transferred` if found, else 0.
 */
[[gnu::noinline]] inline std::uint64_t Ep16DisasmFollyF14ValueFindU64(
    const folly::F14ValueMap<std::uint64_t,
                             hotpath::episode16::CompactFlowStats>& map,
    std::uint64_t key) noexcept {
  const auto it = map.find(key);
  return it != map.end() ? it->second.bytes_transferred : 0ULL;
}

/**
 * @brief Clean single-key `uint64_t` lookup kernel for `folly::F14FastMap`
 *        disassembly (exposes 14-slot chunk probe + 32-bit dense vector index).
 * @param map Populated flow map.
 * @param key 64-bit lookup key.
 * @return `bytes_transferred` if found, else 0.
 */
[[gnu::noinline]] inline std::uint64_t Ep16DisasmFollyF14FastFindU64(
    const folly::F14FastMap<std::uint64_t,
                            hotpath::episode16::CompactFlowStats>& map,
    std::uint64_t key) noexcept {
  const auto it = map.find(key);
  return it != map.end() ? it->second.bytes_transferred : 0ULL;
}

/**
 * @brief Clean single-key `uint64_t` lookup kernel for
 *        `boost::unordered_flat_map` disassembly (exposes 15-slot `group15`
 *        SIMD probe + overflow byte check).
 * @param map Populated flow map.
 * @param key 64-bit lookup key.
 * @return `bytes_transferred` if found, else 0.
 */
[[gnu::noinline]] inline std::uint64_t Ep16DisasmBoostFlatFindU64(
    const boost::unordered_flat_map<std::uint64_t,
                                    hotpath::episode16::CompactFlowStats>& map,
    std::uint64_t key) noexcept {
  const auto it = map.find(key);
  return it != map.end() ? it->second.bytes_transferred : 0ULL;
}

/**
 * @brief Clean single-key `uint64_t` lookup kernel for `std::unordered_map`
 *        disassembly (exposes prime bucket modulo + linked-node chase).
 * @param map Populated flow map.
 * @param key 64-bit lookup key.
 * @return `bytes_transferred` if found, else 0.
 */
[[gnu::noinline]] inline std::uint64_t Ep16DisasmStdFindU64(
    const std::unordered_map<std::uint64_t,
                             hotpath::episode16::CompactFlowStats>& map,
    std::uint64_t key) noexcept {
  const auto it = map.find(key);
  return it != map.end() ? it->second.bytes_transferred : 0ULL;
}

}  // extern "C"

#endif  // HOTPATH_COMMON_CPP_HASH_MAPS_CPP_HASH_MAPS_H_
