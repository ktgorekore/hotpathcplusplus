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

# Episode 12: Why Our C++ Code Got 7.5x Slower Hours Later — `malloc`, Arenas, Slab Pools & C++17 `std::pmr`

[![Live Compiler Explorer](https://img.shields.io/badge/Godbolt-x86--64_Assembly-blue)](https://godbolt.org/z/7hTYGsWjb)

## 1. Executive Summary
**Episode 12** closes Phase II (*Multi-Core Coherency, Kernel Boundaries & Memory Architecture*) of *Hot Path C++* by dissecting **Ring 3 memory allocation** across `glibc 2.39` (`ptmalloc2`), `jemalloc 5.3.0`, custom C++20 allocators (`BumpArena`, zero-header `FixedSlabPool`, `ThreadMagazinePool<32>`), and C++17 `<memory_resource>` (`std::pmr`):

1. **Battle 1 — Inside `glibc ptmalloc2`, `jemalloc` & Cross-Thread Churn:**
   - `glibc` wraps a `32B` `OrderNode` inside an `8B` chunk header rounded up to `16B` alignment (**`48B` chunk stride**, straddling `64B` cache lines) and caps each of its `64` per-thread `tcache` bins at **`7` chunks** before spilling to locked `fastbins`/bins (`59.1 ns` @ `7` $\rightarrow$ `605 ns` @ `64` $\rightarrow$ `10,240 ns` @ `1,024`).
   - Single alloc/free latency: `new`/`delete` = **`8.02 ns`** vs. `FixedSlabPool` (`0B` header) = **`1.39 ns` (`5.77x`)** vs. `BumpArena` (`4` inline instructions) = **`0.739 ns` (`10.85x`)**. At `1,024` objects, `BumpArena + O(1) Reset` runs in **`738 ns` (`13.88x` faster)**.
   - Across `4` producer + `4` consumer threads handing `OrderNode*` over lock-free SPSC rings, `ThreadMagazinePool<32>` boosts Google Benchmark throughput by **`1.49x`** (`5.87 ms` $\rightarrow$ `3.95 ms`; **`91.1M ops/s`** vs. `70.3M/s` `glibc` & `47.2M/s` `jemalloc`).
2. **Battle 2 — The Fresh Heap Illusion vs. Production "Heap Aging" (`65,536` `OrderNode`s):**
   - On a fresh startup heap, `ScanOrderChain()` runs in **`58.2 µs` (`0.88–0.91 ns/node`)** with `100% <= 64B` stride and `1.62` IPC.
   - After mixed-size market/audit churn, the *exact same* scan loop collapses **`7.5x` slower** to **`433.7 µs` (`6.57–6.89 ns/node`)** (`405.4 µs` under `jemalloc`), with a **`1.12 MiB` average pointer hop**, **`95.23M` cache misses (`10.7x`)**, **`2.93M` `dTLB` misses (`16.0x`)**, and **`0.29` IPC**.
   - Compacted `BumpArena` and chained `std::pmr` restore `32B` packing (`2` nodes/`64B` cache line) in **`57.6–58.0 µs` (`1.70` IPC, `-91.1%` cache misses, `-94.0%` `dTLB` misses)**. For request-scoped `4,096`-entry `unordered_map` workloads, `std::pmr::monotonic_buffer_resource` $O(1)$ "wink-out" cuts lifecycle latency **`2.77x`** (`109.9 µs` $\rightarrow$ **`39.6 µs`**).
3. **Battle 3 — C++17 `std::pmr` Stack Buffers & The Nested `std::string` Trap (`24` Fills/Packet):**
   - Omitting `using allocator_type = std::pmr::polymorphic_allocator<>;` leaves `std::uses_allocator_v` `false`, silently escaping **`24` `::operator new` calls per packet (`434 ns`)** whenever strings exceed `libstdc++`'s `15`-byte SSO buffer.
   - Declaring `allocator_type` drops global heap allocations to **`0`**, cutting packet parsing **`3.14x`** (`459 ns` $\rightarrow$ **`146 ns`**) and slashing retired instructions by **`74.8%`** (`1,760.2M` $\rightarrow$ `443.8M`).

---

## 2. Quick Start: Tests, Reproducer, Benchmarks & PMU Forensics

```bash
# 1. Run GoogleTest verification suite (GCC & Clang)
bazel test -c opt //common/memory_allocators:memory_allocators_test

# 2. Run the Cold-Open Heap Aging & std::pmr Nested-String Escape Reproducer
bazel run -c opt //:memory_allocators_heap_aging_repro

# 3. Run the Full Google Benchmark Suite
bazel run -c opt //:memory_allocators_benchmark -- --benchmark_min_time=0.25s

# 4. Run the Two-Pass Unmultiplexed perf stat + glibc vs. jemalloc Forensic Runner
./episodes/high_performance/12_memory_allocators_and_pmr/run_ep12_forensics.sh
```

See [`BENCHMARK_REPORT.md`](BENCHMARK_REPORT.md) for complete Google Benchmark outputs, `glibc` vs. `jemalloc` comparison tables, two-pass unmultiplexed `perf stat` logs, and annotated Intel-syntax disassembly ([`assembly/godbolt_comparison.s`](assembly/godbolt_comparison.s) • [Godbolt](https://godbolt.org/z/7hTYGsWjb)).
