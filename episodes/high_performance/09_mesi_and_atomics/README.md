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

# 🎬 Episode 09: Inside the Silicon Switchboard (MESI, Split Locks & CAS Storms)

> **Subtitle:** How 64-Byte Cache Lines Move Across Cores: MESI/MOESI Protocol, Invalidation Queues, Split Locks, and Read-Before-CAS  
> **Duration Target:** ~08:45 (46-Scene Masterclass Deep Dive) | 3 Companion YouTube Shorts (9A, 9B, 9C)  
> **Host:** Marcus (Systems Architect, 16+ Years Experience in Low-Latency Systems Architecture)  
> **Status:** Production Ready (Full 46-Scene HeyGen Masterclass + 3 Companion Shorts Packages Fully Rendered)  

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.3.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)

---

## 📊 Empirical Silicon & Hardware PMU Telemetry (AMD Ryzen 9 9950X Zen 5)

All benchmarks, hardware PMU counter traces (`perf stat`), and multi-core contention challenges are measured using pure, standard C++20 (`std::atomic<uint64_t>`, `#pragma pack(push, 1)`) on bare-metal **AMD Ryzen 9 9950X** silicon (16 Cores, 32 Threads @ 5.4 GHz Boost, 48 KiB L1D, 1 MiB L2, 2x 32 MiB L3 CCDs) with GCC 13/14 `-O3 -std=c++20 -march=native`:

| Benchmark / Hardware PMU Experiment | Threads / Config | Batch Latency | Per-Op Latency | Throughput / PMU Metric | Speedup / Impact |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `BM_AtomicLoad_SharedScaling` | 1 Thread | 196 ns | **0.19 ns/op** | 5.23 G ops/s | Baseline L1D Hit |
| `BM_AtomicLoad_SharedScaling` | 16 Threads (`Shared S`) | **235 ns** | **0.23 ns/op** | **69.60 G ops/s** | **Linear 16-Core Scaling 🚀** |
| `BM_AtomicRMW_FetchAdd` | 1 Thread (`Modified M`) | 3,755 ns | 3.67 ns/op | 272.73 M ops/s | Local L1D Cache Lock |
| `BM_AtomicRMW_FetchAdd` | 16 Threads (`RFO War`) | 112,393 ns | 109.76 ns/op | 145.78 M ops/s | **30.4x Slower (RFO Storm)** |
| `BM_AtomicRMW_TopologyPingPong/1` | Core 0 $\leftrightarrow$ Core 1 (Same CCD0) | **10,880 ns** | **10.63 ns/op** | **188.24 M ops/s** | Shared 32 MiB L3 Hop |
| `BM_AtomicRMW_TopologyPingPong/8` | Core 0 $\leftrightarrow$ Core 8 (Cross-CCD) | 16,045 ns | 15.67 ns/op | 127.64 M ops/s | Infinity Fabric (GMI3) Hop |
| `BM_CASLoop_Naive` (`perf stat`) | 16 Threads (`lock cmpxchg`) | 3,335 ns | 208.4 ns/op | 2.70B cyc / 28.6M L1 loads | Saturated by Failed RFOs |
| `BM_CASLoop_ReadBeforeCAS` (`perf stat`) | 16 Threads (`Shared S` + `PAUSE`) | **1,747 ns** | **109.2 ns/op** | 1.97B cyc / **130.7M L1 loads** | **1.91x Faster (0 Failed RFOs) 🚀** |
| `BM_SplitLock_CacheLineBoundary/0` | Byte Offset 0 (Aligned) | **3,755 ns** | **3.67 ns/op** | **272.67 M ops/s** | **214x Faster (L1D Lock) 🚀** |
| `BM_SplitLock_CacheLineBoundary/56` | Byte Offset 56 (Aligned) | **3,776 ns** | **3.69 ns/op** | **271.19 M ops/s** | Single 64B Line Lock |
| `BM_SplitLock_CacheLineBoundary/60` | Byte Offset 60 (`#pragma pack(1)`) | 815,060 ns | **795.96 ns/op** | 1.26 M ops/s (~3,800 cyc/op) | **Global Bus Lock Stall! ⚡** |
| `challenge`: Part 1 Split-Lock vs Aligned | 500,000 atomic RMWs | 403.78 ms vs **1.83 ms** | 807.55 ns vs **3.65 ns** | — | **221.0x Alignment Speedup 🚀** |
| `challenge`: Part 2 16-Core CAS Storm | 400,000 ledger updates | 93.91 ms vs **62.35 ms** | 234.78 ns vs **155.87 ns** | 400,000 / 400,000 verified | **1.62x Contention Speedup ✅** |

---

## 🛠️ Reproduction Commands

```bash
# 1. Run unit tests
bazel test //common/mesi_coherency:mesi_coherency_test

# 2. Run Google Benchmark suite
bazel run -c opt //episodes/high_performance/09_mesi_and_atomics:benchmark

# 3. Run Silicon Switchboard Challenge (Split-Lock & 16-Core CAS Storm)
bazel run -c opt //episodes/high_performance/09_mesi_and_atomics:challenge
```
