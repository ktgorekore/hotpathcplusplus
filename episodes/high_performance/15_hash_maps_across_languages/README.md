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

# Episode 15: Hash Maps Across Languages — Why Rust's Default HashMap Beats C++ (What 'Fast' Really Means)

Welcome to **Episode 15** of **Hot Path C++** (the launch of **Phase III: User-Space Application Data Structures on Bare Metal**). This directory contains the complete reproducible multi-language benchmark, testing, live HashDoS, Intel-syntax disassembly, and unmultiplexed Linux `perf stat` telemetry suite comparing **C++20 (`std::unordered_map` vs. `absl::flat_hash_map`)**, **Rust 1.91 (`SipHash-1-3` vs. `FxHash`)**, **Go `1.22.2` vs. Go `1.24.0` (SwissTable `#54766`)**, **Java 21 LTS (`java.util.HashMap` vs. `it.unimi.dsi.fastutil`)**, and **Python 3.12.3 (`dict`)**.

---

## 1. Directory Contents

- **C++20 Suite (Bazel 9 + GoogleTest + Google Benchmark + Abseil `20260526.0`):**
  - [`../../../common/hash_maps/hash_maps_across_languages.h`](../../../common/hash_maps/hash_maps_across_languages.h) — Deterministic SplitMix64 Edge API Gateway Rate-Limiter & Session Directory dataset (`16B` packed token buckets & `24B` `SessionState`), mixed-size heap churn harness, `/proc/self/status` `VmRSS` reader, insert tail percentile tracker, `libstdc++` prime-bucket HashDoS builder, and `extern "C"` lookup kernels.
  - [`../../../common/hash_maps/hash_maps_across_languages_test.cc`](../../../common/hash_maps/hash_maps_across_languages_test.cc) — GoogleTest verification suite.
  - [`benchmark.cc`](benchmark.cc) — Google Benchmark suite across Battles 1–3.
  - [`challenge.cc`](challenge.cc) — Standalone forensic runner and `perf stat` driver (`--mode=all|b1_*|b2_rss*|b2_tail|b3_hashdos`).
  - [`live_hashdos_repro.cc`](live_hashdos_repro.cc) — Live Battle 3 HashDoS demonstration (`40,000` prime-bucket multiples).
- **Polyglot Counterparts (Identical SplitMix64 Checksums Verified):**
  - [`rust_hash_map_bench.rs`](rust_hash_map_bench.rs) — Standalone Rust `1.91.0` suite comparing stdlib `SipHash-1-3` vs. `rustc-hash` `FxHash` and demonstrating `K^-1` H1+H2 `FxHash` collisions.
  - [`go_hash_map_bench.go`](go_hash_map_bench.go) — Standalone Go suite executed under both **Go `1.22.2`** (8-slot buckets) and **Go `1.24.0`** (SwissTable split-directory runtime map).
  - [`java/com/hotpath/episode15/`](java/com/hotpath/episode15/) & [`javatests/com/hotpath/episode15/JavaHashMapTest.java`](javatests/com/hotpath/episode15/JavaHashMapTest.java) — Java 21 LTS (`HashMap<Long, Long>`, `HashMap<Long, SessionState>`, `fastutil` `Long2LongOpenHashMap` & `Long2ObjectOpenHashMap`), OpenJDK JMH `1.37`, G1GC vs. Generational ZGC tail latency, and OpenJDK `JEP 180` Bucket-0 treeification test.
  - [`python_hash_map_bench.py`](python_hash_map_bench.py) — CPython `3.12.3` compact split-table `dict` suite (`int` & `@dataclass(slots=True)`).
