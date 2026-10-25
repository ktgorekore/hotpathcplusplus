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

# 🎬 Episode 10: Why a 3-Line C++ Spinlock Is 3.3x SLOWER Than `std::mutex` (And the 1-Line Fix)

> **Subtitle:** What a Lock Actually Translates To: Spinlocks (`lock xchg`), TTAS in MESI `Shared (S)`, x86 `PAUSE` (`_mm_pause`) in the Reorder Buffer, and 3-State Linux `futex` (`SYS_futex`)  
> **Live Compiler Explorer (Godbolt) Disassembly:** [https://godbolt.org/z/vxdcdefxd](https://godbolt.org/z/vxdcdefxd)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.3.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)

---

## 📊 Empirical Silicon & Hardware PMU Telemetry (AMD Ryzen 9 9950X Zen 5)

All benchmarks, hardware PMU counter traces (`perf stat`), and multi-core contention challenges are measured using standard C++20 on bare-metal **AMD Ryzen 9 9950X** silicon (16 Cores, 32 Threads @ 5.4 GHz Boost, 48 KiB L1D, 1 MiB L2, 2x 32 MiB L3 CCDs) with GCC 13/14 `-O3 -std=c++20 -march=native`:

| Benchmark / Hardware Experiment | Threads / Config | Measured Latency | Throughput | Architectural Root Cause (`perf stat` PMU) |
| :--- | :---: | :---: | :---: | :--- |
| `BM_Lock_Uncontended/TTASPauseLock` | 1 Thread | **4.60 ns/op** | 217.5 M ops/s | Single `xchg` in local L1D |
| `BM_Lock_Uncontended/RawFutexMutex` | 1 Thread | **7.87 ns/op** | 127.1 M ops/s | 0 -> 1 in Ring 3 Userspace (**0 Syscalls!**) |
| `BM_Lock_Uncontended/StdMutex` | 1 Thread | **9.24 ns/op** | 108.3 M ops/s | `pthread_mutex_lock` Fast Path (**5.0x < Syscall!**) |
| `BM_RawSyscall_Futex` | 1 Thread (`SYS_futex`) | 46.50 ns/op | 21.5 M ops/s | Ring 3 $\rightarrow$ Ring 0 Syscall Baseline |
| `BM_Lock_Contended_16T/NaiveTASLock` | 16 Threads (`lock xchg`) | 3,449 ns wait (`215.6 ns/op`) | 4.64 M ops/s | **3.28x Slower** (`0.03 IPC`, `3.92M` L1D RFO Misses) |
| `BM_Lock_Contended_16T/TTASNoPauseLock` | 16 Threads (No `PAUSE`) | 1,276 ns wait (`79.7 ns/op`) | 12.54 M ops/s | `2.10B` Instrs, `805M` Loads, `5.81M` Pipeline Resyncs |
| `BM_Lock_Contended_16T/TTASPauseLock` | 16 Threads (`Shared S` + `PAUSE`) | **1,239 ns wait (`77.4 ns/op`)** | **12.92 M ops/s** | **2.78x Faster than Naive TAS, 4.79x Fewer Instrs (`438M`) 🚀** |
| `BM_Lock_Contended_16T/AdaptiveFutexMutex` | 16 Threads (Spin + `futex`) | 1,261 ns wait (`78.8 ns/op`) | 12.69 M ops/s | Userspace Spin Speed + Kernel Safety Net |
| `BM_Lock_Contended_16T/StdMutex` | 16 Threads (`futex`) | **1,053 ns wait (`65.8 ns/op`)** | **15.19 M ops/s** | **3.28x Faster than Naive 3-Line Spinlock** |
| `BM_Spinlock_OversubscriptionCliff_32T/NaiveTASLock` | 32 Threads on 16 Cores | 960,057 ns wait (`30.0 us/op`) | 33.3 K ops/s | **250x Preemption Cliff Collapse! 💀** |
| `BM_Spinlock_OversubscriptionCliff_32T/TTASPauseLock` | 32 Threads on 16 Cores | 1,470,014 ns wait (`45.9 us/op`) | 21.8 K ops/s | **1.47 ms Preemption Cliff** (Spinning on Off-CPU Holder) |
| `BM_Spinlock_OversubscriptionCliff_32T/StdMutex` | 32 Threads on 16 Cores | 5,197 ns wait (`162.4 ns/op`) | 6.16 M ops/s | **283x Faster Under Oversubscription** |
| `BM_Spinlock_OversubscriptionCliff_32T/AdaptiveFutexMutex` | 32 Threads on 16 Cores | **3,841 ns wait (`120.0 ns/op`)** | **8.33 M ops/s** | **383x Faster Under Oversubscription ✅** |

---

## 🛠️ Reproduction Commands

```bash
# 1. Run unit tests
bazel test //common/locks_under_the_hood:locks_under_the_hood_test

# 2. Run Google Benchmark suite
bazel run -c opt //episodes/high_performance/10_locks_under_the_hood:benchmark

# 3. Run Two-Part Lock Engineering Challenge (16-Core RFO Storm & 32-Thread Preemption Cliff)
bazel run -c opt //episodes/high_performance/10_locks_under_the_hood:challenge
```
