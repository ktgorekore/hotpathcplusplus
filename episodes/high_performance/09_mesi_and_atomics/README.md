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

# Episode 09: Scaling Lock-Free C++ Across 16 Cores (MESI Coherency, RFO Traffic & Production Patterns)

Welcome to **Episode 09** of the **Hot Path C++** low-latency systems engineering series.

In **Episode 08**, we followed a single CPU core's write (`x = 5`) into its private hardware **Store Buffer**. In **Episode 09**, we anchor everything in a realistic production C++20 service (**`HttpRouter::handle_request`**) and measure what happens when three lock-free atomic patterns scale from 1 core to 16 physical cores:

1. **Production Problem 1 — Global Telemetry Counters & Trace IDs (`fetch_add(1, relaxed)`):**
   - Why `std::memory_order_relaxed` still emits `lock xadd` on x86-64, causing 16-core MESI Request For Ownership (`RFO`) contention (`3.67 ns/op` on 1 core $\rightarrow$ `109.2 ns/op` per-thread on 16 cores, `0.013 IPC`, `52.4%` cache misses in `perf stat`).
   - Why unpadded array sharding (`std::atomic<uint64_t> c[16]`) still contends due to 64-byte **False Sharing**.
   - **Production Fix 1A (Counters):** `alignas(64)` Per-Core Sharded Counter (`ShardedCounter<64>`, modeled after Envoy Proxy stats & Linux `percpu_counter`) $\rightarrow$ **`3.85 ns/op` (`4.16 Billion ops/sec` — 28.4x faster!)**.
   - **Production Fix 1B (Unique Trace IDs):** `BatchedIdGenerator<64>` leasing 64 IDs per `fetch_add(64)` $\rightarrow$ **`4.79 ns/op` (`3.34 Billion ops/sec` — 22.8x faster!)**.
2. **Production Problem 2 — Contended Buffer Pool (`compare_exchange_weak`):**
   - Why a *failed* `compare_exchange_weak` is **not** a read-only operation on x86-64 (`lock cmpxchg` acquires `Modified [M]` cache line ownership *before* comparing, generating RFO traffic across retrying cores).
   - **Level 1 Fix:** `_mm_pause()` + relaxed `Shared [S]` read backoff (`3,529 ns` $\rightarrow$ `1,914 ns` — **1.84x faster**).
   - **Level 2 Architectural Fix:** `ThreadMagazine<32>` (`tcmalloc` / `mimalloc`-style per-thread batch-32 buffer magazine) $\rightarrow$ **`81.8 ns/op` per-thread / `4.76 ns/op` aggregate (`195.6M ops/sec` — 43.1x faster!)**.
3. **Production Problem 3 — Zero-Copy `#pragma pack(1)` Wire Header Split Lock (`std::atomic_ref` at Byte Offset 60):**
   - How an 8-byte atomic field at `Byte Offset 60` straddles two 64-byte cache lines (`Bytes 60..63` in Line 0, `Bytes 64..67` in Line 1), triggering a global interconnect **Bus Lock (`LOCK#`)** (**`3.70 ns` $\rightarrow$ `795.0 ns/op` — 215x slower!**) or a Linux `split_lock_detect` `#AC` / `SIGBUS` exception.
   - **Production Fix 3:** Compile-time alignment verification (`static_assert(offsetof(WireHeader, seq_num) % std::atomic_ref<uint64_t>::required_alignment == 0)`).

---

## 📊 Empirical Bare-Metal Telemetry (AMD Ryzen 9 9950X — 16 Cores / Dual CCD)

All benchmarks are written in **100% pure idiomatic C++20**, compiled with `-std=c++20 -O3 -march=native`, and verified with **Google Benchmark** and **Linux `perf stat` hardware PMU counters** (`perf_event_paranoid = -1`) on an **AMD Ryzen 9 9950X 16-Core Processor** (`5.41 GHz`, Dual 8-Core CCDs, `48 KiB` L1D per core, `2 x 32 MiB` L3):