- **Disassembly, Godbolt & Forensic Runner:**
  - **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/hfchvGWGe`](https://godbolt.org/z/hfchvGWGe)
  - [`assembly/godbolt_source.cc`](assembly/godbolt_source.cc) & [`assembly/godbolt_comparison.s`](assembly/godbolt_comparison.s) — Annotated x86-64 Intel-syntax disassembly across Clang 18, GCC 13, Rust 1.91, Go 1.22.2, and Go 1.24.0.
  - [`run_ep15_forensics.sh`](run_ep15_forensics.sh) — End-to-end build, parity check, benchmark, JMH, and 3-pass unmultiplexed `perf stat` runner.
  - [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md) — Full verified telemetry, RSS/tail breakdowns, and PMU tables.

---

## 2. Verified Hardware Highlights (`AMD Ryzen 9 8945HX`)

- **Out-of-the-Box L1 (`1K` `80/20`):** Rust's default `SipHash-1-3` (`6.97 ns`), Java `21` `HashMap` (`5.63–8.10 ns`), and Go `1.24.0` (`8.51 ns`) all beat C++'s `std::unordered_map` (`10.43–11.08 ns`, hampered by hardware `div` + pointer chasing), while `Rust FxHash` (`3.19 ns`) and `absl::flat_hash_map` (`2.57–3.60 ns`) lead via SIMD metadata probing.
- **Aged `2M` Post-Churn (`80/20` & `100%` Miss):** `Rust FxHash` (`13.22 ns`) and `absl::flat_hash_map` (`14.54–14.92 ns`) beat `std::unordered_map` (`37.59–37.85 ns`) by **`2.5x–2.9x`**; on `100%` negative misses, `EMPTY` tag early-exit drops `absl` to **`5.93–6.45 ns`** and `Rust FxHash` to **`5.54 ns`** (even `Rust SipHash` at `11.76 ns` beats `std::unordered_map` `34.46 ns` by **`2.9x`**!).
- **Memory Footprint (`2M` Entries):** On `16B` entries, `fastutil` (`34.6 B/entry`) and `absl`/`Rust` (`35.7 B/entry`) crush boxed `Java HashMap` (`88.7 B/entry`, **`2.56x`** tax). On `32B` (`8B + 24B`), `std::unordered_map` (`108.0 MiB` = `56.6 B/entry`) actually uses **`18%` less RAM** than flat power-of-two `absl`/`Rust` (`132.0 MiB` = `69.2 B/entry`).
- **Unreserved Insert Tail Spike (`Max`):** Unreserved `std::unordered_map` spikes to **`79.91 ms`**, `Rust` to **`37.16 ms`**, and `absl` to **`26.30 ms`** (all fixed down to **`18–90 µs`** with `.reserve()`), while **Go `1.24.0`'s split-directory SwissTable keeps unreserved `Max` at `156.7 µs` (`510x` lower!)**.
- **Adversarial HashDoS (`40,000` Keys):** `std::unordered_map` collapses **`1,254x`** (`0.88 ms` $\rightarrow$ `1,101 ms`; `11.1 s` post-fragmentation in `GBench`) with `40,000/40,000` keys in `bucket(0)`, while `Rust FxHash` collapses **`1,056x`** (`735.8 ms`) under `K^-1` targeting. Default `Rust SipHash` (`0.70 ms`), `absl::flat_hash_map` (`0.43 ms`), `Go` (`0.54–0.78 ms`), and `Python` (`1.68 ms`) remain flat, and `Java 21 HashMap` degrades cleanly via `JEP 180` RB-trees (`5.70 ms`, `8.1x`).

---

## 3. Quick Start Commands

```bash
# 1. Run C++20 GoogleTest & Java 21 JUnit 5 verification tests
bazel test -c opt //common/hash_maps:hash_maps_test //episodes/high_performance/15_hash_maps_across_languages:hotpath_java_test

# 2. Run C++20 Google Benchmark & Java 21 OpenJDK JMH suites
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:benchmark
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:java_benchmarks

# 3. Run Live HashDoS Reproducer & Standalone Forensics
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:live_hashdos_repro
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:challenge

# 4. Run full 5-language telemetry, unmultiplexed perf stat, & disassembly script
./episodes/high_performance/15_hash_maps_across_languages/run_ep15_forensics.sh
```
