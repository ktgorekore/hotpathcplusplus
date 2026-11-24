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
 * @file live_alloc_audit.cc
 * @brief Dedicated global `operator new` allocation auditor for Episode 16
 *        Stories 1 & 2 (isolated from timed benchmark binaries so timed runs
 *        use unmodified stock glibc allocation with zero instrumentation).
 *
 * Demonstrates real silent lookup allocations with ZERO explicit `std::string`
 * construction at the call site: passing a `const char*` path (e.g., from an
 * HTTP/C parser or string literal) to `.find(path)` silently triggers implicit
 * `std::string(const char*)` heap allocations in default `std::unordered_map`,
 * in half-configured transparent maps missing `std::equal_to<>`, and in
 * default `boost::unordered_flat_map`.
 */

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/flags/parse.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "common/cpp_hash_maps/cpp_hash_maps.h"

namespace {

thread_local bool g_track_allocs = false;
thread_local std::uint64_t g_alloc_count = 0;
thread_local std::uint64_t g_alloc_bytes = 0;

/**
 * @brief Allocates `size` bytes via `std::malloc`, recording counts while an
 *        `AllocScope` is active on the calling thread.
 * @param size Requested allocation size in bytes.
 * @return Non-null pointer to allocated memory (throws `std::bad_alloc` on
 * OOM).
 */
void* TrackedMalloc(std::size_t size) {
  if (g_track_allocs) {
    ++g_alloc_count;
    g_alloc_bytes += size;
  }
  void* ptr = std::malloc(size == 0 ? 1 : size);
  if (!ptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

/**
 * @brief Allocates `size` bytes aligned to `al` via `std::aligned_alloc`,
 *        recording counts while an `AllocScope` is active on the calling
 *        thread.
 * @param size Requested allocation size in bytes.
 * @param al Required alignment boundary.
 * @return Non-null aligned pointer (throws `std::bad_alloc` on OOM).
 */
void* TrackedAlignedMalloc(std::size_t size, std::align_val_t al) {
  if (g_track_allocs) {
    ++g_alloc_count;
    g_alloc_bytes += size;
  }
  const std::size_t align = static_cast<std::size_t>(al);
  const std::size_t padded = ((size + align - 1U) / align) * align;
  void* ptr = std::aligned_alloc(align, padded == 0 ? align : padded);
  if (!ptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

/**
 * @class AllocScope
 * @brief RAII scope guard enabling thread-local global `operator new` counting.
 */
class AllocScope {
 public:
  /** @brief Resets counters and begins tracking allocations on this thread. */
  AllocScope() {
    g_alloc_count = 0;
    g_alloc_bytes = 0;
    g_track_allocs = true;
  }

  /** @brief Disables allocation tracking upon scope exit. */
  ~AllocScope() { g_track_allocs = false; }

  /**
   * @brief Returns number of tracked `operator new` calls within this scope.
   * @return Allocation call count.
   */
  [[nodiscard]] std::uint64_t Count() const noexcept { return g_alloc_count; }

  /**
   * @brief Returns cumulative bytes requested within this scope.
   * @return Total allocated bytes.
   */
  [[nodiscard]] std::uint64_t Bytes() const noexcept { return g_alloc_bytes; }
};

}  // namespace

/** @brief Global `operator new` override forwarding to `TrackedMalloc`. */
void* operator new(std::size_t size) { return TrackedMalloc(size); }
/** @brief Global `operator new[]` override forwarding to `TrackedMalloc`. */
void* operator new[](std::size_t size) { return TrackedMalloc(size); }
/** @brief Aligned `operator new` override forwarding to `TrackedAlignedMalloc`.
 */
void* operator new(std::size_t size, std::align_val_t al) {
  return TrackedAlignedMalloc(size, al);
}
/** @brief Aligned `operator new[]` override forwarding to
 * `TrackedAlignedMalloc`. */
void* operator new[](std::size_t size, std::align_val_t al) {
  return TrackedAlignedMalloc(size, al);
}
/** @brief Global `operator delete` override forwarding to `std::free`. */
void operator delete(void* ptr) noexcept { std::free(ptr); }
/** @brief Global `operator delete[]` override forwarding to `std::free`. */
void operator delete[](void* ptr) noexcept { std::free(ptr); }
/** @brief Sized `operator delete` override forwarding to `std::free`. */
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
/** @brief Sized `operator delete[]` override forwarding to `std::free`. */
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }
/** @brief Aligned `operator delete` override forwarding to `std::free`. */
void operator delete(void* ptr, std::align_val_t) noexcept { std::free(ptr); }
/** @brief Aligned `operator delete[]` override forwarding to `std::free`. */
void operator delete[](void* ptr, std::align_val_t) noexcept { std::free(ptr); }
/** @brief Sized aligned `operator delete` forwarding to `std::free`. */
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept {
  std::free(ptr);
}
/** @brief Sized aligned `operator delete[]` forwarding to `std::free`. */
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept {
  std::free(ptr);
}

namespace hotpath::episode16 {

/** @brief Half-configured transparent map missing `std::equal_to<>`. */
using StdRouteMapHalfTransparent =
    std::unordered_map<std::string, RoutePolicy, TransparentStdStringHash>;

/**
 * @brief Executes exact `const char*` lookup loop across any route map with
 *        zero explicit `std::string` construction at the call site.
 * @tparam MapT Associative route map type keyed by `std::string`.
 * @param routes Populated route map.
 * @param c_paths Null-terminated C-string paths from an HTTP/C parser buffer.
 * @return Deterministic checksum verifying bit-for-bit lookup parity.
 */
template <typename MapT>
[[nodiscard]] std::uint64_t RunCstrRouteLookups(
    const MapT& routes, absl::Span<const char* const> c_paths) {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (const char* path : c_paths) {
    // Zero std::string visible here — compiles cleanly with -Wall -Wextra.
    const auto it = routes.find(path);
    if (it != routes.end()) {
      sum += DigestRoutePolicy(it->second);
      ++hits;
    }
  }
  return FinalizeChecksum(sum, hits);
}

/**
 * @brief Executes exact `operator new` allocation counts across Story 1
 *        (`1,000,000` `const char*` route lookups) and Story 2 (`16`-header
 *        HTTP/2 request lifecycles).
 * @return Process exit code (0 on success).
 */
int RunAllocAudit() {
  LOG(INFO) << "=== Story 1 Allocation Audit: 1,000,000 Route Lookups "
               "(routes.find(const char*)) ===";
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);

  // Build null-terminated C-string request buffer (as in nghttp2/C parsers).
  std::string c_backing;
  c_backing.reserve(ds.query_backing_zipf.size() +
                    ds.queries_zipf_70_30.size());
  std::vector<std::size_t> offsets;
  offsets.reserve(ds.queries_zipf_70_30.size());
  for (std::string_view sv : ds.queries_zipf_70_30) {
    offsets.push_back(c_backing.size());
    c_backing.append(sv.data(), sv.size());
    c_backing.push_back('\0');
  }
  std::vector<const char*> c_paths;
  c_paths.reserve(offsets.size());
  for (std::size_t off : offsets) {
    c_paths.push_back(c_backing.data() + off);
  }

  auto audit_route = [&](std::string_view label, auto&& fn) {
    std::uint64_t ck = 0;
    std::uint64_t alloc_count = 0;
    std::uint64_t alloc_bytes = 0;
    {
      AllocScope scope;
      ck = fn();
      alloc_count = scope.Count();
      alloc_bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat("%-48s | allocs=%10u | bytes=%11u | ck=0x%x",
                                 label, alloc_count, alloc_bytes, ck);
  };

  {
    const auto m = BuildAgedRouteMap<StdRouteMapDefault>(ds);
    audit_route("std::unordered_map (default)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }
  {
    const auto m = BuildAgedRouteMap<StdRouteMapHalfTransparent>(ds);
    audit_route("std::unordered_map (hash only, no equal_to<>)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }
  {
    const auto m = BuildAgedRouteMap<BoostFlatRouteMapDefault>(ds);
    audit_route("boost::unordered_flat_map (default!)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }
  {
    const auto m = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
    audit_route("std::unordered_map (is_transparent + equal_to<>)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }
  {
    const auto m = BuildAgedRouteMap<BoostFlatRouteMap>(ds);
    audit_route("boost::unordered_flat_map (transparent)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }
  {
    const auto m = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
    audit_route("absl::flat_hash_map (transparent by default)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }
  {
    const auto m = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
    audit_route("folly::F14FastMap (transparent by default)",
                [&] { return RunCstrRouteLookups(m, c_paths); });
  }

  LOG(INFO) << "=== Story 2 Allocation Audit: Per-Request Small Header Map "
               "(16 Headers, 1 Request) ===";
  const HttpHeaderDataset hdr16 = BuildHttpHeaderDataset(16);
  auto audit_small = [&](std::string_view label, auto&& fn) {
    std::uint64_t ck = 0;
    std::uint64_t alloc_count = 0;
    std::uint64_t alloc_bytes = 0;
    {
      AllocScope scope;
      ck = fn();
      alloc_count = scope.Count();
      alloc_bytes = scope.Bytes();
    }
    LOG(INFO) << absl::StrFormat("%-48s | allocs=%4u | bytes=%6u | ck=0x%x",
                                 label, alloc_count, alloc_bytes, ck);
  };

  audit_small("FlatInlinedSmallMap<16> (Stack Linear Scan)", [&] {
    return RunSmallFlatMapRequestLifecycle<
        FlatInlinedSmallMap<std::string_view, std::string_view, 16>>(hdr16);
  });
  audit_small("StdVectorSmallMap (std::vector reserve)", [&] {
    return RunSmallFlatMapRequestLifecycle<
        StdVectorSmallMap<std::string_view, std::string_view>>(hdr16);
  });
  audit_small("std::unordered_map (reserve(16) + 16 nodes)", [&] {
    return RunSmallHashMapRequestLifecycle<
        std::unordered_map<std::string_view, std::string_view>>(hdr16);
  });
  audit_small("absl::flat_hash_map (reserve(16))", [&] {
    return RunSmallHashMapRequestLifecycle<
        absl::flat_hash_map<std::string_view, std::string_view>>(hdr16);
  });
  audit_small("folly::F14FastMap (reserve(16))", [&] {
    return RunSmallHashMapRequestLifecycle<
        folly::F14FastMap<std::string_view, std::string_view>>(hdr16);
  });
  audit_small("boost::unordered_flat_map (reserve(16))", [&] {
    return RunSmallHashMapRequestLifecycle<
        boost::unordered_flat_map<std::string_view, std::string_view>>(hdr16);
  });
  return 0;
}

}  // namespace hotpath::episode16

/**
 * @brief Entry point initializing Abseil flags and logging before auditing
 *        `operator new` allocation counts across Stories 1 & 2.
 * @param argc Command-line argument count.
 * @param argv Command-line argument vector.
 * @return Process exit code (0 on success).
 */
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  return hotpath::episode16::RunAllocAudit();
}
