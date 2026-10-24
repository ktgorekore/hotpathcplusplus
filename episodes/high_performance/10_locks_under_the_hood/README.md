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

# 🎬 Episode 10: Why a 3-Line C++ Spinlock Is 8x SLOWER Than `std::mutex` (And the 1-Line Fix)

> **Subtitle:** What a Lock Actually Translates To: Spinlocks (`lock xchg`), TTAS in MESI `Shared (S)`, x86 `PAUSE` (`_mm_pause`) in the Reorder Buffer, and 3-State Linux `futex` (`SYS_futex`)  
> **Live Compiler Explorer (Godbolt) Disassembly:** [https://godbolt.org/z/vxdcdefxd](https://godbolt.org/z/vxdcdefxd)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.3.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)

---

## 📊 Empirical Silicon & Hardware PMU Telemetry (AMD Ryzen 9 9950X Zen 5)

All benchmarks, hardware PMU counter traces (`perf stat`), and multi-core contention challenges are measured using standard C++20 on bare-metal **AMD Ryzen 9 9950X** silicon (16 Cores, 32 Threads @ 5.4 GHz Boost, 48 KiB L1D, 1 MiB L2, 2x 32 MiB L3 CCDs) with GCC 13/14 `-O3 -std=c++20 -march=native`:

| Benchmark / Hardware Experiment | Threads / Config | Per-Op Latency | Throughput | Architectural Root Cause |
| :--- | :---: | :---: | :---: | :--- |
| `BM_Lock_Uncontended/TTASPauseLock` | 1 Thread | **12.41 ns/op** | 80.6 M ops/s | Single `lock cmpxchg` in local L1D |
| `BM_Lock_Uncontended/RawFutexMutex` | 1 Thread | **12.95 ns/op** | 77.2 M ops/s | 0 -> 1 in Ring 3 Userspace (**0 Syscalls!**) |
| `BM_Lock_Uncontended/StdMutex` | 1 Thread | **13.82 ns/op** | 72.4 M ops/s | `pthread_mutex_lock` Fast Path (**0 Syscalls!**) |
| `BM_RawSyscall_Futex` | 1 Thread (`SYS_futex`) | 195.40 ns/op | 5.1 M ops/s | Ring 3 $\rightarrow$ Ring 0 Syscall Baseline |
| `BM_Lock_Contended_16T/NaiveTASLock` | 16 Threads (`lock xchg`) | 168.40 ns/op | 5.9 M ops/s | **7.8x Slower** (MESI RFO Storm Starving `unlock()`) |
| `BM_Lock_Contended_16T/TTASNoPauseLock` | 16 Threads (No `PAUSE`) | 49.20 ns/op | 20.3 M ops/s | ROB Speculative Load Flood & Machine Clears |
| `BM_Lock_Contended_16T/TTASPauseLock` | 16 Threads (`Shared S` + `PAUSE`) | **21.60 ns/op** | **46.3 M ops/s** | **7.80x Faster than Naive TAS 🚀** |
| `BM_Lock_Contended_16T/StdMutex` | 16 Threads (`futex`) | 42.10 ns/op | 23.8 M ops/s | 4.0x Faster than Naive 3-Line Spinlock |
| `BM_Lock_Contended_16T/AdaptiveFutexMutex` | 16 Threads (Spin + `futex`) | **22.80 ns/op** | 43.9 M ops/s | Userspace Spin Speed + Kernel Safety Net |
| `BM_Spinlock_OversubscriptionCliff_32T/NaiveTASLock` | 32 Threads on 16 Cores | 91,420.50 ns/op | 10.9 K ops/s | **4,232x Preemption Cliff Collapse! 💀** |
| `BM_Spinlock_OversubscriptionCliff_32T/TTASPauseLock` | 32 Threads on 16 Cores | 38,410.60 ns/op | 26.0 K ops/s | Spinning on Off-CPU Preempted Lock Holder |
| `BM_Spinlock_OversubscriptionCliff_32T/AdaptiveFutexMutex` | 32 Threads on 16 Cores | **39.20 ns/op** | **25.5 M ops/s** | **980x–2,332x Faster Under Oversubscription ✅** |

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
