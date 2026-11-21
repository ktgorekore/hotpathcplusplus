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
 * @file challenge.cc
 * @brief Episode 15 standalone C++20 forensic runner & perf stat driver.
 */

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "common/hash_maps/hash_maps_across_languages.h"

namespace hotpath::episode15 {
namespace {

using Clock = std::chrono::steady_clock;

template <typename T>
inline void DoNotOptimize(T& val) {
  asm volatile("" : "+r,m"(val) : : "memory");
}

inline void ClobberMemory() { asm volatile("" : : : "memory"); }

void PrintLookupRow(const std::string& label, double total_ms,
                    std::size_t total_ops, std::uint64_t checksum) {
  const double ns_per_op =
      (total_ms * 1'000'000.0) / static_cast<double>(total_ops);
  const double mops = static_cast<double>(total_ops) / (total_ms * 1'000.0);
  std::cout << std::left << std::setw(42) << label << " | " << std::right
            << std::setw(8) << std::fixed << std::setprecision(2) << ns_per_op
            << " ns/op | " << std::setw(7) << std::setprecision(2) << mops
            << " M ops/s | checksum=0x" << std::hex << checksum << std::dec
            << "\n";
}

void PrintSweepRow(const std::string& label, double total_ms,
                   std::size_t sweeps, std::size_t live_entries,
                   std::uint64_t checksum) {
  const double ms_per_sweep = total_ms / static_cast<double>(sweeps);
  const std::size_t total_live = sweeps * live_entries;
  const double ns_per_live =
      (total_ms * 1'000'000.0) / static_cast<double>(total_live);
  const double mops = static_cast<double>(total_live) / (total_ms * 1'000.0);
  std::cout << std::left << std::setw(42) << label << " | " << std::right
            << std::setw(6) << std::fixed << std::setprecision(2)
            << ms_per_sweep << " ms/swp | " << std::setw(6) << ns_per_live
            << " ns/live | " << std::setw(7) << mops << " M/s | checksum=0x"
            << std::hex << checksum << std::dec << "\n";
}

void RunL1Std(std::size_t iters) {
  const GatewayDataset ds = BuildDataset(kNumL1Entries, kNumQueries);
  const auto map = BuildAgedStdUnorderedPacked(ds, true);
  std::uint64_t sink = Ep15StdUnorderedFindPacked(map, ds.queries_80_20.data(),
                                                  ds.queries_80_20.size());
  const std::uint64_t* q_ptr = ds.queries_80_20.data();
  const std::size_t q_len = ds.queries_80_20.size();
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += Ep15StdUnorderedFindPacked(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintLookupRow("C++20 std::unordered_map (L1 1K 80/20)", ms, iters * q_len,
                 sink);
}

void RunL1Absl(std::size_t iters) {
  const GatewayDataset ds = BuildDataset(kNumL1Entries, kNumQueries);
  const auto map = BuildAgedAbslFlatPacked(ds, true);
  std::uint64_t sink = Ep15AbslFlatFindPacked(map, ds.queries_80_20.data(),
                                              ds.queries_80_20.size());
  const std::uint64_t* q_ptr = ds.queries_80_20.data();
  const std::size_t q_len = ds.queries_80_20.size();
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += Ep15AbslFlatFindPacked(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintLookupRow("C++20 absl::flat_hash_map (L1 1K 80/20)", ms, iters * q_len,
                 sink);
}

void RunAgedStd(std::size_t iters, bool miss_only) {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, kNumQueries);
  const auto map = BuildAgedStdUnorderedPacked(ds, true);
  const auto& queries = miss_only ? ds.queries_100_miss : ds.queries_80_20;
  const std::uint64_t* q_ptr = queries.data();
  const std::size_t q_len = queries.size();
  std::uint64_t sink = Ep15StdUnorderedFindPacked(map, q_ptr, q_len);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += Ep15StdUnorderedFindPacked(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintLookupRow(miss_only ? "C++20 std::unordered_map (Aged 2M Miss)"
                           : "C++20 std::unordered_map (Aged 2M 80/20)",
                 ms, iters * q_len, sink);
}

void RunAgedAbsl(std::size_t iters, bool miss_only) {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, kNumQueries);
  const auto map = BuildAgedAbslFlatPacked(ds, true);
  const auto& queries = miss_only ? ds.queries_100_miss : ds.queries_80_20;
  const std::uint64_t* q_ptr = queries.data();
  const std::size_t q_len = queries.size();
  std::uint64_t sink = Ep15AbslFlatFindPacked(map, q_ptr, q_len);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += Ep15AbslFlatFindPacked(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintLookupRow(miss_only ? "C++20 absl::flat_hash_map (Aged 2M Miss)"
                           : "C++20 absl::flat_hash_map (Aged 2M 80/20)",
                 ms, iters * q_len, sink);
}

void RunAgedSessionStd(std::size_t iters) {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, kNumQueries);
  const auto map = BuildAgedStdUnorderedSession(ds);
  const std::uint64_t* q_ptr = ds.queries_80_20.data();
  const std::size_t q_len = ds.queries_80_20.size();
  std::uint64_t sink = Ep15StdUnorderedFindSession(map, q_ptr, q_len);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += Ep15StdUnorderedFindSession(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintLookupRow("C++20 std::unordered_map (Aged 24B)", ms, iters * q_len,
                 sink);
}

void RunAgedSessionAbsl(std::size_t iters) {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, kNumQueries);
  const auto map = BuildAgedAbslFlatSession(ds);
  const std::uint64_t* q_ptr = ds.queries_80_20.data();
  const std::size_t q_len = ds.queries_80_20.size();
  std::uint64_t sink = Ep15AbslFlatFindSession(map, q_ptr, q_len);
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    DoNotOptimize(q_ptr);
    sink += Ep15AbslFlatFindSession(map, q_ptr, q_len);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintLookupRow("C++20 absl::flat_hash_map (Aged 24B)", ms, iters * q_len,
                 sink);
}

template <typename MapT, typename SweepFn>
void RunIterLoop(const std::string& label, const MapT& map, std::size_t iters,
                 SweepFn&& sweep_fn) {
  std::uint64_t sink = sweep_fn(map);
  const std::size_t live = map.size();
  const auto t0 = Clock::now();
  for (std::size_t i = 0; i < iters; ++i) {
    const auto* ptr = &map;
    DoNotOptimize(ptr);
    sink += sweep_fn(*ptr);
    DoNotOptimize(sink);
  }
  const auto t1 = Clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  PrintSweepRow(label, ms, iters, live, sink);
}

void RunIterStd(std::size_t iters, bool sparse) {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
  const auto map = sparse ? BuildAgedStdUnorderedPackedSparse(ds)
                          : BuildAgedStdUnorderedPacked(ds, true);
  RunIterLoop(sparse ? "C++20 std::unordered_map (Sparse 200K)"
                     : "C++20 std::unordered_map (Iter 2M Full)",
              map, iters, Ep15StdUnorderedIterPacked);
}

void RunIterAbsl(std::size_t iters, bool sparse) {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
  const auto map = sparse ? BuildAgedAbslFlatPackedSparse(ds)
                          : BuildAgedAbslFlatPacked(ds, true);
  RunIterLoop(sparse ? "C++20 absl::flat_hash_map (Sparse 200K)"
                     : "C++20 absl::flat_hash_map (Iter 2M Full)",
              map, iters, Ep15AbslFlatIterPacked);
}

template <typename MapT>
void MeasureMapRss(const std::string& label, const GatewayDataset& ds,
                   bool session_value, bool reserve_capacity) {
  std::uint64_t rss_before = 0;
  std::uint64_t hwm_before = 0;
  ReadProcessMemoryBytes(rss_before, hwm_before);

  MapT map;
  if (reserve_capacity) {
    map.reserve(ds.keys.size());
  }
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    if constexpr (std::is_same_v<typename MapT::mapped_type, std::uint64_t>) {
      (void)session_value;
      map.emplace(ds.keys[i], ds.packed_vals[i]);
    } else {
      (void)session_value;
      map.emplace(ds.keys[i], ds.session_vals[i]);
    }
  }
  std::size_t sz = map.size();
  DoNotOptimize(sz);

  std::uint64_t rss_after = 0;
  std::uint64_t hwm_after = 0;
  ReadProcessMemoryBytes(rss_after, hwm_after);
  const std::uint64_t delta =
      rss_after > rss_before ? (rss_after - rss_before) : 0ULL;
  const double mb = static_cast<double>(delta) / (1024.0 * 1024.0);
  const double bytes_per_entry =
      static_cast<double>(delta) / static_cast<double>(ds.keys.size());
  std::cout << std::left << std::setw(42) << label << " | RSS=" << std::right
            << std::setw(6) << std::fixed << std::setprecision(1) << mb
            << " MiB | " << std::setw(5) << std::setprecision(1)
            << bytes_per_entry << " B/entry\n";
}

void RunBattle2Rss() {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
  std::cout
      << "\n=== Battle 2A: Resident Memory Footprint (2,000,000 Entries) ===\n";
  MeasureMapRss<std::unordered_map<std::uint64_t, std::uint64_t>>(
      "C++20 std::unordered_map (16B Packed)", ds, false, true);
  MeasureMapRss<absl::flat_hash_map<std::uint64_t, std::uint64_t>>(
      "C++20 absl::flat_hash_map (16B Packed)", ds, false, true);
  MeasureMapRss<std::unordered_map<std::uint64_t, SessionState>>(
      "C++20 std::unordered_map (32B Session)", ds, true, true);
  MeasureMapRss<absl::flat_hash_map<std::uint64_t, SessionState>>(
      "C++20 absl::flat_hash_map (32B Session)", ds, true, true);
}

template <typename MapT>
void RunTailInsert(const std::string& label, const GatewayDataset& ds,
                   bool reserve_capacity) {
  std::vector<std::uint64_t> samples;
  samples.resize(ds.keys.size());

  MapT map;
  if (reserve_capacity) {
    map.reserve(ds.keys.size());
  }

  const auto start_all = Clock::now();
  for (std::size_t i = 0; i < ds.keys.size(); ++i) {
    const auto t0 = Clock::now();
    map.emplace(ds.keys[i], ds.packed_vals[i]);
    const auto t1 = Clock::now();
    samples[i] = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
  }
  const auto end_all = Clock::now();
  const double total_ms =
      std::chrono::duration<double, std::milli>(end_all - start_all).count();
  const TailPercentiles pct = ComputePercentiles(samples, total_ms, map.size());
  std::cout << std::left << std::setw(42) << label << " | total=" << std::right
            << std::setw(7) << std::fixed << std::setprecision(1)
            << pct.total_ms << " ms | p50=" << std::setw(4)
            << std::setprecision(0) << pct.p50_ns
            << " ns | p99=" << std::setw(5) << pct.p99_ns
            << " ns | p99.9=" << std::setw(6) << pct.p999_ns
            << " ns | p99.99=" << std::setw(7) << pct.p9999_ns
            << " ns | max=" << std::setw(10) << std::setprecision(0)
            << pct.max_ns << " ns\n";
}

void RunBattle2Tail() {
  const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
  std::cout << "\n=== Battle 2B: Streaming Insert Tail Latency (0 -> "
               "2,000,000) ===\n";
  RunTailInsert<std::unordered_map<std::uint64_t, std::uint64_t>>(
      "C++20 std::unordered_map (Unreserved)", ds, false);
  RunTailInsert<std::unordered_map<std::uint64_t, std::uint64_t>>(
      "C++20 std::unordered_map (Reserved)", ds, true);
  RunTailInsert<absl::flat_hash_map<std::uint64_t, std::uint64_t>>(
      "C++20 absl::flat_hash_map (Unreserved)", ds, false);
  RunTailInsert<absl::flat_hash_map<std::uint64_t, std::uint64_t>>(
      "C++20 absl::flat_hash_map (Reserved)", ds, true);
}

void RunBattle3HashDos() {
  std::uint64_t prime = 0;
  const std::vector<std::uint64_t> dos_keys =
      BuildLibstdcxxHashDosKeys(kNumHashDosKeys, prime);
  const GatewayDataset normal_ds = BuildDataset(kNumHashDosKeys, 1024);

  std::cout << "\n=== Battle 3: Adversarial HashDoS (" << kNumHashDosKeys
            << " Keys, libstdc++ prime=" << prime << ") ===\n";

  auto run_insert = [](const std::string& label, auto& map,
                       const std::vector<std::uint64_t>& keys) -> double {
    const auto t0 = Clock::now();
    for (std::size_t i = 0; i < keys.size(); ++i) {
      map.emplace(keys[i], static_cast<std::uint64_t>(i + 1));
    }
    const auto t1 = Clock::now();
    std::size_t m_sz = map.size();
    DoNotOptimize(m_sz);
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << std::left << std::setw(42) << label << " | " << std::right
              << std::setw(8) << std::fixed << std::setprecision(2) << ms
              << " ms\n";
    return ms;
  };

  std::unordered_map<std::uint64_t, std::uint64_t> std_norm;
  std_norm.reserve(kNumHashDosKeys);
  const double std_norm_ms = run_insert(
      "C++20 std::unordered_map (Normal Keys)", std_norm, normal_ds.keys);

  std::unordered_map<std::uint64_t, std::uint64_t> std_dos;
  std_dos.reserve(kNumHashDosKeys);
  const double std_dos_ms = run_insert(
      "C++20 std::unordered_map (Crafted HashDoS)", std_dos, dos_keys);
  std::cout << "   -> libstdc++ bucket_size(0) = " << std_dos.bucket_size(0)
            << " / " << std_dos.size() << " (" << std::fixed
            << std::setprecision(1) << (std_dos_ms / std_norm_ms)
            << "x slowdown)\n";

  absl::flat_hash_map<std::uint64_t, std::uint64_t> absl_norm;
  absl_norm.reserve(kNumHashDosKeys);
  run_insert("C++20 absl::flat_hash_map (Normal Keys)", absl_norm,
             normal_ds.keys);

  absl::flat_hash_map<std::uint64_t, std::uint64_t> absl_dos;
  absl_dos.reserve(kNumHashDosKeys);
  run_insert("C++20 absl::flat_hash_map (Crafted HashDoS)", absl_dos, dos_keys);
}

}  // namespace

/**
 * @brief Dispatches Episode 15 C++20 forensic challenge modes from CLI flags.
 * @param argc Argument count passed from process main().
 * @param argv Argument vector passed from process main().
 * @return Process exit code (0 on success, non-zero on unknown mode).
 */
int RunChallengeCli(int argc, char** argv) {
  std::string mode = "all";
  std::size_t iters = 0;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.rfind("--mode=", 0) == 0) {
      mode = arg.substr(7);
    } else if (arg.rfind("--iters=", 0) == 0) {
      iters = static_cast<std::size_t>(std::stoull(arg.substr(8)));
    }
  }

