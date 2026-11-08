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

# Episode 14: The Physics of Memory Safety (C++20 vs. Rust 1.91 vs. Java 21 — Spatial Bounds, AVX-512 Vectorization & Temporal Lifetimes)

Welcome to **Episode 14** of **Hot Path C++**. This package contains the complete C++20 (Google Benchmark + GoogleTest + Forensic Challenge + Live UAF Reproducer), Rust 1.91 (Spatial & Generational Slab Suite + Compile-Time Borrow Checker Reproducer), and Java 21 LTS (Hermetic Bazel + OpenJDK JMH 1.37 + Log4j 2 + JUnit 5 + G1GC/ZGC Forensic Challenge) suites, annotated x86-64 disassembly, Linux `perf stat` hardware PMU runner, and authentic live session captures for Episode 14.

---

## 1. Directory Structure & Deliverables

- **C++20 Source & Benchmarks (Google C++ Style + Doxygen + Classic Header Guards):**
  - [`memory_safety.h`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/memory_safety.h) — Core header implementing `Order` (`32B`), `AuditEntry` (`32B`), `OrderHandle` (`8B`), `OrderSlabSlot` (`64B`), `OrderSlabPool` (`O(1)` intrusive freelist with 32-bit epoch validation), and non-conflated kernels for Battle 1A (Sequential Scan), Battle 1B (Independent Random Batch Gather), Battle 1C (Latency-Bound Dependent Probe Chain), and Battle 3 (Temporal Safety under 25% Order Cancellation & ABA Slot Recycling).
  - [`order_book_types.h`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/order_book_types.h) — Forwarding header for shared order book types and constants.
  - [`benchmark.cc`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/benchmark.cc) — Google Benchmark suite (`benchmark` and `benchmark_hardened` with `-D_GLIBCXX_ASSERTIONS`).
  - [`challenge.cc`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/challenge.cc) & [`benchmark_spatial_and_temporal.cc`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/benchmark_spatial_and_temporal.cc) — Standalone C++20 forensic CLI & `perf stat` runner (`battle1a`, `battle1b`, `battle1c`, `battle3`, `perf_*`, `all`) with unmangled `extern "C" ep14_*` entry points for disassembly inspection.
  - [`live_uaf_repro.cc`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/live_uaf_repro.cc) — Battle 2 temporal Use-After-Free & 64-byte `glibc` `tcache` reclamation reproducer (`--mode=uaf`, `--mode=safe`, `-DUSE_DANGLING_LOCAL=1`, and `-fsanitize=address`).
  - [`memory_safety_test.cc`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/memory_safety_test.cc) — GoogleTest verification suite covering spatial kernels, bitmask indexing, generational handle ABA prevention, and `tcache` corruption reproduction.
- **Rust 1.91 Counterparts:**
  - [`memory_safety_bench.rs`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/memory_safety_bench.rs) — Standalone Rust 1.91 benchmark suite (`rustc -C opt-level=3 -C target-cpu=native`) comparing iterator RCE, checked indexing (`orders[idx]`), power-of-two bitmask indexing (`orders[idx & 0xFFFF]`), `unsafe get_unchecked`, and `OrderSlabPool` across Battles 1A, 1B, 1C, and 3.
  - [`live_uaf_repro.rs`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/live_uaf_repro.rs) — Compile-time borrow-checker demonstration (`rustc --cfg trigger_uaf_compile_error` emitting `error[E0502]`) alongside safe `OrderSlabPool` execution.
- **Java 21 LTS Source & JMH Benchmarks (Google Java Style + Javadoc + Log4j 2):**
  - [`java/com/hotpath/episode14/OrderObj.java`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/java/com/hotpath/episode14/OrderObj.java) & [`JavaMemorySafetyEngine.java`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/java/com/hotpath/episode14/JavaMemorySafetyEngine.java) — Boxed `OrderObj[]` vs. flat primitive `long[]` SoA & `PrimitiveGenerationalSlab`.
  - [`java/com/hotpath/episode14/JavaMemorySafetyBenchmark.java`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/java/com/hotpath/episode14/JavaMemorySafetyBenchmark.java) & [`runner/`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/java/com/hotpath/episode14/runner/GoogleBenchmarkRunner.java) — OpenJDK JMH 1.37 suite with Google-Benchmark-formatted tabular output.
  - [`java/com/hotpath/episode14/JavaForensicChallenge.java`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/java/com/hotpath/episode14/JavaForensicChallenge.java) & [`MemorySafetyBench.java`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/MemorySafetyBench.java) — Standalone CLI forensic runner under G1GC and Generational ZGC.
  - [`javatests/com/hotpath/episode14/JavaMemorySafetyTest.java`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/javatests/com/hotpath/episode14/JavaMemorySafetyTest.java) — JUnit 5 verification suite.
