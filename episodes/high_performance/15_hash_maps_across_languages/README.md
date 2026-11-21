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

# Episode 15: Why 'Fastest Hash Map' Is the Wrong Question (5 Production Workloads Compared)

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

- **Out-of-the-Box L1 (`1K` `80/20`):** Rust's default `SipHash-1-3` (`6.60 ns`, `2.87 IPC`), Java `21` `HashMap` (`5.37–7.83 ns`), and Go `1.24.0` (`8.12 ns`) all beat C++'s `std::unordered_map` (`9.79–10.62 ns`, hampered by hardware `div` + pointer chasing), while `absl::flat_hash_map` (`2.45 ns` `GBench` / `3.42 ns`) and `Rust FxHash` (`3.07 ns`) lead via SIMD metadata probing.
- **Aged `2M` Post-Churn (`80/20` & `100%` Miss):** `Rust FxHash` (`11.65 ns`, `85.84M/s`) and `absl::flat_hash_map` (`13.16 ns` `GBench` / `13.31 ns`, `75.14M/s`) beat `std::unordered_map` (`33.58–35.10 ns`, `28.49M/s`) by **`2.6x–3.0x`**; on `100%` negative misses, `EMPTY` (`0x80`) tag early-exit drops `Rust FxHash` to **`4.02 ns`** (`248.6M/s`) and `absl` to **`5.56–5.88 ns`** (`170.2M/s`; even `Rust SipHash` at `9.39 ns` beats `std::unordered_map` `30.96 ns` by **`3.3x`**!).
- **Battle 1B Full `2M` Sweep vs. `90%`-Evicted Sparse `200K` Flip:** On a full `2M`-entry sweep (`0` hashing runs), `Rust SipHash` clears in **`2.43 ms`** (`823.1M/s`, `1.51 IPC`) and `absl::flat_hash_map` in **`8.09–8.26 ms`**, while `std::unordered_map` stalls to **`64.6–73.99 ms`** (`0.13 IPC`, **`30x` slower**). After evicting `90%` overnight inside unshrunk peak `4.19M` capacity down to `200K` active keys, `std::unordered_map` speeds up **`21x`** to **`3.57 ms`** (`2.73 ms` `GBench`, `2.09B` insns) and **beats `absl::flat_hash_map`** (**`3.77 ms`**, `2.83 ms` `GBench`, `7.09B` insns scanning empty control bytes), while `Go 1.24.0` trails `Go 1.22.2` by `41%` (`6.22 ms` vs. `4.41 ms`; `57.19M` vs. `19.62M` LLC misses).
- **Memory Footprint (`2M` Entries):** On `16B` entries, `fastutil` (`34.6 B/entry`) and `absl`/`Rust` (`35.7 B/entry`) crush boxed `Java HashMap` (`88.7 B/entry`, **`2.56x`** tax). On `32B` (`8B + 24B`), `std::unordered_map` (`108.0 MiB` = `56.6 B/entry`) uses **`18%` less RAM** than flat power-of-two `absl`/`Rust` (`132.0–132.1 MiB` = `69.2 B/entry`).
- **Unreserved Insert Tail Spike (`Max`):** Unreserved `std::unordered_map` spikes to **`65.76 ms`**, `Java GenZGC` to **`47.69 ms`**, `Rust` to **`36.19 ms`**, and `absl` to **`25.00 ms`** (all fixed down to **`18.6–41.3 µs`** with `.reserve()`), while **Go `1.24.0`'s split-directory SwissTable keeps unreserved `Max` at `185.5 µs` (`354x` lower!)**.
- **Adversarial HashDoS (`40,000` Keys):** `std::unordered_map` collapses **`1,228x`** (`0.85 ms` $\rightarrow$ `1,047.92 ms`; `11,465 ms` post-fragmentation in `GBench`) with `40,000/40,000` keys in `bucket(0)`, while `Rust FxHash` collapses **`1,092x`** (`709.18 ms`) under `K^-1` targeting. Salted `absl::flat_hash_map` (`0.45 ms`), `Rust SipHash` (`0.65 ms`), `Go` (`0.75–0.77 ms`), and `Python` (`1.76 ms`) remain flat, and `Java 21 HashMap` bounds worst case cleanly via `JEP 180` RB-trees (`6.00 ms`, `5.1x`).

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
