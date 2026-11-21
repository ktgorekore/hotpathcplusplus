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

# Episode 15: C++ vs. Rust vs. Java Hash Maps — How Memory Layout Decides Everything

Welcome to **Episode 15** of **Hot Path C++** (the launch of **Phase III: User-Space Application Data Structures on Bare Metal**). This directory contains the complete reproducible benchmark, unit test, live HashDoS, Intel-syntax disassembly, and unmultiplexed Linux `perf stat` telemetry suite focused deeply on **three systems ecosystems**:

- **C++20:** `std::unordered_map` (pointer-stable linked heap nodes, GCC prime buckets) vs. `absl::flat_hash_map` (SIMD SwissTable open addressing, per-process random salt)
- **Rust `1.91.0`:** `std::collections::HashMap` (`hashbrown` SwissTable + keyed `SipHash-1-3` default) vs. `rustc-hash` `FxHash` (unkeyed internal fast path)
- **Java 21 LTS:** `java.util.HashMap` (boxed references + OpenJDK `JEP 180` Red-Black trees, `G1GC` & `Generational ZGC`) vs. `it.unimi.dsi.fastutil` (`Long2LongOpenHashMap` / `Long2ObjectOpenHashMap` unboxed primitive arrays)

*(Additional Go `1.22.2`/`1.24.0` and CPython `3.12.3` harnesses are preserved in this directory and `BENCHMARK_REPORT.md` as optional lab baselines.)*

---

## 1. Directory Contents

- **C++20 Suite (Bazel 9 + GoogleTest + Google Benchmark + Abseil `20260526.0`, Google C++ Style Guide):**
  - [`../../../common/hash_maps/hash_maps_across_languages.h`](../../../common/hash_maps/hash_maps_across_languages.h) — Deterministic SplitMix64 ephemeral edge rate-limiter & metrics dataset (`16B` packed entries & `24B` structured bucket state), `300K`-chunk aged-heap pre-conditioning, `/proc/self/status` `VmRSS` reader, insert tail percentile tracker, GCC prime-bucket HashDoS builder, and `extern "C"` lookup kernels.
  - [`../../../common/hash_maps/hash_maps_across_languages_test.cc`](../../../common/hash_maps/hash_maps_across_languages_test.cc) — GoogleTest verification suite.
  - [`benchmark.cc`](benchmark.cc) — Google Benchmark suite across Battles 1–3.
  - [`challenge.cc`](challenge.cc) — Standalone forensic runner and `perf stat` driver (`--mode=all|b1_*|b2_rss*|b2_tail|b3_hashdos`).
  - [`live_hashdos_repro.cc`](live_hashdos_repro.cc) — Live Battle 3 HashDoS demonstration (`40,000` prime-bucket multiples).
- **Rust & Java Counterparts (Bit-for-Bit Parity Checksum `0x88ced7ad3465eeb6`):**
  - [`rust_hash_map_bench.rs`](rust_hash_map_bench.rs) — Standalone Rust `1.91.0` suite comparing stdlib `SipHash-1-3` vs. `FxHash` and demonstrating `K^-1` H1+H2 `FxHash` collisions.
  - [`java/com/hotpath/episode15/`](java/com/hotpath/episode15/) & [`javatests/com/hotpath/episode15/JavaHashMapTest.java`](javatests/com/hotpath/episode15/JavaHashMapTest.java) — Java 21 LTS (`HashMap<Long, Long>`, `HashMap<Long, SessionState>`, `fastutil` `Long2LongOpenHashMap` & `Long2ObjectOpenHashMap`), OpenJDK JMH `1.37`, G1GC vs. Generational ZGC tail latency, and OpenJDK `JEP 180` Bucket-0 treeification test.