- **Disassembly, Telemetry Report & Authentic Live Session Captures:**
  - [`BENCHMARK_REPORT.md`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/BENCHMARK_REPORT.md) — Comprehensive empirical report with Google Benchmark, JMH, `perf stat`, and disassembly analysis.
  - [`assembly/godbolt_comparison.s`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/assembly/godbolt_comparison.s) — Complete Intel-syntax disassembly of all 6 `ep14_*` kernels (`llvm-objdump -d -M intel`).
  - **Compiler Explorer (Godbolt) Permanent Shortlink:** [`https://godbolt.org/z/1fdvehqGG`](https://godbolt.org/z/1fdvehqGG)
  - [`run_ep14_forensics.sh`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/run_ep14_forensics.sh) & [`run_ep14_live_demo.sh`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/run_ep14_live_demo.sh) — Automated end-to-end telemetry & live session drivers.
  - [`assets/`](file:///home/kelvingorekore/projects/hotpathcplusplus/episodes/high_performance/14_memory_safety_physics/assets/) — 5 authentic 1080p terminal & Godbolt web captures.

---

## 2. Verified Hardware Telemetry (`AMD Ryzen 9 9950X 16-Core Zen 5`)

### Battle 1A: Sequential Scan (`65,536` Orders — Compiler Range Check Elimination)

| Variant (`65,536` Orders, `2.0 MiB` Working Set) | Clang 18 (`-O3`) | GCC 13 (`-O3`) | Rust 1.91 (`-C opt-level=3`) | Java 21 C2 JIT (`JMH`) | Microarchitectural Mechanism |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Unchecked Sequential (`ptr[i]`)** | `0.558 ns/op` | `0.265 ns/op` | `0.315 ns/op` | — | AVX-512 unrolled loop; induction variable `i < N` proven in-bounds |
| **Checked Sequential (`span[i]` / Slice / Array)** | `0.564 ns/op` | `0.275 ns/op` | **`0.309 ns/op`** | `0.430 ns/op` (`OrderObj[]`) | **0.0% Overhead:** Range Check Elimination (RCE) hoists bounds check out of loop |
| **Java 21 Primitive `long[]` SoA Sequential** | — | — | — | **`0.062 ns/op`** (`16.1B/s`) | C2 SuperWord AVX-512 `vpmullq` + RCE (`6.9x` faster than `OrderObj[]`) |

### Battle 1B: Independent Random Batch Gather (`65,536` Indirections — Vectorization vs. `ud2` Trap)

| Variant (`262,144,000` Lookups under `perf stat`) | Clang 18 (`ns/op`) | GCC 13 (`ns/op`) | Rust 1.91 (`ns/op`) | Instructions Retired (`perf`) | Vectorization & Safety Analysis |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **1. Unchecked Gather (`orders[idx]`)** | `0.755 ns` | **`0.538 ns`** | `0.720 ns` (`unsafe`) | **`435,843,037`** (`1.66 insn/op`) | Emits AVX-512 `vpgatherqq zmm` (`8` loads/instruction); **0% spatial safety** |
| **2. Checked Hardened Trap (`idx >= N ? ud2`)** | `0.896 ns` | `0.927 ns` | `0.794 ns` (`orders[idx]`) | **`2,665,672,186`** (`6.12x` more!) | Side-effecting trap (`ud2` / `panic_bounds_check`) blocks speculative `vpgatherqq`; drops to scalar |
| **3. Checked `std::vector::at()` (`throw`)** | `0.903 ns` | `0.974 ns` | — | `2,927,820,623` (`11.17 insn/op`) | Stack-unwinding landing pad + scalar loop (`1.81x` slower than GCC AVX-512) |
| **4. Power-of-Two Bitmask (`orders[idx & 0xFFFF]`)** | **`0.760 ns`** | `0.785 ns` | **`0.714 ns` (100% Safe!)** | **`468,966,184`** (`1.79 insn/op`) | **`vpandd` + `vpgatherqq`:** Algebraically `<= 65535`; elides bounds branch and restores AVX-512! |

### Battle 1C: Latency-Bound Dependent Probe Chain (`65,536` Steps — RoB & Branch Predictor Hiding)

| Variant (`next = (idx + qty) & 0xFFFF`) | Clang 18 (`ns/op`) | GCC 13 (`ns/op`) | Rust 1.91 (`ns/op`) | IPC (`perf`) | Branch Miss Rate | Microarchitectural Mechanism |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **Unchecked Dependent Chain** | `8.763 ns` | `8.894 ns` | `8.742 ns` | `0.48` | `0.00%` | Serial L2 load dependency (`~40 cycles` per step); cannot auto-vectorize |
| **Hardened Checked (`cmp + jbe ud2`)** | **`8.747 ns`** | **`8.923 ns`** | **`8.763 ns`** | `0.72` | **`0.00%`** | Out-of-order engine executes `cmp + jbe` for free inside L2 load stall shadow |
| **Power-of-Two Bitmask (`idx & 0xFFFF`)** | `8.758 ns` | `8.918 ns` | `8.778 ns` | `0.48` | `0.00%` | Identical serial latency floor (`~8.75 ns/op`) |

### Battle 2: Temporal Memory Safety (`std::vector` Reallocation & `64B` `glibc` `tcache` Reuse)

| Protection Mechanism | Build / Runtime Flag | Detection Phase | Production Latency Overhead | Catches `best_bid = &book[0]` Across `book.push_back()`? |
| :--- | :--- | :---: | :---: | :--- |
| **Compiler Warning (`-Wdangling`)** | `-Wall -Wextra -Wdangling` | Compile-Time | `0.0%` | **NO** for `const Order* best_bid = &book[0]` (`0` warnings; silently reads `$9,999,999.00` from `AuditEntry`!) |
| **Hardened Standard Library** | `-D_GLIBCXX_ASSERTIONS` | Runtime (`__builtin_trap`) | `0.0%` – `18.7%` (Scalar Gather) | **NO** (Spatial bounds only; cannot track dangling pointers after reallocation) |
| **AddressSanitizer (ASan)** | `-fsanitize=address` | Runtime Shadow Check | `~2.0x` CPU / `1.5x–3x` RAM | **YES** (`*(addr >> 3) + 0x7fff8000 == 0xfd` catches `READ of size 8` at `0x508000000028`) |
| **Rust 1.91 Borrow Checker** | `rustc` (Default Safe) | **Compile-Time** | **`0.0%`** | **YES** (`error[E0502]: cannot borrow book as mutable because it is also borrowed as immutable`) |
| **C++20 `OrderSlabPool` Handle** | `OrderHandle {index, gen}` | **Runtime Epoch Check** | **`~0.08 ns`** (`cmp + jne`) | **YES** (`100%` of stale/canceled handles rejected in O(1) with `0` heap churn) |

### Battle 3: Temporal Safety Under 25% Order Cancellation & ABA Slot Recycling (`65,536` Slots)

| Temporal Strategy (`25%` Canceled & Recycled) | Clang 18 (`ns/op`) | GCC 13 (`ns/op`) | Rust 1.91 (`ns/op`) | Java 21 (`512 MB` Heap) | Instructions (`perf`) | Silent ABA Corruptions |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **1. Raw Unchecked Slot Index (`slots[idx]`)** | `2.490 ns` | `2.183 ns` | `2.084 ns` | — | `839.2M` (`3.20/op`) | **`16,384` (`25.0%` Silent ABA Bug!)** |
| **2. 64-Bit Generational Handle (`OrderSlabPool`)** | **`2.567 ns`** | **`2.176 ns`** | **`2.423 ns`** | **`3.112 ns`** (`0` GC Cycles) | **`1,037.0M`** (`3.96/op`) | **`0` (`100%` Caught in O(1))** |
| **3. `std::weak_ptr<Order>::lock()`** | `5.987 ns` | `5.512 ns` | — | — | `2,399.8M` (`2.31x` more!) | `0` (`2.53x` slower; control-block refcount traffic) |
| **4. Java 21 Boxed `OrderObj[]` (G1GC / ZGC)** | — | — | — | `3.530 ns` (G1) / `4.142 ns` (ZGC) | — | `0` (`20` ZGC cycles / `34 ms` GC pause time) |

---

## 3. Quick Start Commands (Bazel 9)

```bash
# 1. Run C++20 (GoogleTest) and Java 21 (JUnit 5) Unit Tests
bazel test -c opt //episodes/high_performance/14_memory_safety_physics/...

# 2. Run C++20 Google Benchmark Suite (Standard & Hardened -D_GLIBCXX_ASSERTIONS)
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:benchmark
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:benchmark_hardened

# 3. Run Battle 2 Temporal Use-After-Free & tcache Reuse Reproducer
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:live_uaf_repro -- --mode=uaf
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:live_uaf_repro -- --mode=safe

# 4. Run Java 21 OpenJDK JMH Suite & Forensic Challenges (G1GC & Generational ZGC)
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:java_benchmarks
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:java_challenge
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:java_challenge_zgc

# 5. Run Standalone C++20 Forensic Challenge & Full Multi-Language Telemetry Script
bazel run -c opt //episodes/high_performance/14_memory_safety_physics:challenge
./episodes/high_performance/14_memory_safety_physics/run_ep14_forensics.sh
```