| Benchmark Suite | Scenario | Per-Thread Latency | Aggregate Throughput | Speedup / Hardware Mechanism |
| :--- | :--- | :---: | :---: | :--- |
| **`BM_AtomicLoad_SharedScaling`** | 1 Thread $\rightarrow$ 16 Threads | **`0.20 - 0.24 ns/load`** | **69.25G loads/s** | All 16 L1D caches hold line in **`Shared [S]`** (`0` RFOs) |
| **`BM_AtomicRMW_TopologyPingPong`** | Intra-CCD (`Core 0 <-> Core 1`) | **`11.32 ns/RMW`** | 176.71M ops/s | Shared 32 MiB L3 MOESI transfer inside CCD 0 |
| **`BM_AtomicRMW_TopologyPingPong`** | Cross-CCD (`Core 0 <-> Core 8`) | **`15.50 ns/RMW`** | 129.01M ops/s | **1.37x Slower** — Cross-die Infinity Fabric (GMI3) hop |
| **`BM_AtomicRMW_FetchAdd`** | 1 Thread (Uncontended `fetch_add(1)`) | **`3.67 ns/op`** | 272.45M ops/s | Line stays pinned in Core 0 L1D **`Modified [M]`** |
| **`BM_AtomicRMW_FetchAdd`** | 16 Threads (Contended `fetch_add(1)`) | **`114.81 ns/op`** | 146.56M ops/s | **29.7x Latency Increase** — 16-core `lock xadd` RFO contention (`0.013 IPC`) |
| **`BM_TraceId_BatchedLease64`** | 16 Threads (`fetch_add(64)` Lease) | **`4.79 ns/op`** | **3.34G ops/s** | **22.8x Faster 🚀** — 63 of 64 IDs served from local register |
| **`BM_TelemetryCounter_ShardedAlignas64`** | 16 Threads (`alignas(64)` Shards) | **`3.85 ns/op`** | **4.16G ops/s** | **28.4x Faster 🚀** — Dedicated 64B `[M]` line per core |
| **`BM_CASLoop_Naive`** | 1 Thread (`compare_exchange_weak`) | **`4.84 ns/op`** | 206.67M ops/s | Uncontended `lock cmpxchg` in local L1D `[M]` |
| **`BM_CASLoop_Naive`** | 16 Threads (`compare_exchange_weak`) | **`3,529 ns`** (`287.2 ns` agg) | 4.53M ops/s | Failed `lock cmpxchg` requests **`[M]` state** before comparing |
| **`BM_CASLoop_ReadBeforeCAS`** | 16 Threads (`_mm_pause()` + Load) | **`1,914 ns`** (`115.8 ns` agg) | 8.36M ops/s | **1.84x Faster** — Spins in `[S]` with `_mm_pause()` backoff |
| **`BM_BufferPool_ThreadMagazine32`** | 16 Threads (`tcmalloc` Batch-32 Mag) | **`81.8 ns/op`** (`4.76 ns` agg) | **195.59M ops/s** | **43.1x Faster 🚀** — 31 of 32 allocations hit local L1 cache |
| **`BM_SplitLock_CacheLineBoundary`** | Byte Offset `0` or `56` (Aligned) | **`3.71 - 3.70 ns/op`** | 269.65M ops/s | Fast local L1D **Cache Lock** (~18 cycles) |
| **`BM_SplitLock_CacheLineBoundary`** | Byte Offset `60` (**Split Lock!**) | **`864.79 ns/op`** | 1.26M ops/s | **215x Slower** — `#pragma pack(1)` straddles 64B boundary |

---

## 🛠️ Build, Test & Benchmark Commands (Bazel 9)

```bash
# Run GoogleTest unit tests
bazel test //common/mesi_coherency:mesi_coherency_test

# Run the Google Benchmark suite
bazel run -c opt //episodes/high_performance/09_mesi_and_atomics:benchmark -- --benchmark_min_time=0.3s

# Run the 3-Part Production Retrofit Challenge
bazel run -c opt //episodes/high_performance/09_mesi_and_atomics:challenge
```
