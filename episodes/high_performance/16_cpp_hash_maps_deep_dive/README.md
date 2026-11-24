<!--
  Copyright 2026 Hot Path C++ Authors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->

# Episode 16: Production C++ Hash Maps — Abseil vs. Folly F14 vs. Boost (And What Actually Breaks)

Welcome to **Episode 16** of **Hot Path Engineering** (`@HotPathEng`, continuing **Phase III: User-Space Application Data Structures on Bare Metal**). Stepping inside an **Envoy-style Edge HTTP/RPC Gateway & Connection Router**, this directory contains the complete reproducible C++20 benchmark, unit test, global `operator new` allocation audit, AddressSanitizer migration Use-After-Free reproducer, isolated `VmRSS` memory audit, Intel-syntax Zen 4 disassembly, and 3-pass unmultiplexed Linux `perf stat` telemetry suite across:

- **Zero-Dependency Standard C++20:** `std::unordered_map` (default vs. C++20 `is_transparent` `P0919R3`/`P1690R1` + GCC `libstdc++` `noexcept` hash-caching trap) and `std::pmr::unordered_map` (`monotonic_buffer_resource`)
- **Stack-Inlined Small Maps:** `FlatInlinedSmallMap<16>` (`absl::InlinedVector`) & `StdVectorSmallMap` (`std::vector`)
- **Google Abseil (`20260526.0`):** `absl::flat_hash_map`, `absl::node_hash_map`, and `absl::flat_hash_map<K, std::unique_ptr<V>>` (16-way SwissTable SIMD)
- **Meta Folly (`2026.08.31.00`):** `folly::F14FastMap` (auto-switching `F14VectorMap` at `sizeof(value_type) >= 24`), `folly::F14ValueMap`, and `folly::F14NodeMap` (14-way / 12-way F14 chunks with reference-counted overflow)
- **Boost.Unordered (`1.90.0.bcr.1`):** `boost::unordered_flat_map` and `boost::unordered_node_map` (15-way `group15` + 8-bit overflow bitmask + anti-drift cleanup)

---

## 1. Directory Contents

- **Core C++20 Workload Library & GoogleTest Suite (Bazel 9 + BCR, Google C++ Style Guide, Abseil Logging & Flags):**
  - [`../../../common/cpp_hash_maps/cpp_hash_maps.h`](../../../common/cpp_hash_maps/cpp_hash_maps.h) — Deterministic SplitMix64 route tables (`78–82B` multi-tenant non-SSO strings + Zipfian $s=0.99$ hot skew), per-request HTTP/2 header lifecycles (`N = 4..128`), aged-heap connection/endpoint datasets (`32B`, `128B`, `256B`), high-occupancy sliding-window eviction churn (`4,000,000` cycles), `/proc/self/status` `VmRSS` reader, and `extern "C"` lookup kernels.
  - [`../../../common/cpp_hash_maps/cpp_hash_maps_test.cc`](../../../common/cpp_hash_maps/cpp_hash_maps_test.cc) — Cross-container bit-for-bit parity & pointer-stability GoogleTest suite across all 5 stories.
- **Episode 16 Benchmarks, Forensics & Live Reproducers (100% Abseil `LOG(INFO)` + `ABSL_FLAG`):**
  - [`benchmark.cc`](benchmark.cc) — Official Google Benchmark suite across Stories 1–5 (including `BM_Story1_Route100Miss_StdNoexceptTrap` and normalized shared-hasher controls).
  - [`challenge.cc`](challenge.cc) — Standalone forensic runner, isolated fresh-process `VmRSS` auditor, eviction-churn tail profiler, and Linux `perf stat` driver (`--mode=all|story1|story2|story3_5|story4|s1_*|rss_*|s3_iter256_*|s4_churn_*`, `--iters=N`, `--count=N`, `--quiet`).
  - [`live_alloc_audit.cc`](live_alloc_audit.cc) — Isolated global `operator new` allocation counter proving `1,000,000` vs. `0` lookup allocations (Story 1) and `17` vs. `0` allocations per 16-header HTTP/2 request (Story 2).
  - [`live_uaf_migration_repro.cc`](live_uaf_migration_repro.cc) — Live pointer-stability audit across flat and node maps plus Abseil Tip #136 migration Use-After-Free reproducer (`--trigger_uaf` under `-fsanitize=address`).
