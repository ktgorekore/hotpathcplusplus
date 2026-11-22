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
 * @file live_hashdos_repro.cc
 * @brief Episode 15 Battle 3 standalone live HashDoS reproducer.
 *
 * Demonstrates how GCC libstdc++'s identity std::hash<uint64_t> maps
 * crafted prime-bucket multiples into bucket 0, collapsing std::unordered_map
 * into an O(N^2) linked-list scan while absl::flat_hash_map hashes via
 * randomized per-process salt + SwissTable probing.
 */

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "common/hash_maps/hash_maps_across_languages.h"

namespace hotpath::episode15 {

/**
 * @brief Executes the live HashDoS collision comparison between
 *        `std::unordered_map` and `absl::flat_hash_map`.
 * @return Process exit code (0 on success).
 */
int RunHashDosRepro() {
  using Clock = std::chrono::steady_clock;

  std::uint64_t prime = 0;
  const std::vector<std::uint64_t> crafted =
      BuildLibstdcxxHashDosKeys(kNumHashDosKeys, prime);
  const GatewayDataset normal = BuildDataset(kNumHashDosKeys, 1024);

  std::cout << "=== Episode 15 Live HashDoS Reproducer (" << kNumHashDosKeys
            << " Keys) ===\n";
  std::cout << "Discovered libstdc++ prime bucket_count = " << prime << "\n\n";

  std::unordered_map<std::uint64_t, std::uint64_t> std_normal;
  std_normal.reserve(kNumHashDosKeys);
  auto t0 = Clock::now();
  for (std::size_t i = 0; i < normal.keys.size(); ++i) {
    std_normal.emplace(normal.keys[i], i + 1);
  }
  auto t1 = Clock::now();
  const double std_normal_ms =
      std::chrono::duration<double, std::milli>(t1 - t0).count();

  std::unordered_map<std::uint64_t, std::uint64_t> std_dos;
  std_dos.reserve(kNumHashDosKeys);
  t0 = Clock::now();
  for (std::size_t i = 0; i < crafted.size(); ++i) {
    std_dos.emplace(crafted[i], i + 1);
  }
  t1 = Clock::now();
  const double std_dos_ms =
      std::chrono::duration<double, std::milli>(t1 - t0).count();

  absl::flat_hash_map<std::uint64_t, std::uint64_t> absl_dos;
  absl_dos.reserve(kNumHashDosKeys);
  t0 = Clock::now();
  for (std::size_t i = 0; i < crafted.size(); ++i) {
    absl_dos.emplace(crafted[i], i + 1);
  }
  t1 = Clock::now();
  const double absl_dos_ms =
      std::chrono::duration<double, std::milli>(t1 - t0).count();

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "1. std::unordered_map (normal traffic) : " << std_normal_ms
            << " ms (max bucket=" << std_normal.bucket_size(0) << ")\n";
  std::cout << "2. std::unordered_map (HashDoS attack) : " << std_dos_ms
            << " ms (bucket 0=" << std_dos.bucket_size(0) << " / "
            << std_dos.size() << " -> " << std::setprecision(1)
            << (std_dos_ms / std_normal_ms) << "x slower!)\n";
  std::cout << std::setprecision(2);
  std::cout << "3. absl::flat_hash_map (HashDoS attack): " << absl_dos_ms
            << " ms (salted SwissTable shrugs off attack)\n";
  return 0;
}

}  // namespace hotpath::episode15

int main() { return hotpath::episode15::RunHashDosRepro(); }
