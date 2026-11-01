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

> **Subtitle:** Crossing the Ring 0 Silicon Border: Ring 3 `vDSO` Seqlocks (`18.0 ns` vs `68.9 ns` Syscall), `%fs` Thread-Local Storage (`199x`), Optimistic Demand Paging & `#PF` Interrupt Vector 14 (`0.69 µs` Alloc vs `217.6 µs` First Write $\rightarrow$ `197 ns` Pool, `1,105x` Faster), 4-Level Radix Page Walks, and `2 MiB` HugePages (`MADV_HUGEPAGE`, `-99.99%` `dTLB` Misses, `2.14x–2.45x` Faster)  
> **Live Compiler Explorer (Godbolt) Disassembly:** [https://godbolt.org/z/W9KETqadE](https://godbolt.org/z/W9KETqadE)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.3.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)

---

## 📊 Empirical Silicon & Hardware PMU Telemetry (AMD Ryzen 9 9950X Zen 5)

All benchmarks, hardware PMU counter traces (`perf stat`), and virtual-memory challenges are measured using standard C++20 on bare-metal **AMD Ryzen 9 9950X** silicon (16 Cores, 32 Threads @ 5.4 GHz Boost, 48 KiB L1D, 1 MiB L2, 64 MiB L3 [2x 32 MiB CCDs], `L1 dTLB = 96 entries`, `L2 dTLB = 3,072 entries = 12 MiB reach @ 4 KiB`) with GCC 13.3 `-O3 -std=c++20 -march=native`:

| Benchmark / Production Workload | Config / Page Granularity | Measured Latency | Throughput | Architectural Root Cause (`perf stat` PMU) |
| :--- | :---: | :---: | :---: | :--- |
| **Workload 1: `ClockGettime_vDSO`** | Ring 3 `[vdso]` + `[vvar]` Seqlock | **18.0 ns/op** | **55.69 M ops/s** | **0 Syscalls (`rdtscp` + Seqlock in Ring 3) 🚀** |
| **Workload 1: `ClockGettime_RawSyscall`** | Ring 0 `syscall(SYS_clock_gettime)` | 68.9 ns/op | 14.52 M ops/s | **3.83x Slower (`swapgs` + `pt_regs` + Ring 0 trap)** |
| **Workload 1: `Gettid_CachedTLS`** | Ring 3 `thread_local` (`%fs` segment) | **0.21 ns/op** | **4.77 G ops/s** | **199x Faster (`mov eax, fs:[offset]` L1D hit) 🚀** |
| **Workload 1: `Gettid_RawSyscall`** | Ring 0 `syscall(SYS_gettid)` | 41.7 ns/op | 23.96 M ops/s | Ring 3 $\rightarrow$ Ring 0 `entry_SYSCALL_64` transition |
| **Workload 1: `WorkingSet48KiB_Ring3Only`** | `48 KiB` L1D Touch + Ring 3 TLS | **670 ns** | 1.49 M ops/s | Application L1D working set stays 100% hot |
| **Workload 1: `WorkingSet48KiB_WithSyscall`** | `48 KiB` L1D Touch + `1` `SYS_gettid` | 796 ns (**`+126 ns` tax**) | 1.26 M ops/s | **3.0x Worse Effective Syscall Cost (L1D/BTB eviction)** |
| **Workload 2: `Mmap2MiB_AllocCallOnly`** | `mmap(2 MiB)` (`512` $\times$ `4 KiB` pages) | **0.69 µs (`686 ns`)** | 1.46 M ops/s | **Virtual IOU (`vm_area_struct` only, `0` bytes RAM wired)** |
| **Workload 2: `FirstTouch_512Pages_Trap`** | 1st Write Pass (`512` $\times$ `4 KiB` pages) | **217.6 µs (`217,610 ns`)** | 2.35 M pages/s | **317x Slower Than Alloc (`512` `#PF` Traps @ `425 ns/page`)** |
| **Workload 2: `GlibcVector2MiB_PerRequestChurn`** | `std::vector<uint8_t>(2 MiB)` (`>= 128 KiB`) | 329.3 µs (`643 ns/page`) | 1.55 M pages/s | **`M_MMAP_THRESHOLD`: `mmap` + `512` `#PF`s + `munmap` Teardown** |
| **Workload 2: `PrefaultedPool_512Pages_Resident`** | `MAP_POPULATE` + `mlock` Ring 3 Pool | **197 ns (`0.38 ns/store`)** | **2.60 G pages/s** | **1,105x Faster Than 1st-Touch & 1,671x > `vector` Churn 🏆** |
| **Workload 3: `OrderBook32MiB_4KiB_StandardPages`** | `32 MiB` (`8,192` $\times$ `4 KiB` pages) | 9.59 ns (`8.64 ns` `perf`) | 104.24 M/s | **Fits in `64 MiB` L3, but overflows `12 MiB` L2 dTLB (`70.9%` miss)** |
| **Workload 3: `OrderBook32MiB_2MiB_HugePages`** | `32 MiB` (`16` $\times$ `2 MiB` HugePages) | **4.93 ns (`4.55 ns` `perf`)** | **202.75 M/s** | **1.95x Faster (`dTLB` misses: `35.6M` $\rightarrow$ `2,184`, `-99.99%`) 🚀** |
| **Workload 3: `OrderBook512MiB_4KiB_StandardPages`** | `512 MiB` (`131,072` $\times$ `4 KiB` pages) | 29.9 ns (`33.97 ns` `perf`) | 33.41 M/s | **`93.9%` `dTLB` Miss Rate (`48.04M` 4-level page walks)** |
| **Workload 3: `OrderBook512MiB_2MiB_HugePages`** | `512 MiB` (`256` $\times$ `2 MiB` HugePages) | **14.0 ns (`13.88 ns` `perf`)** | **71.20 M/s** | **2.14x–2.45x Faster (`dTLB` misses: `48.04M` $\rightarrow$ `5,824`, `-99.99%`) 🏆** |

---

### Hardware PMU Counter Summary (`perf stat` on AMD Ryzen 9 9950X)

#### 1. Demand Paging `#PF` Traps vs. Pre-Faulted Ring 3 Pool (`1,000` Iterations of `2 MiB` Buffer Writes)
- **Lazy `mmap` + `munmap` (`./challenge fault_2m`):** `512,125 page-faults` (`512` `#PF` traps/iter), `1,180,290,489 cycles`, `88.5%` of wall-clock time spent inside Ring 0 kernel mode (`0.278s sys` / `0.314s elapsed`).
- **Pre-Faulted `MAP_POPULATE` Pool (`./challenge pool_2m`):** `124 page-faults` (**`0` runtime `#PF` traps**), `9,061,487 cycles` (**`130x` fewer total CPU cycles**, `0.0023s elapsed`).

#### 2. `512 MiB` Order Book Random Lookups (`50,000,000` Lookups: `4 KiB` Standard Pages vs. `2 MiB` HugePages)
- **`4 KiB` Standard Pages (`./challenge 4k_512m`, `MADV_NOHUGEPAGE`):** `48,036,179 dTLB-load-misses` / `51,143,087 dTLB-loads` (**`93.92%` `dTLB` miss rate**), `47,866,072 ls_l1_d_tlb_miss.all_l2_miss`, `9,324,842,927 cycles` (`33.97 ns/lookup`).
- **`2 MiB` HugePages (`./challenge 2m_512m`, `MADV_HUGEPAGE`):** `5,824 dTLB-load-misses` / `51,162,764 dTLB-loads` (**`0.011%` `dTLB` miss rate — `-99.99%` reduction!**), `31,720,410 ls_l1_d_tlb_miss.tlb_reload_2m_l2_hit`, `3,882,888,303 cycles` (`13.88 ns/lookup` — **`2.45x` faster**).

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