- **Disassembly, Godbolt & Telemetry:**
  - **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/babYarMhf`](https://godbolt.org/z/babYarMhf)
  - [`assembly/godbolt_source.cc`](assembly/godbolt_source.cc) & [`assembly/godbolt_comparison.s`](assembly/godbolt_comparison.s) — Self-contained GCC 14.1 / Clang 18 `-march=znver4` codegen plus live `llvm-objdump` kernels for all four engines.
  - [`run_ep16_forensics.sh`](run_ep16_forensics.sh) — End-to-end test, allocation audit, ASan UAF check, isolated `VmRSS`, Google Benchmark, 3-pass unmultiplexed `perf stat`, and disassembly runner.
  - [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md) — Full verified bare-metal Zen 4 telemetry, isolated RSS tables, churn percentiles, and PMU counters.

---

## 2. Verified Hardware Highlights (`AMD Ryzen 9 8945HX` Zen 4 • Idle Unmultiplexed `perf stat`)

- **Story 1 — Before Adding Dependencies (Hidden Lookup Allocs, GCC's `noexcept` Trap & Shared-Hasher Control):** Querying a `100K`-route table of realistic `78–82B` multi-tenant non-SSO strings via plain `const char*` (`routes.find(path)` with zero `std::string` cast at the call site) triggers **`1,000,000` `operator new` calls (`81.67 MB`)** per million finds in default `std::unordered_map`, in half-transparent maps missing `std::equal_to<>`, and in default `boost::unordered_flat_map`. Adding C++20 `is_transparent` + `std::equal_to<>` (`P0919R3`/`P1690R1`) drops allocations to **`0`**, cuts instructions **`-41%`**, and speeds up 404 misses **`1.78×`** (`13.73` $\rightarrow$ `24.48 M/s`)—unless the custom string hasher is marked `noexcept`, which satisfies `__is_fast_hash` (already `true` for user hashers) `&& __is_nothrow_invocable` in GCC `libstdc++` `bits/hashtable.h:47-52`, drops the 8B cached node hash, forces single-linked-list bucket-boundary re-hashing of stored heap strings, and deterministically collapses 404 misses **`2.29×`** (`10.68 M/s`, ~22% slower than allocating!). With default production hashers, SIMD flat maps (`absl`, `folly::F14FastMap`, `boost::unordered_flat_map`) push Zipfian lookups to **`30.3–43.3 M/s`** and 404 misses to **`86.0–120.9 M/s`** (`6×–9×`); even when normalized onto identical `std::hash<std::string_view>` (`--mode=s1_shared_hash_control`), SIMD flat maps still beat transparent `std::unordered_map` **`1.4×–1.6×`** on Zipfian traffic and **`2.6×–3.2×`** on 404 misses purely from flat cache layout and SIMD tag probing.
- **Story 2 — Per-Request Small Maps (`N = 4..128` HTTP/2 Headers):** At `N = 16`, even with `reserve(16)`, `std::unordered_map` only pre-allocates the bucket array and still executes **`17` heap allocations per request** (`403 ns`, `2.48 M req/s`), whereas a stack-inlined `FlatInlinedSmallMap<16>` runs in **`124 ns` (`8.03 M req/s`, `0` heap allocs — `3.2×` faster)** and beats every hash map through `N = 16`. Past `N ≈ 20–24`, SIMD flat maps (`folly::F14FastMap`, `absl::flat_hash_map`, `boost::unordered_flat_map`) cross over and lead by **`2.3×–16×`**.
- **Story 3 — Large-Value RAM Cliff & Dense Iteration (`1M × 32B / 128B / 256B`):** Just past power-of-two doubling (`1M × 256B` `UpstreamEndpointProfile`), inline flat maps balloon to **`514.0 MiB`** (`absl::flat_hash_map`, `538.9 B/row` — nearly `2×` `std::unordered_map`'s `267.5 MiB`) and **`482.0 MiB`** (`boost::unordered_flat_map`). **`folly::F14FastMap`** auto-switches to `F14VectorMap`, cutting RSS **`-51%`** to **`252.3 MiB`** (`264.5 B/row`) and streaming a 1M-entry flush in **`5.13 ms`** (`4.81M` LLC misses = `-92.4%`, **`3.8×` faster** than `absl` and **`13.1×` faster** than `std::unordered_map`'s `67.3 ms`), while inline flat maps retain a **`1.24×–1.34×`** edge on random `80/20` hits (`43.4–46.9 M/s` vs. `35.0 M/s`).
- **Story 4 — Steady-State Eviction Churn (`4,000,000` Cycles Across Both High-Fill Bands):** Sliding-window `erase + emplace` exposes the **tail-latency vs. lookup-drift triangle**: `folly::F14FastMap` / `F14ValueMap` deliver **`21–28 µs` max stall** (zero rehash pauses) but drift `3.3×` on negative misses when packed tight at `81.4%` load; `boost::unordered_flat_map` eliminates drift (`268.7 -> 272.0 M/s`) via **`15.3–15.7 ms`** anti-drift rehashes; `absl::flat_hash_map` pauses **`5.1–5.8 ms`** on tombstone cleanups while cutting miss speed ~`2×` between cleanups.
- **Story 5 — 1-Line C++17 Migration UAF & SIMD Node Maps (`1M × 128B`):** Flat maps relocate storage on rehash—turning a single innocent C++17 assignment (`policies[child_id] = policies[1ULL];`, where RHS evaluates before LHS) into a zero-warning `AddressSanitizer: heap-use-after-free` crash at `live_uaf_migration_repro.cc:122`. When pointer stability is required, **`folly::F14NodeMap` (`37.1–42.4 M/s` / `257.3 M/s` miss)**, **`boost::unordered_node_map` (`39.4 M/s` / `323.0 M/s` miss)**, and **`absl::node_hash_map` (`33.3–42.3 M/s` / `181.0 M/s` miss)** preserve 100% stable addresses at **`2.0×–2.5×` faster `80/20` lookups and `5.9×–10.5×` faster negative misses** than `std::unordered_map` (`16.6` / `30.6 M/s`).

---

## 3. Quick Start Commands (Bazel 9)

```bash
# 1. Run C++20 GoogleTest cross-container parity & pointer-stability suite
bazel test -c opt //common/cpp_hash_maps:cpp_hash_maps_test

# 2. Run Episode 16 Google Benchmark suite
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:benchmark

# 3. Run Allocation Audit (Stories 1 & 2) & Pointer-Stability Reproducer (Story 5)
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_alloc_audit
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_uaf_migration_repro

# 4. Trigger live Story 5 Migration UAF under AddressSanitizer (expected ASan abort)
bazel run --copt=-fsanitize=address --linkopt=-fsanitize=address --copt=-g \
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_uaf_migration_repro -- --trigger_uaf

# 5. Run GCC noexcept Hash-Caching Demo & Normalized Shared-Hasher Control
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:challenge -- --mode=s1_noexcept_demo --iters=5
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:challenge -- --mode=s1_shared_hash_control --iters=5

# 6. Run Full Standalone Forensic Suite & Unmultiplexed perf stat Script
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:challenge -- --mode=all
./episodes/high_performance/16_cpp_hash_maps_deep_dive/run_ep16_forensics.sh
```
