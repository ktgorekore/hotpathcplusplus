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
 * @file live_uaf_migration_repro.cc
 * @brief Episode 16 Story 5 live pointer-stability & migration UAF reproducer.
 *
 * Models a real production migration bug (Abseil Tip of the Week #136):
 * a gateway service caches a pointer/reference to an existing tenant policy
 * (or active connection) across subsequent insertions that trigger a rehash.
 *   - Under `std::unordered_map`, `absl::node_hash_map`, `folly::F14NodeMap`,
 *     `boost::unordered_node_map`, and `absl::flat_hash_map<K, unique_ptr<V>>`,
 *     the value address remains 100% stable across rehashes.
 *   - Under `absl::flat_hash_map`, `folly::F14FastMap`, and
 *     `boost::unordered_flat_map`, the backing array reallocates and moves
 *     every element—silently turning cached pointers/references into dangling
 *     Use-After-Free hazards (immediately caught when run with `--trigger-uaf`
 *     under AddressSanitizer).
 */

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "common/cpp_hash_maps/cpp_hash_maps.h"

namespace hotpath::episode16 {
namespace {

template <typename MapT>
void AuditAddressStability(const std::string& name) {
  MapT connections;
  connections.emplace(1001ULL, MakeConnectionMetadata(1001ULL));
  const auto* addr_before = &connections.at(1001ULL);

  // Incoming traffic spike inserts 2,000 new connections, triggering rehashes.
  for (std::uint64_t id = 2000; id < 4000; ++id) {
    connections.emplace(id, MakeConnectionMetadata(id));
  }
  const auto* addr_after = &connections.at(1001ULL);
  const bool stable = (addr_before == addr_after);

  std::cout << std::left << std::setw(36) << name << " | before=" << addr_before
            << " | after=" << addr_after << " | "
            << (stable ? "STABLE (Safe)" : "RELOCATED (Dangling Ptr!)") << "\n";
}

void AuditUniquePtrHybrid() {
  absl::flat_hash_map<std::uint64_t, std::unique_ptr<ConnectionMetadata>>
      connections;
  connections.emplace(1001ULL, std::make_unique<ConnectionMetadata>(
                                   MakeConnectionMetadata(1001ULL)));
  const ConnectionMetadata* addr_before = connections.at(1001ULL).get();

  for (std::uint64_t id = 2000; id < 4000; ++id) {
    connections.emplace(
        id, std::make_unique<ConnectionMetadata>(MakeConnectionMetadata(id)));
  }
  const ConnectionMetadata* addr_after = connections.at(1001ULL).get();
  const bool stable = (addr_before == addr_after);

  std::cout << std::left << std::setw(36) << "absl::flat<K, unique_ptr<V>>"
            << " | before=" << addr_before << " | after=" << addr_after << " | "
            << (stable ? "STABLE (Value Safe)" : "RELOCATED") << "\n";
}

/**
 * @brief Demonstrates the Abseil Tip #136 migration bug: taking a reference
 *        to a parent tenant policy, then inserting child tenants (triggering
 *        rehash) and reading through the invalidated reference.
 *        ONLY invoked when explicitly passed `--trigger-uaf` (e.g., under
 *        ASan).
 */
void TriggerFlatMapMigrationUaf() {
  std::cout << "\n--- Triggering Live Migration UAF (Abseil Tip #136) ---\n";
  absl::flat_hash_map<std::uint64_t, RoutePolicy> policies;
  policies.emplace(1ULL, RoutePolicy{42U, 250U, 3U, 5000U, 0xCAFEBABEULL});

  // Valid for 10 years in std::unordered_map; compiles with ZERO warnings!
  const RoutePolicy& parent_policy = policies.at(1ULL);
  for (std::uint64_t child_id = 2; child_id <= 64; ++child_id) {
    // Rehash frees the old backing slot while `parent_policy` still points to
    // it! Under ASan this traps immediately with heap-use-after-free.
    policies.emplace(child_id, parent_policy);
  }
  std::cout << "Child 64 inherited timeout_ms=" << policies.at(64ULL).timeout_ms
            << " (expected 250; UB without ASan!)\n";
}

}  // namespace

/**
 * @brief Runs the Story 5 address-stability audit across flat and node hash
 *        maps, and optionally executes the live migration UAF reproducer.
 * @param trigger_uaf True to invoke the intentional flat-map rehash UAF (for
 *        verification under `-fsanitize=address`).
 * @return Process exit code (0 on success).
 */
int RunMigrationUafRepro(bool trigger_uaf) {
  std::cout << "=== Episode 16 Story 5: Pointer-Stability Migration Audit "
               "===\n";
  AuditAddressStability<std::unordered_map<std::uint64_t, ConnectionMetadata>>(
      "std::unordered_map");
  AuditAddressStability<absl::flat_hash_map<std::uint64_t, ConnectionMetadata>>(
      "absl::flat_hash_map");
  AuditAddressStability<folly::F14FastMap<std::uint64_t, ConnectionMetadata>>(
      "folly::F14FastMap (F14VectorMap)");
  AuditAddressStability<
      boost::unordered_flat_map<std::uint64_t, ConnectionMetadata>>(
      "boost::unordered_flat_map");
  AuditAddressStability<absl::node_hash_map<std::uint64_t, ConnectionMetadata>>(
      "absl::node_hash_map");
  AuditAddressStability<folly::F14NodeMap<std::uint64_t, ConnectionMetadata>>(
      "folly::F14NodeMap");
  AuditAddressStability<
      boost::unordered_node_map<std::uint64_t, ConnectionMetadata>>(
      "boost::unordered_node_map");
  AuditUniquePtrHybrid();

  if (trigger_uaf) {
    TriggerFlatMapMigrationUaf();
  }
  return 0;
}

}  // namespace hotpath::episode16

int main(int argc, char** argv) {
  bool trigger_uaf = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--trigger-uaf") {
      trigger_uaf = true;
    }
  }
  return hotpath::episode16::RunMigrationUafRepro(trigger_uaf);
}
