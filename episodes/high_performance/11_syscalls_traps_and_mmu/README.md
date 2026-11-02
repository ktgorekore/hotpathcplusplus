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

# 🎬 Episode 11: Every Pointer Is a Lie — Syscalls, Page Faults & the Hardware MMU in C++20

> **Subtitle:** Crossing the Ring 0 Silicon Border: Ring 3 `vDSO` Seqlocks (`16.3 ns` vs `58.8 ns` Syscall), `%fs` Thread-Local Storage (`0.19 ns`, `196x`), Optimistic Demand Paging & `#PF` Interrupt Vector 14 (`0.72 µs` Alloc vs `198.4 µs` First Write $\rightarrow$ `3.27 µs` Pool, `60.6x` Faster), 4-Level Radix Page Walks, and `2 MiB` HugePages (`MADV_HUGEPAGE`, `-99.79%` `dTLB` Misses, `1.55x–1.87x` Faster)  
> **Live Compiler Explorer (Godbolt) Disassembly:** [https://godbolt.org/z/W9KETqadE](https://godbolt.org/z/W9KETqadE)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.3.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)

---

## 📊 Empirical Silicon & Hardware PMU Telemetry (AMD Ryzen 9 9950X Zen 5)

All benchmarks, hardware PMU counter traces (`perf stat`), and virtual-memory challenges are measured using standard C++20 on bare-metal **AMD Ryzen 9 9950X** silicon (16 Cores, 32 Threads @ 5.4 GHz Boost, 48 KiB L1D, 1 MiB L2, 64 MiB L3 [2x 32 MiB CCDs], `L1 dTLB = 96 entries`, `L2 dTLB = 3,072 entries = 12 MiB reach @ 4 KiB`) with GCC 13.3 `-O3 -std=c++20 -march=native`:

| Benchmark / Production Workload | Config / Page Granularity | Measured Latency | Throughput | Architectural Root Cause (`perf stat` PMU) |
| :--- | :---: | :---: | :---: | :--- |
| **`BM_Case1_ClockGettime_vDSO_Ring3`** | Ring 3 `[vdso]` + `[vvar]` Seqlock | **16.3 ns/op** | **61.26 M ops/s** | **0 Syscalls (`rdtsc` + Seqlock in Ring 3) 🚀** |
| **`BM_Case1_ClockGettime_RawSyscall_Ring0`** | Ring 0 `syscall(SYS_clock_gettime)` | 58.8 ns/op | 17.00 M ops/s | **3.61x Slower (`swapgs` + `pt_regs` + Ring 0 trap)** |
| **`BM_Case1_Gettid_ThreadLocal_Cached`** | Ring 3 `thread_local` (`%fs` segment) | **0.192 ns/op** | **5.22 G ops/s** | **196x Faster (`mov eax, fs:[offset]` L1D hit) 🚀** |
| **`BM_Case1_Gettid_RawSyscall_TightLoop`** | Ring 0 `syscall(SYS_gettid)` | 37.6 ns/op | 26.61 M ops/s | Ring 3 $\rightarrow$ Ring 0 `entry_SYSCALL_64` transition |
| **`BM_Case1_AppWorkingSet48KiB_WithCachedTid`** | `48 KiB` L1D Touch + Ring 3 TLS | **220 ns** | 4.54 M ops/s | Application L1D working set stays 100% hot |
| **`BM_Case1_AppWorkingSet48KiB_WithRawSyscall`** | `48 KiB` L1D Touch + `1` `SYS_gettid` | 285 ns (**`80.8 ns` delta in `challenge`**) | 3.51 M ops/s | **2.15x Worse Effective Syscall Cost (L1D/BTB eviction)** |
| **`BM_Case2_Mmap2MiB_VirtualAllocOnly`** | `mmap(2 MiB)` (`512` $\times$ `4 KiB` pages) | **0.72 µs (`0.85 µs` w/ `munmap`)** | 1.18 M ops/s | **Virtual IOU (`vm_area_struct` only, `0` bytes RAM wired)** |
| **`BM_Case2_Mmap2MiB_FirstWritePass_512PageFaults`** | 1st Write Pass (`512` $\times$ `4 KiB` pages) | **198.4 µs (`198,392 ns`)** | 2.58 M pages/s | **60.6x Slower Than Pool (`512` `#PF` Traps @ `387.5 ns/page`)** |
| **`BM_Case2_PrefaultedArena2MiB_SecondWritePass`** | `MAP_POPULATE` + `mlock` Ring 3 Pool | **3.27 µs (`6.39 ns/page`)** | **156.49 M pages/s** | **60.6x Faster Than 1st-Touch (`0` `#PF` Traps) 🏆** |
| **`BM_Case2_TempStdVector64KiB_RecycledHeap`** | `std::vector<uint8_t>(64 KiB)` (`< 128 KiB`) | **316 ns** | 3.16 M ops/s | **`0` Minor Faults/Call (Recycled in Ring 3 `glibc` Heap)** |
| **`BM_Case2_TempStdVector2MiB_PerCallMmapChurn`** | `std::vector<uint8_t>(2 MiB)` (`>= 128 KiB`) | 278.5 µs | 3.59 K ops/s | **`M_MMAP_THRESHOLD`: `mmap` + `513` `#PF`s + `munmap` Teardown** |
| **`BM_Case2_ReusedStdVector2MiB_ZeroPageFaults`** | Reused `std::vector(2 MiB)` (`.clear()`) | **18.1 µs** | **55.2 K ops/s** | **15.4x Faster Than Per-Call `vector(2 MiB)` (`0` `#PF`s) 🚀** |
| **`BM_Case3_L3Resident32MiB_4KiBPages_TLBThrash`** | `32 MiB` (`8,192` $\times$ `4 KiB` pages) | 7.67–9.09 ns | 110.1–130.4 M/s | **Fits in `64 MiB` L3, but overflows `12 MiB` L2 dTLB (`50.9%` miss)** |
| **`BM_Case3_L3Resident32MiB_2MiBHugePages_TLBHit`** | `32 MiB` (`16` $\times$ `2 MiB` HugePages) | **7.02–7.05 ns** | **141.8–142.5 M/s** | **1.09x–1.29x Faster (`dTLB` misses: `10.24M` $\rightarrow$ `15,546`, `-99.85%`) 🚀** |
| **`BM_Case3_OrderBook512MiB_4KiBPages_TLBThrash`** | `512 MiB` (`131,072` $\times$ `4 KiB` pages) | 15.2–16.2 ns (`24.9 ns` `challenge`) | 61.9–65.8 M/s | **`93.4%` `dTLB` Miss Rate (`13.87M` 4-level page walks)** |
| **`BM_Case3_OrderBook512MiB_2MiBHugePages_TLBHit`** | `512 MiB` (`256` $\times$ `2 MiB` HugePages) | **8.66–9.79 ns (`13.6 ns` `challenge`)** | **102.1–115.5 M/s** | **1.55x–1.87x Faster (`dTLB` misses: `13.87M` $\rightarrow$ `28,560`, `-99.79%`) 🏆** |

---

### Hardware PMU Counter Summary (`perf stat` on AMD Ryzen 9 9950X)

#### 1. Demand Paging `#PF` Traps vs. Pre-Faulted Ring 3 Pool (`2 MiB` Buffer Writes)
- **1st Write Pass (`BM_Case2_Mmap2MiB_FirstWritePass_512PageFaults`):** `2,166,575 minor-faults` (`512` `#PF` traps/iter), `5,087,823,166 cycles`, `86.0%` of wall-clock time spent inside Ring 0 kernel mode (`0.915s sys` / `1.065s elapsed`).
- **Pre-Faulted `MAP_POPULATE` Pool (`BM_Case2_PrefaultedArena2MiB_SecondWritePass`):** `0` runtime `#PF` traps in timed loop (`60.6x` faster per pass, `99.7%` user-space execution).

#### 2. `512 MiB` Order Book Random Lookups (`4 KiB` Standard Pages vs. `2 MiB` HugePages)
- **`4 KiB` Standard Pages (`BM_Case3_OrderBook512MiB_4KiBPages_TLBThrash`, `MADV_NOHUGEPAGE`):** `13,866,978 dTLB-load-misses` / `14,851,761 dTLB-loads` (**`93.37%` `dTLB` miss rate**), `8,496,673,503 cycles` (`15.2–16.2 ns/lookup`).
- **`2 MiB` HugePages (`BM_Case3_OrderBook512MiB_2MiBHugePages_TLBHit`, `MADV_HUGEPAGE`):** `28,560 dTLB-load-misses` / `10,690,335 dTLB-loads` (**`0.27%` `dTLB` miss rate — `-99.79%` reduction!**), `5,003,463,847 cycles` (`8.66–9.79 ns/lookup` — **`1.55x–1.87x` faster**).

---

## 🛠️ Reproduction Commands

```bash
# 1. Run unit tests (vDSO/TLS telemetry, #PF minor-fault verification, PrefaultedArenaPool, OrderBookArena)
bazel test -c opt //common/syscalls_traps_and_mmu:syscalls_traps_and_mmu_test

# 2. Run Google Benchmark suite
bazel run -c opt //episodes/high_performance/11_syscalls_traps_and_mmu:benchmark

# 3. Run 3-Part Interactive OS Boundary & Hardware MMU Verification Lab
bazel run -c opt //episodes/high_performance/11_syscalls_traps_and_mmu:challenge
```
