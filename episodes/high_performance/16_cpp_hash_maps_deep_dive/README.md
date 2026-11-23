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

Welcome to **Episode 16** of **Hot Path C++** (continuing **Phase III: User-Space Application Data Structures on Bare Metal**). Stepping inside an **Envoy-style Edge HTTP/RPC Gateway & Connection Router**, this directory contains the complete reproducible C++20 benchmark, unit test, global `operator new` allocation audit, AddressSanitizer migration Use-After-Free reproducer, isolated `VmRSS` memory audit, Intel-syntax Zen 4 disassembly, and 3-pass unmultiplexed Linux `perf stat` telemetry suite across:

- **Zero-Dependency Standard C++20:** `std::unordered_map` (default vs. C++20 `is_transparent` `P0919R3`/`P1690R1` + GCC `libstdc++` `noexcept` hash-caching trap) and `std::pmr::unordered_map` (`monotonic_buffer_resource`)
- **Stack-Inlined Small Maps:** `FlatInlinedSmallMap<16>` (`absl::InlinedVector`) & `StdVectorSmallMap` (`std::vector`)
- **Google Abseil (`20260526.0`):** `absl::flat_hash_map`, `absl::node_hash_map`, and `absl::flat_hash_map<K, std::unique_ptr<V>>` (16-way SwissTable SIMD)
- **Meta Folly (`2026.08.31.00`):** `folly::F14FastMap` (auto-switching `F14VectorMap` at `sizeof(value_type) >= 24`), `folly::F14ValueMap`, and `folly::F14NodeMap` (14-way / 12-way F14 chunks with reference-counted overflow)
- **Boost.Unordered (`1.90.0.bcr.1`):** `boost::unordered_flat_map` and `boost::unordered_node_map` (15-way `group15` + 8-bit overflow bitmask + anti-drift cleanup)

---

## 1. Directory Contents

- **Core C++20 Workload Library & GoogleTest Suite (Bazel 9 + BCR, Google C++ Style Guide):**
  - [`../../../common/cpp_hash_maps/cpp_hash_maps.h`](../../../common/cpp_hash_maps/cpp_hash_maps.h) — Deterministic SplitMix64 route tables (`36–42B` non-SSO strings + Zipfian $s=0.99$ hot skew), per-request HTTP/2 header lifecycles (`N = 4..128`), aged-heap connection/endpoint datasets (`32B`, `128B`, `256B`), high-occupancy sliding-window eviction churn (`4,000,000` cycles), `/proc/self/status` `VmRSS` reader, and `extern "C"` lookup kernels.
  - [`../../../common/cpp_hash_maps/cpp_hash_maps_test.cc`](../../../common/cpp_hash_maps/cpp_hash_maps_test.cc) — Cross-container bit-for-bit parity & pointer-stability GoogleTest suite across all 5 stories.
- **Episode 16 Benchmarks, Forensics & Live Reproducers:**
  - [`benchmark.cc`](benchmark.cc) — Official Google Benchmark suite across Stories 1–5.
  - [`challenge.cc`](challenge.cc) — Standalone forensic runner, isolated fresh-process `VmRSS` auditor, eviction-churn tail profiler, and Linux `perf stat` driver (`--mode=all|story1|story2|story3_5|story4|s1_*|rss_*|s3_iter256_*|s4_churn_*`).
  - [`live_alloc_audit.cc`](live_alloc_audit.cc) — Isolated global `operator new` allocation counter proving `1,000,000` vs. `0` lookup allocations (Story 1) and `17` vs. `0` allocations per 16-header HTTP/2 request (Story 2).
  - [`live_uaf_migration_repro.cc`](live_uaf_migration_repro.cc) — Live pointer-stability audit across flat and node maps plus Abseil Tip #136 migration Use-After-Free reproducer (`--trigger-uaf` under `-fsanitize=address`).
