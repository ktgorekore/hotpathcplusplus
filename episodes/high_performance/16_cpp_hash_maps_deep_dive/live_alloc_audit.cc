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
#include <iomanip>
#include <iostream>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/cpp_hash_maps/cpp_hash_maps.h"

namespace {

thread_local bool g_track_allocs = false;
thread_local std::uint64_t g_alloc_count = 0;
thread_local std::uint64_t g_alloc_bytes = 0;

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

struct AllocScope {
  AllocScope() {
    g_alloc_count = 0;
    g_alloc_bytes = 0;
    g_track_allocs = true;
  }
  ~AllocScope() { g_track_allocs = false; }
  std::uint64_t count() const { return g_alloc_count; }
  std::uint64_t bytes() const { return g_alloc_bytes; }
};

}  // namespace

void* operator new(std::size_t size) { return TrackedMalloc(size); }
void* operator new[](std::size_t size) { return TrackedMalloc(size); }
void* operator new(std::size_t size, std::align_val_t al) {
  return TrackedAlignedMalloc(size, al);
}
void* operator new[](std::size_t size, std::align_val_t al) {
  return TrackedAlignedMalloc(size, al);
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::align_val_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept {
  std::free(ptr);
}
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept {
  std::free(ptr);
}

namespace hotpath::episode16 {

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
    const MapT& routes, const std::vector<const char*>& c_paths) {
  std::uint64_t sum = 0;
  std::uint64_t hits = 0;
  for (const char* path : c_paths) {
    // Zero std::string visible here — compiles cleanly with -Wall -Wextra!
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
  std::cout << "=== Story 1 Allocation Audit: 1,000,000 Route Lookups "
               "(routes.find(const char*)) ===\n";
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

  auto audit_route = [&](const std::string& label, auto&& fn) {
    AllocScope scope;
    const std::uint64_t ck = fn();
    std::cout << std::left << std::setw(48) << label
              << " | allocs=" << std::right << std::setw(10) << scope.count()
              << " | bytes=" << std::setw(11) << scope.bytes() << " | ck=0x"
              << std::hex << ck << std::dec << "\n";
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

  std::cout << "\n=== Story 2 Allocation Audit: Per-Request Small Header Map "
               "(16 Headers, 1 Request) ===\n";
  const HttpHeaderDataset hdr16 = BuildHttpHeaderDataset(16);
  auto audit_small = [&](const std::string& label, auto&& fn) {
    AllocScope scope;
    const std::uint64_t ck = fn();
    std::cout << std::left << std::setw(48) << label
              << " | allocs=" << std::right << std::setw(4) << scope.count()
              << " | bytes=" << std::setw(6) << scope.bytes() << " | ck=0x"
              << std::hex << ck << std::dec << "\n";
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

int main() { return hotpath::episode16::RunAllocAudit(); }
