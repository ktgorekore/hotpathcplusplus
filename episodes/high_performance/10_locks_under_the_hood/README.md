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

# 🎬 Episode 10: Locks Under the Hood — Why `std::shared_mutex` Is Slower Than `std::mutex`, Seqlocks, `futex` & 64-Shard Maps

> **Subtitle:** What Locks Actually Compile To: Why `std::shared_mutex` Causes MESI RFO Contention, Wait-Free Seqlocks (`41.8x`), TTAS + `_mm_pause()`, 3-State Linux `SYS_futex`, 32-Thread Oversubscription, and 64-Shard Hash Maps (`12.3x`–`17.3x`)  
> **Live Compiler Explorer (Godbolt) Disassembly:** [https://godbolt.org/z/vxdcdefxd](https://godbolt.org/z/vxdcdefxd)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.3.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)

---

## 📊 Empirical Silicon & Hardware PMU Telemetry (AMD Ryzen 9 9950X Zen 5)

All benchmarks, hardware PMU counter traces (`perf stat`), and multi-core contention challenges are measured using standard C++20 on bare-metal **AMD Ryzen 9 9950X** silicon (16 Cores, 32 Threads @ 5.4 GHz Boost, 48 KiB L1D, 1 MiB L2, 2x 32 MiB L3 CCDs, `perf_event_paranoid = -1`) with GCC `-O3 -std=c++20 -march=native`:

| Benchmark / Production Workload | Threads / Config | Measured Latency | Aggregate Throughput | Architectural Root Cause (`perf stat` PMU) |
| :--- | :---: | :---: | :---: | :--- |
| **Workload A: `SharedMutex_99R1W`** | 16 Threads (99% Read / 1% Write) | 1,037 ns (`64.8 ns/op` agg) | 15.43 M ops/s | **Reader-Count RMW Contention (`lock_shared()` writes on every read)** |
| **Workload A: `ExclusiveMutex_99R1W`** | 16 Threads (99% Read / 1% Write) | 607 ns (`37.9 ns/op` agg) | 26.37 M ops/s | **1.71x Faster Than `std::shared_mutex`** |
| **Workload A: `Seqlock_99R1W`** | 16 Threads (99% Read / 1% Write) | **24.8 ns (`1.55 ns/op` agg)** | **645.60 M ops/s** | **41.8x Faster (`Shared S` Loads, `0` Reader RMWs) 🚀** |
| `BM_Lock_Uncontended/TTASPauseLock` | 1 Thread | **4.85 ns/op** | 206.21 M ops/s | Single `lock cmpxchg` in local L1D (`Modified M`) |
| `BM_Lock_Uncontended/RawFutexMutex` | 1 Thread | **8.51 ns/op** | 117.56 M ops/s | `0 -> 1 -> 0` in Ring 3 Userspace (**0 Syscalls**) |
| `BM_Lock_Uncontended/Cpp20AtomicWaitLock` | 1 Thread | **8.56 ns/op** | 116.86 M ops/s | C++20 `std::atomic::wait` Userspace Fast Path |
| `BM_Lock_Uncontended/StdMutex` | 1 Thread | **10.0 ns/op** | 99.64 M ops/s | `pthread_mutex_lock` Fast Path (**4.9x < Syscall**) |
| `BM_RawSyscall_Futex` | 1 Thread (`SYS_futex`) | 48.80 ns/op | 20.50 M ops/s | Ring 3 $\rightarrow$ Ring 0 Syscall Baseline |
| **Workload B: `NaiveTASLock`** | 16 Threads (`lock xchg`) | 3,082 ns (`192.6 ns/op` agg) | 5.19 M ops/s | **2.99x Slower Than `std::mutex`** (`0.017 IPC`, `76.5%` cache misses) |
| **Workload B: `TTASNoPauseLock`** | 16 Threads (No `PAUSE`) | 1,297 ns (`81.1 ns/op` agg) | 12.34 M ops/s | Spins in `[S]` but retires `4.12B` spin instructions (`0.83 IPC`) |
| **Workload B: `TTASPauseLock`** | 16 Threads (`Shared S` + `PAUSE`) | **2,046 ns (`127.9 ns/op` agg)** | **7.82 M ops/s** | **1.51x–1.84x Faster Than Naive TAS (`201M` spin instrs — 20x fewer)** |
| **Workload B: `AdaptiveFutexMutex`** | 16 Threads (Spin + `futex`) | 987 ns (`61.7 ns/op` agg) | 16.22 M ops/s | Userspace TTAS Spin + Kernel Parking Safety Net |
| **Workload B: `StdMutex`** | 16 Threads (`futex`) | **1,031 ns (`64.4 ns/op` agg)** | **15.51 M ops/s** | **2.99x Faster Than Naive TAS Spinlock** |
| **Workload B: `ShardedHashMap_64Shards`** | 16 Threads (`alignas(64)` Shards) | **250 ns (`15.6 ns/op` agg)** | **63.99 M ops/s** | **12.3x Faster Than Naive TAS & 4.1x > `std::mutex` 🚀** |
| **32T Oversubscribed: `NaiveTASLock`** | 32 Threads on 16 Cores | 9,653 ns (`301.7 ns/op` agg) | 3.31 M ops/s | RFO Contention + Preempted Lock Holder |
| **32T Oversubscribed: `TTASPauseLock`** | 32 Threads on 16 Cores | 7,729 ns (`241.5 ns/op` agg) | 4.14 M ops/s | Active Cores Spin While Holder Is Preempted |
| **32T Oversubscribed: `StdMutex`** | 32 Threads on 16 Cores | **1,897 ns (`59.3 ns/op` agg)** | **16.87 M ops/s** | **4.1x Faster Under Oversubscription (`FUTEX_WAIT`)** |
| **32T Oversubscribed: `AdaptiveFutexMutex`** | 32 Threads on 16 Cores | **1,986 ns (`62.1 ns/op` agg)** | **16.11 M ops/s** | **3.9x Faster Under Oversubscription ✅** |
| **32T Oversubscribed: `ShardedHashMap_64Shards`** | 32 Threads on 16 Cores | **558 ns (`17.4 ns/op` agg)** | **57.31 M ops/s** | **13.8x Faster Than `TTASPauseLock` & 17.3x > `NaiveTASLock` 🏆** |
| `challenge`: Part 1 Routing Cache (16T) | `SharedMutex` vs `Mutex` vs `Seqlock` | `123.47 ns` $\rightarrow$ `35.46 ns` $\rightarrow$ **`6.11 ns`** | — | **3.48x `std::mutex` & 20.22x `Seqlock` Speedup 🚀** |
| `challenge`: Part 2 16T Lock Contention | `NaiveTAS` vs `TTAS+PAUSE` vs `64-Shard` | `309.99 ns` $\rightarrow$ `168.08 ns` $\rightarrow$ **`21.21 ns`** | — | **1.84x `TTAS+PAUSE` & 14.61x `64-Shard` Speedup ✅** |
| `challenge`: Part 3 32T Oversubscription | `TTASPauseLock` vs `Adaptive` vs `64-Shard` | `304.94 ns` $\rightarrow$ `74.06 ns` $\rightarrow$ **`21.67 ns`** | — | **4.12x `AdaptiveFutexMutex` & 14.07x `64-Shard` Speedup 🏆** |

### Hardware PMU Counter Summary (`perf stat` on AMD Ryzen 9 9950X)
- **Cycles:** `37,384,040,845`
- **Instructions Retired:** `23,853,792,821` (**`0.64` IPC**)
- **Cache References / Misses:** `193,012,844` references / `49,078,463` misses (**`25.43%` miss rate**)
- **Context Switches:** `148,672` (driven by 32-thread oversubscription and `SYS_futex` kernel parking)

---

## 🛠️ Reproduction Commands

```bash
# 1. Run unit tests (SeqlockRoutingCache, TTASPauseLock, RawFutexMutex, AdaptiveFutexMutex, ShardedHashMap)
bazel test -c opt //common/locks_under_the_hood:locks_under_the_hood_test

# 2. Run Google Benchmark suite
bazel run -c opt //episodes/high_performance/10_locks_under_the_hood:benchmark

# 3. Run 3-Part Production Lock & Sharding Engineering Challenge
bazel run -c opt //episodes/high_performance/10_locks_under_the_hood:challenge
```