  if (mode == "all") {
    std::cout
        << "=== Battle 1: L1 (1,024) vs. Aged Post-Churn (2,000,000) ===\n";
    RunL1Std(iters > 0 ? iters : 100);
    RunL1Absl(iters > 0 ? iters : 100);
    RunAgedStd(iters > 0 ? iters : 10, false);
    RunAgedAbsl(iters > 0 ? iters : 10, false);
    RunAgedStd(iters > 0 ? iters : 10, true);
    RunAgedAbsl(iters > 0 ? iters : 10, true);
    RunAgedSessionStd(iters > 0 ? iters : 10);
    RunAgedSessionAbsl(iters > 0 ? iters : 10);
    RunIterStd(iters > 0 ? iters : 20, false);
    RunIterAbsl(iters > 0 ? iters : 20, false);
    RunIterStd(iters > 0 ? iters : 60, true);
    RunIterAbsl(iters > 0 ? iters : 60, true);
    RunBattle2Rss();
    RunBattle2Tail();
    RunBattle3HashDos();
  } else if (mode == "b1_l1_std") {
    RunL1Std(iters > 0 ? iters : 500);
  } else if (mode == "b1_l1_absl") {
    RunL1Absl(iters > 0 ? iters : 500);
  } else if (mode == "b1_aged_std") {
    RunAgedStd(iters > 0 ? iters : 40, false);
  } else if (mode == "b1_aged_absl") {
    RunAgedAbsl(iters > 0 ? iters : 40, false);
  } else if (mode == "b1_miss_std") {
    RunAgedStd(iters > 0 ? iters : 40, true);
  } else if (mode == "b1_miss_absl") {
    RunAgedAbsl(iters > 0 ? iters : 40, true);
  } else if (mode == "b1_aged_session_std") {
    RunAgedSessionStd(iters > 0 ? iters : 40);
  } else if (mode == "b1_aged_session_absl") {
    RunAgedSessionAbsl(iters > 0 ? iters : 40);
  } else if (mode == "b1_iter_full_std") {
    RunIterStd(iters > 0 ? iters : 40, false);
  } else if (mode == "b1_iter_full_absl") {
    RunIterAbsl(iters > 0 ? iters : 40, false);
  } else if (mode == "b1_iter_sparse_std") {
    RunIterStd(iters > 0 ? iters : 200, true);
  } else if (mode == "b1_iter_sparse_absl") {
    RunIterAbsl(iters > 0 ? iters : 200, true);
  } else if (mode == "b2_rss") {
    RunBattle2Rss();
  } else if (mode == "b2_rss_std_packed") {
    const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
    MeasureMapRss<std::unordered_map<std::uint64_t, std::uint64_t>>(
        "C++20 std::unordered_map (16B Packed)", ds, false, true);
  } else if (mode == "b2_rss_absl_packed") {
    const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
    MeasureMapRss<absl::flat_hash_map<std::uint64_t, std::uint64_t>>(
        "C++20 absl::flat_hash_map (16B Packed)", ds, false, true);
  } else if (mode == "b2_rss_std_session") {
    const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
    MeasureMapRss<std::unordered_map<std::uint64_t, SessionState>>(
        "C++20 std::unordered_map (32B Session)", ds, true, true);
  } else if (mode == "b2_rss_absl_session") {
    const GatewayDataset ds = BuildDataset(kNumAgedEntries, 1024);
    MeasureMapRss<absl::flat_hash_map<std::uint64_t, SessionState>>(
        "C++20 absl::flat_hash_map (32B Session)", ds, true, true);
  } else if (mode == "b2_tail") {
    RunBattle2Tail();
  } else if (mode == "b3_hashdos") {
    RunBattle3HashDos();
  } else {
    std::cerr << "Unknown --mode=" << mode << "\n";
    return 1;
  }
  return 0;
}

}  // namespace hotpath::episode15

int main(int argc, char** argv) {
  return hotpath::episode15::RunChallengeCli(argc, argv);
}
