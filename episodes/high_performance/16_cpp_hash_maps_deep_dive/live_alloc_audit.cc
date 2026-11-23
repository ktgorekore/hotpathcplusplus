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

/**
 * @brief Executes exact `operator new` allocation counts across Story 1
 *        (`1,000,000` route lookups) and Story 2 (`16`-header HTTP/2 request
 *        lifecycles).
 * @return Process exit code (0 on success).
 */
int RunAllocAudit() {
  std::cout << "=== Story 1 Allocation Audit: 1,000,000 Route Lookups "
               "(36-42B std::string_view) ===\n";
  const RouteTableDataset ds =
      BuildRouteTableDataset(kNumRoutes, kNumRouteQueries);

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
    audit_route("std::unordered_map (Default non-transparent)", [&] {
      return Ep16StdDefaultFindRoute(m, ds.queries_zipf_70_30.data(),
                                     ds.queries_zipf_70_30.size());
    });
  }
  {
    const auto m = BuildAgedRouteMap<StdRouteMapTransparent>(ds);
    audit_route("std::unordered_map (C++20 is_transparent)", [&] {
      return Ep16StdTransparentFindRoute(m, ds.queries_zipf_70_30.data(),
                                         ds.queries_zipf_70_30.size());
    });
  }
  {
    const auto m = BuildAgedRouteMap<BoostFlatRouteMapDefault>(ds);
    audit_route("boost::unordered_flat_map (Default non-transp!)", [&] {
      std::uint64_t sum = 0;
      std::uint64_t hits = 0;
      for (std::string_view sv : ds.queries_zipf_70_30) {
        const auto it = m.find(std::string(sv));
        if (it != m.end()) {
          sum += DigestRoutePolicy(it->second);
          ++hits;
        }
      }
      return FinalizeChecksum(sum, hits);
    });
  }
  {
    const auto m = BuildAgedRouteMap<BoostFlatRouteMap>(ds);
    audit_route("boost::unordered_flat_map (Transparent hash)", [&] {
      return Ep16BoostFlatFindRoute(m, ds.queries_zipf_70_30.data(),
                                    ds.queries_zipf_70_30.size());
    });
  }
  {
    const auto m = BuildAgedRouteMap<AbslFlatRouteMap>(ds);
    audit_route("absl::flat_hash_map (Transparent by default)", [&] {
      return Ep16AbslFlatFindRoute(m, ds.queries_zipf_70_30.data(),
                                   ds.queries_zipf_70_30.size());
    });
  }
  {
    const auto m = BuildAgedRouteMap<FollyF14FastRouteMap>(ds);
    audit_route("folly::F14FastMap (Transparent by default)", [&] {
      return Ep16FollyF14FastFindRoute(m, ds.queries_zipf_70_30.data(),
                                       ds.queries_zipf_70_30.size());
    });
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
