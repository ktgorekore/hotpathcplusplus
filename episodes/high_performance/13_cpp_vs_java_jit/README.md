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

# Episode 13: Is C++ Actually Faster Than Java? (When the JIT Wins, Why It Loses & What 'Fast' Means in Production)

Welcome to **Episode 13** of **Hot Path C++**. This package contains the complete C++20 (Google Benchmark + GoogleTest + Forensic Challenge) and Java 21 LTS (Hermetic Bazel + OpenJDK JMH 1.37 + Log4j 2 + JUnit 5) benchmark suites, annotated x86-64 disassembly, Linux `perf stat` hardware PMU runner, and production scripts/metadata for Episode 13.

---

## 1. Directory Structure & Deliverables

- **C++20 Source & Benchmarks (Google C++ Style + Doxygen):**
  - [`cpp_vs_java_jit.h`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/cpp_vs_java_jit.h) — Cross-TU polymorphic `Event` interface, `OrderResult`, `FlatEvent`, `Order` (`32B` AoS), `OrderBookSoA`, and Battle 3 pre-faulted slab pool telemetry engine.
  - [`cpp_vs_java_jit_oop_tu.cc`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/cpp_vs_java_jit_oop_tu.cc) — Isolated translation unit implementing the 5 concrete `Event` subclasses (`LimitOrderEvent`, `CancelOrderEvent`, `ModifyOrderEvent`, `PeggedOrderEvent`, `MarketHaltEvent`) and `MakeEventByType`.
  - [`benchmark.cc`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/benchmark.cc) — Google Benchmark suite (`benchmark` without LTO and `benchmark_lto` with `-flto`).
  - [`challenge.cc`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/challenge.cc) — Standalone C++20 forensic CLI runner (`battle1`, `battle2_aos`, `battle2_soa`, `battle2_ptr`, `battle3`, `all`).
  - [`cpp_vs_java_jit_test.cc`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/cpp_vs_java_jit_test.cc) — GoogleTest verification suite.
- **Java 21 LTS Source & JMH Benchmarks (Google Java Style + Javadoc/Doxygen + Log4j 2):**
  - [`java/com/hotpath/episode13/OrderEvent.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/OrderEvent.java), [`OrderResult.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/OrderResult.java), [`LimitOrderEvent.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/LimitOrderEvent.java), [`CancelOrderEvent.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/CancelOrderEvent.java), [`ModifyOrderEvent.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/ModifyOrderEvent.java), [`PeggedOrderEvent.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/PeggedOrderEvent.java), [`MarketHaltEvent.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/MarketHaltEvent.java) — Battle 1 & 3 polymorphic event hierarchy.
  - [`java/com/hotpath/episode13/OrderRecord.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/OrderRecord.java) & [`OrderBookLayouts.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/OrderBookLayouts.java) — Battle 2 boxed `OrderRecord` (`48B` object + `4B` ref = `52B`) vs. flat primitive `long[]` SoA layout.
  - [`java/com/hotpath/episode13/StreamingStateEngine.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/StreamingStateEngine.java) — Battle 3 GC tail-latency & C2 Uncommon Trap deoptimization harness.
  - [`java/com/hotpath/episode13/JavaHotPathBenchmark.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/JavaHotPathBenchmark.java) & [`runner/`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/runner/GoogleBenchmarkRunner.java) — OpenJDK JMH microbenchmark suite with Google-Benchmark-formatted tabular output.
  - [`java/com/hotpath/episode13/JavaForensicChallenge.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/java/com/hotpath/episode13/JavaForensicChallenge.java) — Standalone CLI forensic runner (`--mode=battle1|battle2_fresh|battle2_churned|battle2_soa|battle3|all`).
  - [`javatests/com/hotpath/episode13/JavaHotPathTest.java`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/javatests/com/hotpath/episode13/JavaHotPathTest.java) — JUnit 5 verification suite.
- **Disassembly & Automated Telemetry Runner:**
  - [`assembly/godbolt_comparison.s`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/assembly/godbolt_comparison.s) — Annotated x86-64 disassembly comparing C++ Separate-TU, C++ `-flto`, HotSpot C2 speculative class-guard inlining, and AVX-512 `vpmullq` SoA loops.
  - [`run_ep13_forensics.sh`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/run_ep13_forensics.sh) — End-to-end Bazel 9 build, benchmark, JMH, ZGC, and unmultiplexed `perf stat` runner.