- **Disassembly, Godbolt & Forensic Runner:**
  - **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/hfchvGWGe`](https://godbolt.org/z/hfchvGWGe)
  - [`assembly/godbolt_source.cc`](assembly/godbolt_source.cc) & [`assembly/godbolt_comparison.s`](assembly/godbolt_comparison.s) — Annotated x86-64 Intel-syntax disassembly.
  - [`run_ep15_forensics.sh`](run_ep15_forensics.sh) — End-to-end build, parity check, benchmark, JMH, and 3-pass unmultiplexed `perf stat` runner.
  - [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md) — Full verified bare-metal telemetry, RSS/tail breakdowns, and PMU tables.

---

## 2. Verified Hardware Highlights (`AMD Ryzen 9 8945HX` Zen 4 • Idle Unmultiplexed `perf stat`)

- **Story 1 — Aged `2M` Edge Rate-Limiter Lookups & Bot Misses:** `Rust FxHash` (`83.03M/s`), `absl::flat_hash_map` (`68.94M/s`), and `Java fastutil` (`47.18M/s`) pull ahead of `Java HashMap` (`29.12M/s`) and `std::unordered_map` (`26.56M/s`); on `100%` negative misses, SwissTable `0x80` EMPTY early-exit pushes `Rust FxHash` to **`270.9M/s`**, `absl` to **`161.7M/s`**, and even `Rust SipHash-1-3` to **`109.2M/s`** (`3.8×` faster than `std::unordered_map`'s `28.83M/s`). On `80M` aged probes, `std::unordered_map` stalls at **`0.25 IPC`** (`206.9M` dTLB misses, `360.2M` LLC) vs. `Rust` (`174.3M–180.3M` LLC) and `absl` (`225.2M` LLC).
- **Story 2 — `60s` Metrics Flush (`2M` Full) vs. `3 AM` `90%`-Evicted Flip (`200K` Sparse):** Walking all `2,000,000` live counters (`0` hashing), `Rust` clears in **`2.45 ms`** (`816.1M/s`, `1.51 IPC`), `absl` in **`8.34 ms`** (`0.93 IPC`), `fastutil` in **`11.62 ms`**, and `Java HashMap` in **`39.08 ms`**, while `std::unordered_map` stalls to **`78.83 ms`** (`0.13 IPC`, **`32×` slower**). After `90%` idle eviction inside unshrunk peak `4.19M` capacity (`4.8%` fill), `std::unordered_map` walks only live nodes (`2.08B` insns) and speeds up **`20.4×`** to **`3.86 ms`**—edging out standalone `absl::flat_hash_map` (**`3.94 ms`**, `7.10B` insns scanning empty control slots), while Rust's SIMD `RawIter` remains fastest at **`1.15–1.26 ms`**.
- **Story 3 — JVM Stream Processor RAM (`16B`) & The `32B` Struct Cliff:** For `2M × 16B` (`32 MB` / `30.5 MiB` raw), **`Java fastutil` (`66.0 MiB`, `34.6 B/entry`)** is leanest overall—beating `Rust` & `absl` (`68.1 MiB`), `std::unordered_map` (`77.5 MiB`), and cutting boxed `Java HashMap` (`169.3 MiB`, `88.7 B/entry`, `6M` extra GC objects) by **`61%`**. At `32B` (`8B` key + `24B` bucket struct), power-of-two capacity doubling pushes `Rust` & `absl` to **`132.0–132.1 MiB`** (`+22%`), making `std::unordered_map` (**`108.0 MiB`**, `-18%`) the leanest map and highlighting C++ pointer stability (`WG21 P1863R1`) vs. Rust's borrow checker.
- **Story 4 — Cold-Start Warmup Freezes (`0 -> 2M` Inserts):** Unreserved growth stalls to **`70.00 ms`** (`C++ std`), **`57.80 ms`** (`Java G1GC`), **`44.38 ms`** (`Java Generational ZGC` — mutator-thread `HashMap.resize()` copy!), **`35.71 ms`** (`Rust`), and **`25.16 ms`** (`absl`). Upfront pre-sizing (`reserve` / `with_capacity` / initial capacity) flattens spikes to **`6.8 µs`** (`fastutil ZGC`), **`15.6 µs`** (`Rust`), **`97.0 µs`** (`fastutil G1GC`), **`157.7 µs`** (`C++ std`), **`171.0 µs`** (`absl`), and **`351.6 µs`** (`Java HashMap G1GC`).
- **Story 5 — Public JSON Batch API HashDoS (`40,000` Keys):** Multiples of GCC prime `42,043` collapse `std::unordered_map` **`1,251×`** (`0.83 ms` $\rightarrow$ **`1,043.32 ms`**; `11,318 ms` post-fragmentation in `GBench`) with `40,000/40,000` entries in `bucket(0)`, while `K^-1` targeting collapses `Rust FxHash` **`1,072×`** (`689.36 ms`). Three distinct defenses stay resilient: salted `absl::flat_hash_map` (**`0.43 ms`**), keyed `Rust SipHash-1-3` (**`0.64 ms`**), and `Java 21 HashMap` `JEP 180` Red-Black trees (**`5.86 ms`**, `5.0×`).

---

## 3. Quick Start Commands (Bazel 9)

```bash
# 1. Run C++20 GoogleTest & Java 21 JUnit 5 verification tests
bazel test -c opt //common/hash_maps:hash_maps_test //episodes/high_performance/15_hash_maps_across_languages:hotpath_java_test

# 2. Run C++20 Google Benchmark & Java 21 OpenJDK JMH suites
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:benchmark
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:java_benchmarks

# 3. Run Live HashDoS Reproducer & Standalone Forensics
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:live_hashdos_repro
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:challenge
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:java_challenge
bazel run -c opt //episodes/high_performance/15_hash_maps_across_languages:java_challenge_zgc

# 4. Run full telemetry, unmultiplexed perf stat, & disassembly script
./episodes/high_performance/15_hash_maps_across_languages/run_ep15_forensics.sh
```