- **Disassembly, Godbolt & Telemetry:**
  - **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/9zj7daGTq`](https://godbolt.org/z/9zj7daGTq)
  - [`assembly/godbolt_source.cc`](assembly/godbolt_source.cc) & [`assembly/godbolt_comparison.s`](assembly/godbolt_comparison.s) — Self-contained Clang 18 `-march=znver4` codegen plus live `llvm-objdump` kernels for all four engines.
  - [`run_ep16_forensics.sh`](run_ep16_forensics.sh) — End-to-end test, allocation audit, ASan UAF check, isolated `VmRSS`, Google Benchmark, 3-pass unmultiplexed `perf stat`, and disassembly runner.
  - [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md) — Full verified bare-metal Zen 4 telemetry, isolated RSS tables, churn percentiles, and PMU counters.

---

## 2. Verified Hardware Highlights (`AMD Ryzen 9 8945HX` Zen 4 • Idle Unmultiplexed `perf stat`)

- **Story 1 — Before Adding Dependencies (Hidden Lookup Allocs & GCC's `noexcept` Trap):** Querying a `100K`-route table of `36–42B` non-SSO strings via `std::string_view` triggers **`1,000,000` `operator new` calls (`42.43 MB`)** per million finds in *both* default `std::unordered_map` and default `boost::unordered_flat_map`. Adding C++20 `is_transparent` (`P0919R3`/`P1690R1`) drops allocations to **`0`**, cuts instructions **`-48%`**, and speeds up 404 misses **`2.19×`** (`13.02` $\rightarrow$ `28.54 M/s`)—unless the custom string hasher is marked `noexcept`, which disables GCC `libstdc++` `__cache_default` and collapses 404 misses **`2.41×`** (`11.82 M/s`). A monotonic `std::pmr::unordered_map` cuts `dTLB` misses **`-87%`** (`27.2M` $\rightarrow$ `3.49M`), while SIMD flat maps (`absl`, `folly::F14FastMap`, `boost::unordered_flat_map`) push Zipfian lookups to **`38.3–49.2 M/s`** and 404 misses to **`116.6–185.8 M/s`** (`9×–14×`).
- **Story 2 — Per-Request Small Maps (`N = 4..128` HTTP/2 Headers):** At `N = 16`, `std::unordered_map` executes **`17` heap allocations per request** (`391 ns`, `2.56 M req/s`), whereas a stack-inlined `FlatInlinedSmallMap<16>` runs in **`118 ns` (`8.49 M req/s`, `0` heap allocs — `3.3×` faster)** and beats every hash map through `N = 16`. Past `N ≈ 20–24`, SIMD flat maps (`folly::F14FastMap`, `absl::flat_hash_map`, `boost::unordered_flat_map`) cross over and lead by **`2.3×–15×`**.
- **Story 3 — Large-Value RAM Cliff & Dense Iteration (`1M × 32B / 128B / 256B`):** Just past power-of-two doubling (`1M × 256B` `UpstreamEndpointProfile`), inline flat maps balloon to **`513.9 MiB`** (`absl::flat_hash_map`, `538.9 B/row`) and **`481.9 MiB`** (`boost::unordered_flat_map`). **`folly::F14FastMap`** auto-switches to `F14VectorMap`, cutting RSS **`-51%`** to **`252.2 MiB`** (`264.5 B/row`) and streaming a 1M-entry flush in **`5.22 ms`** (`4.63M` LLC misses, **`3.8×` faster** than `absl` and **`12.9×` faster** than `std::unordered_map`'s `67.3 ms`), while inline flat maps retain a **`1.6×–1.8×`** edge on random `80/20` hits (`38.4–41.6 M/s` vs. `23.4 M/s`).
- **Story 4 — Steady-State Eviction Churn (`4,000,000` Cycles Across Both High-Fill Bands):** Sliding-window `erase + emplace` exposes the **tail-latency vs. lookup-drift triangle**: `folly::F14FastMap` / `F14ValueMap` deliver **`21–60 µs` max stall** (zero rehash pauses) but drift `3.3×–3.5×` on negative misses when packed tight at `81–85%` load; `boost::unordered_flat_map` eliminates drift (`211.5 -> 204.2 M/s`) via **`18.5–20.5 ms`** anti-drift rehashes; `absl::flat_hash_map` pauses **`5.3–6.2 ms`** on tombstone cleanups while cutting miss speed ~`2×` between cleanups.
- **Story 5 — Migration UAF & SIMD Node Maps (`1M × 128B`):** Flat maps relocate storage on rehash—turning legacy cached references (`const RoutePolicy& parent = policies.at(1); policies.emplace(child, parent);`) into zero-warning `AddressSanitizer: heap-use-after-free` crashes. When pointer stability is required, **`folly::F14NodeMap` (`37.9 M/s` / `255.3 M/s` miss)**, **`boost::unordered_node_map` (`37.4 M/s` / `318.9 M/s` miss)**, and **`absl::node_hash_map` (`29.7–44.2 M/s` / `162.9 M/s` miss)** preserve 100% stable addresses at **`2.2×–3.2×` faster `80/20` lookups and `5×–10×` faster negative misses** than `std::unordered_map` (`13.6` / `32.2 M/s`).

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
  //episodes/high_performance/16_cpp_hash_maps_deep_dive:live_uaf_migration_repro -- --trigger-uaf

# 5. Run Standalone Forensic Suite & Full Unmultiplexed perf stat Script
bazel run -c opt //episodes/high_performance/16_cpp_hash_maps_deep_dive:challenge -- --mode=all
./episodes/high_performance/16_cpp_hash_maps_deep_dive/run_ep16_forensics.sh
```