- **Production Scripts & YouTube Upload Kits:**
  - [`HEYGEN_FULL_PRODUCTION_SCRIPT.md`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/HEYGEN_FULL_PRODUCTION_SCRIPT.md) — 29-slide full episode teleprompter script and synchronization blueprint.
  - [`PRODUCTION_EDITS.md`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/PRODUCTION_EDITS.md) — Empirical Zen 5 hardware calibration table and engineering notes.
  - [`YOUTUBE_FULL_EPISODE_UPLOAD_METADATA.md`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/YOUTUBE_FULL_EPISODE_UPLOAD_METADATA.md) — Full episode titles, A/B testing matrix, chapters, description, and pinned comment.
  - [`shorts/`](file:///home/kelvingorekore/projects/hotpathcpp/episodes/high_performance/13_cpp_vs_java_jit/shorts/README.md) — Companion YouTube Shorts `13A` and `13B` scripts and upload metadata packages.

---

## 2. Verified Hardware Telemetry (`AMD Ryzen 9 9950X 16-Core Zen 5`)

### Battle 1: Dynamic Allocation (`malloc` vs. TLAB/Escape Analysis) & Polymorphic Dispatch

| Benchmark Variant | C++20 (Separate-TU `-O3`) | C++20 (`-O3 -flto`) | Java 21 HotSpot C2 (Forensic) | Java 21 HotSpot C2 (JMH) | Architectural Winner & Mechanism |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Naive OOP (`new` + Virtual Dispatch)** | `16.8 ns/op` (`59.7M/s`) | `15.2 ns/op` (`66.0M/s`) | **`0.73 ns/op`** (`1.38G/s`) | **`0.89 ns/op`** (`1.12G/s`) | **Java 21 C2 (`22.9x` faster)** — Speculative inlining + Escape Analysis Scalar Replacement (`0` heap bytes) |
| **Fresh Event + Result Alloc in Loop** | `16.8 ns/op` (`59.7M/s`) | `15.2 ns/op` (`66.0M/s`) | **`0.53 ns/op`** (`1.89G/s`) | **`0.67 ns/op`** (`1.50G/s`) | **Java 21 C2 (`31.7x` faster)** — Both allocations eliminated into CPU registers |
| **Pre-Allocated Virtual Dispatch (Monomorphic)** | `1.52 ns/op` (`658.9M/s`) | `1.32 ns/op` (`757.8M/s`) | **`0.73 ns/op`** (`1.38G/s`) | **`0.89 ns/op`** (`1.12G/s`) | **Java 21 C2 (`1.8x–2.1x` faster)** — Class guard (`cmp [rsi+8], imm32`) beats C++ indirect `call [rax+0x18]` |
| **Pre-Allocated Virtual Dispatch (50/50 Bimodal)** | `1.51 ns/op` (`661.0M/s`) | **`1.31 ns/op`** (`761.8M/s`) | `2.66 ns/op` (`375.3M/s`) | `2.55 ns/op` (`391.7M/s`) | **C++20 (`1.9x` faster)** — C2 2-receiver PIC + branch misprediction overhead |
| **Pre-Allocated Virtual Dispatch (5-Type Megamorphic)** | `1.52 ns/op` (`656.8M/s`) | **`1.33 ns/op`** (`750.5M/s`) | `3.32 ns/op` (`301.5M/s`) | `3.52 ns/op` (`284.5M/s`) | **C++20 (`2.5x–2.6x` faster)** — Exceeds `TypeProfileWidth=2`, defeating C2 inlining & Escape Analysis |
| **Idiomatic Flat Value (`FlatEvent` 24B, 0 `vptr`)** | **`0.56 ns/op`** (`1.78G/s`) | **`0.55 ns/op`** (`1.82G/s`) | — | — | **C++20 Flat Value** — Direct register arithmetic, zero `vptr`, zero deoptimization risk |

### Battle 2: Cache-Line Physics & Hardware PMU Counters (`4,000,000` Orders)

| Memory Layout (`4,000,000` Orders) | Scan Latency | Per-Order Latency | LLC Miss Rate (`perf stat`) | `dTLB` Load Misses (`perf stat`) | Hardware Analysis |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Java 21 `ArrayList<OrderRecord>` (Post-Churn Heap)** | `32.21 ms` (`32.25 ms` JMH) | `8.05 ns/order` | **`49.4%`** (`477.1M` misses / `40` scans) | **`204.6M`** (`5.11M` / scan) | `52B`/order (`12B` header + `4B` pad + `4B` ref); pointer chasing stalls at `0.38 IPC` |
| **C++20 `std::vector<Order*>` (Post-Churn Pointers)** | `26.6 ms` (`4.49 GiB/s`) | `6.65 ns/order` | **`51.6%`** (`398.2M` misses / `52` scans) | **`245.6M`** (`4.72M` / scan) | Proves pointer chasing in C++ suffers the exact same L3 + dTLB collapse |
| **Java 21 `ArrayList<OrderRecord>` (Fresh Sequential TLAB)** | `5.24 ms` (`5.24 ms` JMH) | `1.31 ns/order` | `10.7%` (`79.1M` misses / `40` scans) | `7.06M` (`176K` / scan) | **The Microbenchmark Trap:** sequential TLAB bump allocation hides pointer chasing (`6.1x` illusion) |
| **C++20 `std::vector<Order>` (Contiguous `32B` AoS)** | **`4.74 ms`** (`25.18 GiB/s`) | **`1.18 ns/order`** | **`6.3%`** (`104.8M` misses / `298` scans) | `18.0M` (`60K` / scan) | **`6.8x` faster than Churned `ArrayList`**; `2` orders per `64B` cache line |
| **C++20 `OrderBookSoA` (AVX-512 `vpmullq` Flat SoA)** | **`1.56 ms`** (`57.26 GiB/s`) | **`0.39 ns/order`** | **`1.5%`** (`46.4M` misses / `868` scans) | `22.1M` (`25K` / scan) | **`20.6x` faster than Churned `ArrayList`**; saturates Zen 5 memory bus |
| **Java 21 Flat Primitive Arrays (`long[]` SoA)** | **`1.55 ms`** (`1.56 ms` JMH) | **`0.39 ns/order`** | `14.9%` (`76.9M` misses / `40` scans) | `5.25M` (`131K` / scan) | **`20.8x` faster than Churned `ArrayList`**; ties C++ AVX-512 SoA! |

### Battle 3: Streaming State Tail Latency (`2,097,152` Live Orders, `50,000` Batches × `256` Updates) & C2 Deopt

| Runtime & Memory Manager | `p50` Batch | `p99` Batch | `p99.9` Batch | `p99.99` Batch | `Max` Batch Spike | 1st Rare `MarketHaltEvent` | GC Cycles / Time |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 Pre-Faulted Slab Pool** | **`0.401 µs`** | **`1.744 µs`** | **`3.106 µs`** | **`8.166 µs`** | **`24.096 µs`** | **`0.14 ns/op`** (`0` Deopt) | `0 cycles` (`0 ms`) |
| **Java 21 Default G1GC (`-Xms4g -Xmx4g`)** | `1.543 µs` | `5.050 µs` | `15.981 µs` | `58.352 µs` | **`11,163.784 µs` (`11.16 ms`)** | **`35,407 ns` (`35.41 µs`)** | `3 cycles` (`30 ms`) |
| **Java 21 Generational ZGC (`-Xms4g -Xmx4g`)** | `1.533 µs` | `8.176 µs` | `18.876 µs` | `90.733 µs` | **`268.693 µs` (`0.27 ms`)** | **`37,952 ns` (`37.95 µs`)** | `6 cycles` (`20 ms`) |
| **Java 21 Generational ZGC (`-Xms1536m` Tight)** | `2.836 µs` | `16.692 µs` | `30.739 µs` | `111.002 µs` | **`618.732 µs` (`0.62 ms`)** | **`23,465 ns` (`23.47 µs`)** | `15 cycles` (`94 ms`) |

---

## 3. Quick Start Commands (Bazel 9)

```bash
# 1. Run C++20 and Java 21 Unit Tests
bazel test -c opt //episodes/high_performance/13_cpp_vs_java_jit/...

# 2. Run C++20 Google Benchmark Suite (Separate-TU vs. Whole-Program LTO)
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:benchmark
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:benchmark_lto

# 3. Run Java 21 OpenJDK JMH Suite (Formatted with Google Benchmark Table Printer)
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:java_benchmarks

# 4. Run Standalone Forensic Challenges (C++20, Java G1GC, Java ZGC 4G & Tight Heap)
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:challenge
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:java_challenge
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:java_challenge_zgc
bazel run -c opt //episodes/high_performance/13_cpp_vs_java_jit:java_challenge_zgc_tight

# 5. Run Full Automated Telemetry & Linux perf stat Suite
./episodes/high_performance/13_cpp_vs_java_jit/run_ep13_forensics.sh
```
