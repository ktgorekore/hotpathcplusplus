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
 * @file benchmark.cc
 * @brief Episode 15 Google Benchmark suite comparing std::unordered_map vs.
 *        absl::flat_hash_map across L1 hot lookups, Aged 2M post-churn lookups,
 *        streaming insert growth, and adversarial HashDoS collisions.
 */

#include <benchmark/benchmark.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "common/hash_maps/hash_maps_across_languages.h"

namespace hotpath::episode15 {
namespace {

const GatewayDataset& GetL1Dataset() {
  static const GatewayDataset ds = BuildDataset(kNumL1Entries, 65'536);
  return ds;
}

const GatewayDataset& GetAgedDataset() {
  static const GatewayDataset ds = BuildDataset(kNumAgedEntries, kNumQueries);
  return ds;
}

const std::unordered_map<std::uint64_t, std::uint64_t>& GetL1StdPacked() {
  static const auto map = BuildAgedStdUnorderedPacked(GetL1Dataset(), true);
  return map;
}

const absl::flat_hash_map<std::uint64_t, std::uint64_t>& GetL1AbslPacked() {
  static const auto map = BuildAgedAbslFlatPacked(GetL1Dataset(), true);
  return map;
}

const std::unordered_map<std::uint64_t, std::uint64_t>& GetAgedStdPacked() {
  static const auto map = BuildAgedStdUnorderedPacked(GetAgedDataset(), true);
  return map;
}

const absl::flat_hash_map<std::uint64_t, std::uint64_t>& GetAgedAbslPacked() {
  static const auto map = BuildAgedAbslFlatPacked(GetAgedDataset(), true);
  return map;
}

const std::unordered_map<std::uint64_t, SessionState>& GetAgedStdSession() {
  static const auto map = BuildAgedStdUnorderedSession(GetAgedDataset());
  return map;
}

const absl::flat_hash_map<std::uint64_t, SessionState>& GetAgedAbslSession() {
  static const auto map = BuildAgedAbslFlatSession(GetAgedDataset());
  return map;
}

const std::unordered_map<std::uint64_t, std::uint64_t>& GetAgedStdSparse() {
  static const auto map = BuildAgedStdUnorderedPackedSparse(GetAgedDataset());
  return map;
}

const absl::flat_hash_map<std::uint64_t, std::uint64_t>& GetAgedAbslSparse() {
  static const auto map = BuildAgedAbslFlatPackedSparse(GetAgedDataset());
  return map;
}

void BM_Battle1_L1_StdUnorderedMap_80_20(benchmark::State& state) {
  const auto& ds = GetL1Dataset();
  const auto& map = GetL1StdPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15StdUnorderedFindPacked(map, ds.queries_80_20.data(),
                                                   ds.queries_80_20.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Battle1_L1_StdUnorderedMap_80_20);

void BM_Battle1_L1_AbslFlatHashMap_80_20(benchmark::State& state) {
  const auto& ds = GetL1Dataset();
  const auto& map = GetL1AbslPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15AbslFlatFindPacked(map, ds.queries_80_20.data(),
                                               ds.queries_80_20.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Battle1_L1_AbslFlatHashMap_80_20);

void BM_Battle1_Aged2M_StdUnorderedMap_80_20(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  const auto& map = GetAgedStdPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15StdUnorderedFindPacked(map, ds.queries_80_20.data(),
                                                   ds.queries_80_20.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Battle1_Aged2M_StdUnorderedMap_80_20);

void BM_Battle1_Aged2M_AbslFlatHashMap_80_20(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  const auto& map = GetAgedAbslPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15AbslFlatFindPacked(map, ds.queries_80_20.data(),
                                               ds.queries_80_20.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Battle1_Aged2M_AbslFlatHashMap_80_20);

void BM_Battle1_Aged2M_StdUnorderedMap_100_Miss(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  const auto& map = GetAgedStdPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15StdUnorderedFindPacked(
        map, ds.queries_100_miss.data(), ds.queries_100_miss.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Battle1_Aged2M_StdUnorderedMap_100_Miss);

void BM_Battle1_Aged2M_AbslFlatHashMap_100_Miss(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  const auto& map = GetAgedAbslPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15AbslFlatFindPacked(map, ds.queries_100_miss.data(),
                                               ds.queries_100_miss.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(
                                                   ds.queries_100_miss.size()));
}
BENCHMARK(BM_Battle1_Aged2M_AbslFlatHashMap_100_Miss);

void BM_Battle1_Aged2M_StdUnorderedMap_Session24B(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  const auto& map = GetAgedStdSession();
  for (auto _ : state) {
    std::uint64_t res = Ep15StdUnorderedFindSession(
        map, ds.queries_80_20.data(), ds.queries_80_20.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Battle1_Aged2M_StdUnorderedMap_Session24B);

void BM_Battle1_Aged2M_AbslFlatHashMap_Session24B(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  const auto& map = GetAgedAbslSession();
  for (auto _ : state) {
    std::uint64_t res = Ep15AbslFlatFindSession(map, ds.queries_80_20.data(),
                                                ds.queries_80_20.size());
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.queries_80_20.size()));
}
BENCHMARK(BM_Battle1_Aged2M_AbslFlatHashMap_Session24B);

void BM_Battle1B_IterFull2M_StdUnorderedMap(benchmark::State& state) {
  const auto& map = GetAgedStdPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15StdUnorderedIterPacked(map);
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(map.size()));
}
BENCHMARK(BM_Battle1B_IterFull2M_StdUnorderedMap)
    ->Unit(benchmark::kMillisecond);

void BM_Battle1B_IterFull2M_AbslFlatHashMap(benchmark::State& state) {
  const auto& map = GetAgedAbslPacked();
  for (auto _ : state) {
    std::uint64_t res = Ep15AbslFlatIterPacked(map);
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(map.size()));
}
BENCHMARK(BM_Battle1B_IterFull2M_AbslFlatHashMap)
    ->Unit(benchmark::kMillisecond);

void BM_Battle1B_IterSparse200K_StdUnorderedMap(benchmark::State& state) {
  const auto& map = GetAgedStdSparse();
  for (auto _ : state) {
    std::uint64_t res = Ep15StdUnorderedIterPacked(map);
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(map.size()));
}
BENCHMARK(BM_Battle1B_IterSparse200K_StdUnorderedMap)
    ->Unit(benchmark::kMillisecond);

void BM_Battle1B_IterSparse200K_AbslFlatHashMap(benchmark::State& state) {
  const auto& map = GetAgedAbslSparse();
  for (auto _ : state) {
    std::uint64_t res = Ep15AbslFlatIterPacked(map);
    benchmark::DoNotOptimize(res);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(map.size()));
}
BENCHMARK(BM_Battle1B_IterSparse200K_AbslFlatHashMap)
    ->Unit(benchmark::kMillisecond);

void BM_Battle2_Insert2M_StdUnorderedMap_Unreserved(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  for (auto _ : state) {
    std::unordered_map<std::uint64_t, std::uint64_t> m;
    for (std::size_t i = 0; i < ds.keys.size(); ++i) {
      m.emplace(ds.keys[i], ds.packed_vals[i]);
    }
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.keys.size()));
}
BENCHMARK(BM_Battle2_Insert2M_StdUnorderedMap_Unreserved)
    ->Unit(benchmark::kMillisecond);

void BM_Battle2_Insert2M_StdUnorderedMap_Reserved(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  for (auto _ : state) {
    std::unordered_map<std::uint64_t, std::uint64_t> m;
    m.reserve(ds.keys.size());
    for (std::size_t i = 0; i < ds.keys.size(); ++i) {
      m.emplace(ds.keys[i], ds.packed_vals[i]);
    }
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.keys.size()));
}
BENCHMARK(BM_Battle2_Insert2M_StdUnorderedMap_Reserved)
    ->Unit(benchmark::kMillisecond);

void BM_Battle2_Insert2M_AbslFlatHashMap_Unreserved(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  for (auto _ : state) {
    absl::flat_hash_map<std::uint64_t, std::uint64_t> m;
    for (std::size_t i = 0; i < ds.keys.size(); ++i) {
      m.emplace(ds.keys[i], ds.packed_vals[i]);
    }
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.keys.size()));
}
BENCHMARK(BM_Battle2_Insert2M_AbslFlatHashMap_Unreserved)
    ->Unit(benchmark::kMillisecond);

void BM_Battle2_Insert2M_AbslFlatHashMap_Reserved(benchmark::State& state) {
  const auto& ds = GetAgedDataset();
  for (auto _ : state) {
    absl::flat_hash_map<std::uint64_t, std::uint64_t> m;
    m.reserve(ds.keys.size());
    for (std::size_t i = 0; i < ds.keys.size(); ++i) {
      m.emplace(ds.keys[i], ds.packed_vals[i]);
    }
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(ds.keys.size()));
}
BENCHMARK(BM_Battle2_Insert2M_AbslFlatHashMap_Reserved)
    ->Unit(benchmark::kMillisecond);

void BM_Battle3_HashDos40K_StdUnorderedMap(benchmark::State& state) {
  std::uint64_t prime = 0;
  const std::vector<std::uint64_t> dos_keys =
      BuildLibstdcxxHashDosKeys(kNumHashDosKeys, prime);
  for (auto _ : state) {
    std::unordered_map<std::uint64_t, std::uint64_t> m;
    m.reserve(dos_keys.size());
    for (std::size_t i = 0; i < dos_keys.size(); ++i) {
      m.emplace(dos_keys[i], i);
    }
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(dos_keys.size()));
}
BENCHMARK(BM_Battle3_HashDos40K_StdUnorderedMap)->Unit(benchmark::kMillisecond);

void BM_Battle3_HashDos40K_AbslFlatHashMap(benchmark::State& state) {
  std::uint64_t prime = 0;
  const std::vector<std::uint64_t> dos_keys =
      BuildLibstdcxxHashDosKeys(kNumHashDosKeys, prime);
  for (auto _ : state) {
    absl::flat_hash_map<std::uint64_t, std::uint64_t> m;
    m.reserve(dos_keys.size());
    for (std::size_t i = 0; i < dos_keys.size(); ++i) {
      m.emplace(dos_keys[i], i);
    }
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(dos_keys.size()));
}
BENCHMARK(BM_Battle3_HashDos40K_AbslFlatHashMap)->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace hotpath::episode15
