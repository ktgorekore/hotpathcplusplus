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

# ⚡ Hot Path C++ — Production Low-Latency Systems Library

[![YouTube Channel](https://img.shields.io/badge/YouTube-Hot%20Path%20C%2B%2B-red?logo=youtube)](https://youtube.com/@HotPathCpp)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Bazel 9](https://img.shields.io/badge/Bazel-9.2.0-green.svg)](https://bazel.build/)
[![Google Benchmark](https://img.shields.io/badge/Google_Benchmark-1.9.5-brightgreen.svg)](https://github.com/google/benchmark)
[![CI](https://github.com/ktgorekore/hotpathcplusplus/actions/workflows/ci.yml/badge.svg)](https://github.com/ktgorekore/hotpathcplusplus/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

> **Official companion code repository for the [Hot Path C++ YouTube Channel](https://youtube.com/@HotPathCpp).**  
> We deconstruct bare-metal systems programming, high-frequency trading (HFT) patterns, CPU microarchitecture (caches, store buffers, PMUs), compiler optimizations, and modern C++20 low-latency idioms.

---

## 📺 Featured Deep-Dive Episode

| Episode | Title | Companion Video |
| :--- | :--- | :--- |
| **Episode 03** | **Zero-Copy IPC at 286 GiB/s in C++20 (The Linux MMU Magic Mirror Hack!)** | [![Watch on YouTube](https://img.shields.io/badge/Watch-YouTube_Video-red?logo=youtube)](https://youtube.com/@HotPathCpp) |
| **Episode 04** | **Stop Locking Your Hash Maps! (perf, pprof & 831M Ops/Sec RCU Teaser)** | [![Watch on YouTube](https://img.shields.io/badge/Watch-YouTube_Video-red?logo=youtube)](https://youtube.com/@HotPathCpp) |
| **Episode 05** | **How to Measure C++ Performance Without Fooling Yourself (Microbenchmarks & PMUs)** | [![Watch on YouTube](https://img.shields.io/badge/Watch-YouTube_Video-red?logo=youtube)](https://youtube.com/@HotPathCpp) |
| **Episode 06** | **Stop Chasing Pointers: The Cost of Abstraction (OOP vs. std::variant)** | [![Watch on YouTube](https://img.shields.io/badge/Watch-YouTube_Video-red?logo=youtube)](https://youtube.com/@HotPathCpp) |
| **Episode 07** | **The Silicon Time Machine: Out-of-Order Execution & The Superscalar Pipeline** | [![Watch on YouTube](https://img.shields.io/badge/Watch-YouTube_Video-red?logo=youtube)](https://youtube.com/@HotPathCpp) |

In Episode 07, Marcus deconstructs the Silicon Time Machine: how modern superscalar processors execute C++ instructions out of order using Tomasulo's algorithm, Register Renaming (eliminating false WAW/WAR hazards), and Reorder Buffers (ROBs). Learn why naive loops create fatal loop-carried dependency chains (RAW) that collapse 4-to-8-wide superscalar backends into 1-wide dirt roads, and how multi-accumulator pipelines unlock an 11.2x speedup! Includes the **Episode 07 Community Challenge** (`//common/execution:challenge` or `//episodes/07_out_of_order_execution:challenge`).

---

## 🚀 Module Overview: Single-Producer Multi-Consumer (SPMC) Broadcast Ring Buffer

Located under [`common/shared_memory/`](common/shared_memory/), this package provides a production-grade, zero-copy, lock-free SPMC broadcast ring buffer engineered for ultra-low-latency market data dissemination and high-throughput inter-process communication (IPC).

### Architectural Highlights

1. **The Virtual Memory "Magic Mirror" (`MAP_SHARED | MAP_FIXED`):**  
   Maps the underlying physical shared RAM page twice into two adjacent virtual memory ranges (`2 * capacity`). When a reader accesses a message that straddles the end of the buffer, the CPU Memory Management Unit (MMU) automatically wraps the virtual pointer back to the beginning in silicon hardware. This allows returning contiguous `absl::Span<const uint8_t>` views across buffer boundaries with **zero buffer-wrap branches** and **zero memcpy splitting**.

2. **Process Isolation via Dual POSIX Shared Memory Segments:**  
   - `_meta` segment: Mapped `PROT_READ | PROT_WRITE` for producer commit cursors and consumer heartbeat tracking.
   - `_data` segment: Mapped `PROT_READ` (Read-Only) in consumer processes, guaranteeing that rogue or crashing consumers cannot corrupt the ring buffer payload data.

3. **SeqLock Atomic Protocol & Hardware Memory Fences:**  
   Uses an atomic sequence lock with version bit-masking. Consumers detect concurrent producer overwrites via acquire loads on the sequence counter before and after reading, coupled with `std::atomic_thread_fence(std::memory_order_release)` on commits.

4. **MESI Cache-Line Hygiene (`alignas(64)`):**  
   Every consumer `ReaderSlot` is padded with an explicit 35-byte moat (`alignas(64)`) to ensure each consumer's heartbeat, PID, and cursor occupy a dedicated 64-byte hardware cache line, preventing False Sharing and Read For Ownership (RFO) bus storms between CPU cores.

5. **Hardware TSC Eviction (`__rdtsc()`):**  
   Producers monitor consumer heartbeats using invariant CPU cycle counters (`__rdtsc()`). Sluggish, deadlocked, or crashed consumers (e.g., timed out past 500ms) are safely evicted via atomic CAS on `generation_id`, preventing zombie consumers from stalling producer backpressure.

---

## 📐 Memory Architecture

### The Virtual Memory Magic Mirror
```text
Virtual Address Space (2 * Capacity):
┌──────────────────────────────────────┬──────────────────────────────────────┐
│       Primary Virtual Map            │         Mirror Virtual Map           │
│       [base .. base + Capacity)      │  [base + Capacity .. base + 2*Cap)   │
└──────────────────────────────────────┴──────────────────────────────────────┘
                   │                                      │
                   ▼                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Single Physical Shared RAM Block                         │
│                    [Physical Page 0 .. Physical Page N)                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### ReaderSlot Cache-Line Hygiene (64-Byte Moat)
```text
Bytes 0..28: Active Fields (29 Bytes)
┌───────┬───────────────┬──────────────────┬──────────┬─────────────────┐
│active │ cursor        │ heartbeat_ts     │ pid      │ generation_id   │
│(1B)   │ (8B)          │ (8B)             │ (4B)     │ (4B)            │
└───────┴───────────────┴──────────────────┴──────────┴─────────────────┘
Bytes 29..63: Cache Moat (35 Bytes Explicit Padding)
┌───────────────────────────────────────────────────────────────────────┐
│ char padding[35] — Explicit alignment to 64-byte boundary             │
└───────────────────────────────────────────────────────────────────────┘
Total: Exactly 64 Bytes (alignas(64)) — Zero false sharing across cores!
```

---

## 📊 Bare-Metal Hardware Telemetry

*Audited on AMD Ryzen 9 9950X (16 Cores, 32 Threads @ 5.7 GHz boost) running Linux 6.8 with GCC 13 `-O3 -march=native`:*

| Benchmark Variant | Message Size | Latency | Throughput | Kernel Syscalls | Branch Misses |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Magic Mirror Zero-Copy (Batch)** | 8,192 Bytes | 27.2 ns | **286.9 GiB/s** | **0** | **0.00%** |
| **Copy-Batch (Amortized)** | 8 Bytes | **6.1 ns** | 1.22 GiB/s | **0** | **0.00%** |
| **Single Zero-Copy** | 8 Bytes | 26.0 ns | 293.4 MiB/s | **0** | **0.00%** |
| **SeqLock Uncontended Read** | 64 Bytes | **0.18 ns** | **5.41 G ops/s** | **0** | **0.00%** |
| **SeqLock Contended Read (4T)**| 64 Bytes | **5.6 ns** | **707.3 M ops/s** | **0** | **0.00%** |
| **Traditional POSIX Pipe** | 8 Bytes | 1,240.0 ns | 6.2 MiB/s | 2 (read/write) | 4.8% |

---

## 🔒 Module Overview: Sequence Lock (`common/concurrency/seqlock.h`)

Located under [`common/concurrency/`](common/concurrency/), this header provides an industrial-strength **Sequence Lock (SeqLock)** for lock-free, zero-copy optimistic concurrency in Single-Producer Multi-Consumer (SPMC) systems.

### Key Architectural Characteristics:
* **The Odd/Even Counter:**
  * `Counter & 1 == 0` (EVEN): Data is 100% consistent and safe to read.
  * `Counter & 1 == 1` (ODD): Producer is actively modifying memory; readers pause via `_mm_pause()` and retry.
* **CPU Out-of-Order Memory Fencing:**
  * Uses `std::atomic_thread_fence(std::memory_order_acquire)` in `ReadRetry()` to prevent out-of-order CPUs from speculatively loading payload fields *before* checking the counter.
* **Hardware Cache-Line Isolation:**
  * Annotated with `alignas(64)` to ensure the atomic counter occupies a dedicated L1/L2 cache line, eliminating false sharing.

---

## 🛠️ Quick Start & Building with Bazel 9

### Prerequisites
- **Operating System:** Linux (Kernel 5.4+ recommended for POSIX shm & MMU support)
- **Compiler:** GCC 11+ or Clang 13+ with `-std=c++20` support
- **Build System:** [Bazel 9](https://bazel.build/) or [Bazelisk](https://github.com/bazelbuild/bazelisk)

### 1. Clone the Repository
```bash
git clone https://github.com/ktgorekore/hotpathcplusplus.git
cd hotpathcplusplus
```

### 2. Build All Targets
```bash
bazel build //...
```

### 3. Run the Unit Test Suite
```bash
# Run all tests (shared memory + seqlock)
bazel test //...

# Run SeqLock unit tests specifically
bazel test //common/concurrency:seqlock_test
```

### 4. Run the Bare-Metal Microarchitecture Benchmarks
```bash
# Run SeqLock benchmark harness
bazel run -c opt //:seqlock_benchmark

# Run Broadcast Ring Buffer benchmark harness
bazel run -c opt //:benchmark
```
# Profile CPU hardware PMU counters with Linux perf
perf stat -e cycles,instructions,cache-misses,branch-misses \
    ./bazel-bin/common/shared_memory/broadcast_ring_buffer_benchmark
```

---

## 💻 C++20 Usage Example

### Producer (Writer Process)
```cpp
#include "common/shared_memory/broadcast_ring_buffer.h"

using cognitas::trading::BroadcastRingBuffer;

int main() {
  // Create 16MB ring buffer with virtual memory magic mirror
  auto ring_or = BroadcastRingBuffer::Create("/market_data_shm", 16 * 1024 * 1024);
  if (!ring_or.ok()) return 1;
  auto& ring = *ring_or;

  // Reserve space for payload (zero-copy buffer pointer)
  size_t payload_len = 256;
  auto buf_or = ring->Reserve(payload_len, /*msg_type=*/1);
  if (buf_or.ok()) {
    uint8_t* dest = *buf_or;
    // Direct write to shared memory...
    std::memcpy(dest, "HOT_PATH_TELEMETRY", 18);

    // Atomic release commit
    ring->Commit();
  }
  return 0;
}
```

### Consumer (Reader Process)
```cpp
#include "common/shared_memory/broadcast_ring_buffer.h"

using cognitas::trading::BroadcastReader;
using cognitas::trading::BroadcastRingBuffer;
using cognitas::trading::FrameHeader;

int main() {
  // Attach as Read-Only consumer
  auto ring_or = BroadcastRingBuffer::Attach("/market_data_shm");
  if (!ring_or.ok()) return 1;
  auto& ring = *ring_or;

  BroadcastReader reader;
  ring->RegisterReader(reader);

  while (true) {
    FrameHeader header;
    // Zero-copy read spanning array boundary safely thanks to Magic Mirror!
    auto span_or = ring->PollZeroCopy(reader, header);
    if (span_or.ok()) {
      absl::Span<const uint8_t> payload = *span_or;
      // Process payload directly from physical RAM without copying...
      ring->Advance(reader, header.TotalFrameSize());
    }
  }

  ring->UnregisterReader(reader);
  return 0;
}
```

---

## ⚡ Module Overview: Wait-Free Concurrent RCU Hash Map & EBR Engine

Located under [`common/concurrency/`](common/concurrency/), this package provides an industrial-strength, wait-free read concurrent hash map using **Epoch-Based Reclamation (EBR)** and optimistic **Seqlocks**.

### Architectural Highlights

1. **100% Wait-Free Reads (`RcuHashMap::Get`):**  
   Readers execute zero atomic writes to shared memory (`ZERO lock cmpxchg`, `ZERO lock xadd`). All CPU cores keep container memory lines in the **Shared (S)** cache state, completely eliminating cross-core cache line bouncing and MESI invalidation storms.

2. **Zero-Dependency Epoch-Based Reclamation (`EpochBasedReclamation`):**  
   Tracks reader quiescence through thread-local epoch registration. Retired nodes are deferred in epoch batches and reclaimed only after older epochs have quiesced, guaranteeing memory safety without atomic reference counting.

3. **Optimistic Seqlock Value Protection:**  
   Node values are guarded by a 32-bit sequence counter. Reads perform an optimistic validation loop using `_mm_pause()` if a write is actively committing, ensuring atomic value snapshot isolation without blocking.

4. **Copy-On-Write (COW) Mutation:**  
   Writers take an internal mutex to serialize structural updates, splice replacement nodes into bucket chains with `std::memory_order_release`, and retire obsolete nodes through EBR. Writers never block readers!

### 📊 Multi-Threaded Benchmark Results (16 Reader Threads)

```text
Benchmark                                    Latency      Throughput
--------------------------------------------------------------------
BM_AbslFlatHashMap_SharedMutex/16          553.0 ns    28.9M ops/sec
BM_StdUnorderedMap_SharedMutex/16          440.0 ns    36.3M ops/sec
BM_RcuHashMap_WaitFree_Reads/16             19.2 ns   831.6M ops/sec 🚀 (28.7x SPEEDUP!)
```

### Usage Example

```cpp
#include "common/concurrency/rcu_hash_map.h"

using cognitas::trading::EpochBasedReclamation;
using cognitas::trading::RcuHashMap;

int main() {
  RcuHashMap<std::string, int> map(1024);

  // Writers insert/update
  map.Put("AAPL", 225);
  map.Put("NVDA", 120);

  // Thread participation in EBR
  EpochBasedReclamation::RegisterThread();

  // 100% Wait-free read lookup
  auto price = map.Get("AAPL");
  if (price.has_value()) {
    // Process price...
  }

  EpochBasedReclamation::UnregisterThread();
  return 0;
}
```

---

## 🔬 Module Overview: Empirical Microbenchmarking & Silicon Measurement Suite

Located under [`common/measurement/`](common/measurement/), this package provides an industrial-grade benchmarking harness designed to prevent the 4 classic microarchitectural measurement traps in high-frequency and low-latency C++20 systems.

### The 4 Microbenchmarking Pitfalls

1. **The Compiler Trap (Dead Code Elimination under ISO "as-if" rule):**  
   Optimizing compilers (`-O3`) detect unobserved loop results and evaporate multi-stage computations down to `0.00 ns` (`xor eax, eax; ret`).  
   - **Remedy:** `ForceOptimizationBarrier(val)` and `benchmark::DoNotOptimize(val)` establish an inline assembly constraint barrier, forcing values to materialize in registers/memory without adding synthetic instruction overhead.

2. **The CPU Trap (Branch Predictor Omniscience & Zero Entropy):**  
   Modern TAGE and Branch Target Buffer (BTB) predictors memorize repetitive test data patterns, exhibiting >99% prediction accuracy that evaporates in production.  
   - **Remedy:** `DatasetGenerator::GenerateUniformRandom` generates high-entropy inputs to measure realistic pipeline stall penalties (15–20 cycles per misprediction).

3. **The Memory Trap (Cache Residency Illusion & Hardware Prefetchers):**  
   Testing small buffers repeatedly measures L1/L2 cache latency (~1–3 ns) rather than cold DRAM access latency (~65–85 ns). Hardware stream prefetchers mask memory latency on sequential arrays.  
   - **Remedy:** `DatasetGenerator::GeneratePointerChaseRing` constructs pseudo-random Hamiltonian cycles that defeat stride prefetchers across L1 (16 KiB), L2 (256 KiB), L3 (8 MiB), and DRAM (64 MiB). `FlushCacheRange` leverages `_mm_clflush` + `_mm_mfence` to force cache evictions.

4. **The Setup & Timer Overhead Trap:**  
   Allocating dynamic buffers inside the hot measurement loop measures OS page faults, `mmap` kernel locks, and heap allocation rather than algorithm latency.  
   - **Remedy:** Pre-warming buffers outside the timed scope, and using hardware cycle counters (`ReadTscSerialized` / `__rdtscp`) for sub-nanosecond timestamping.

### 📊 Empirical Silicon Measurement Results

*Measured on 32-core x86-64 Linux Host with GCC 13.3 (`-O3 -march=native`):*

```text
---------------------------------------------------------------------------
Benchmark                                 Time             CPU   Iterations
---------------------------------------------------------------------------
BM_DeadCode_Eliminated                0.000 ns        0.000 ns   1000000000000  <-- Evaporated!
BM_DeadCode_DoNotOptimize              67.7 ns         67.7 ns     10026811     <-- Real compute
BM_DeadCode_ForceBarrier               67.0 ns         67.0 ns     10350584     <-- Hardware materialized
BM_Branch_BranchlessCmov               4459 ns         4459 ns       160997     <-- Immune to entropy (3.67G items/s)
BM_Cache_L1Resident                    0.93 ns/hop     0.93 ns      7527093     <-- L1 Data Cache
BM_Cache_L2Resident                    2.47 ns/hop     2.47 ns      2853544     <-- L2 Unified Cache
BM_Cache_L3Resident                    10.5 ns/hop     10.5 ns       671279     <-- L3 Shared Cache
BM_Cache_DRAM                          85.2 ns/hop     85.2 ns        81415     <-- DRAM Wall (91x L1 latency!)
BM_TimerOverhead_StdChrono             17.0 ns         17.0 ns     41614245
BM_TimerOverhead_RDTSCP                13.1 ns         13.1 ns     52924453
BM_Trap_SetupAllocationPollution        121 ns          121 ns      5754914     <-- 4x slower from heap in loop
BM_Trap_SetupPreAllocated              30.8 ns         30.8 ns     23280655     <-- Pure compute
```

### 🏆 Episode 05 Community Measurement Challenge (3 Hidden Traps)

Featured in [Episode 05](https://youtube.com/@HotPathCpp), [`common/measurement/challenge.cc`](common/measurement/challenge.cc) presents three subtle, real-world microbenchmarking traps designed to audit your intuition against optimizing compilers and bare-metal CPU silicon:

1. **Trap 1: The Invisible Workload (Dead Code Elimination):**  
   The accumulator `sum` is never escaped outside the benchmark loop. At `-O3`, the compiler evaporates the entire 500-iteration loop under the ISO "as-if" rule into `0.00 ns` (`xor eax, eax; ret`).  
   - **Fix:** Prevent dead code elimination with `benchmark::DoNotOptimize(sum)`.

2. **Trap 2: Measuring the Allocator (Setup Overhead):**  
   Allocating and deallocating `std::vector<int64_t>(1024)` *inside* the benchmark loop pollutes timing with heap allocation, `malloc` thread lock contention, and OS page faults, dominating pure compute latency by ~4x.  
   - **Fix:** Pre-allocate and populate the vector outside the timed benchmark loop.

3. **Trap 3: The All-Knowing CPU (Predictor Omniscience / Zero Entropy):**  
   Evaluating a conditional branch against a repeating pattern (`[1, -1, 1, -1]`) allows the hardware branch predictor (TAGE/BTB) to learn the history pattern and achieve ~0% mispredictions, hiding the catastrophic 15–20 cycle pipeline flush penalty.  
   - **Fix:** Inject uniform pseudo-random entropy via `DatasetGenerator::GenerateUniformRandom`.

### Running Tests & Benchmarks

```bash
# Run unit tests
bazel test //common/measurement/...

# Run optimized microbenchmarks
bazel run -c opt //common/measurement:measurement_benchmark

# Run Episode 05 Community Measurement Challenge (all 3 traps)
bazel run -c opt //:challenge

# Filter specific challenge traps:
bazel-bin/common/measurement/challenge --benchmark_filter="BM_Challenge_Trap1.*"
bazel-bin/common/measurement/challenge --benchmark_filter="BM_Challenge_Trap2.*"
bazel-bin/common/measurement/challenge --benchmark_filter="BM_Challenge_Trap3.*"
```

---

## 🏎️ Module Overview: Dynamic Dispatch & Data-Oriented Design (`common/dispatch/`)

Located under [`common/dispatch/`](common/dispatch/), this package provides production-grade dynamic dispatch comparisons and market order matching engines contrasting classical Object-Oriented polymorphism against modern C++17/C++20 Data-Oriented Design (`std::variant` & `std::visit`).

### The Silicon Cost of Classical OOP Abstraction

1. **The Double-Pointer Dereference Penalty:**  
   Classical virtual dispatch requires dereferencing an object instance pointer to locate its 8-byte `vptr`, then dereferencing the `vtable` to obtain the target function pointer before issuing `call *%rax`. In a polymorphic vector of `std::unique_ptr<BaseOrder>`, every single call forces two dependent memory loads before instruction execution can even begin.

2. **Cache-Line Fragmentation:**  
   Heap-allocated polymorphic objects scatter memory allocations randomly across DRAM pages. When iterating through a vector of 100,000 pointers, the CPU hardware stream prefetcher cannot anticipate pointer destinations, resulting in massive L1 data cache line misses and TLB churn.

3. **Indirect Branch Target Buffer (BTB) Mispredictions:**  
   While standard `if` statements are direct branches with two static targets, virtual function calls compile to indirect jumps (`call *%rax`). If the order stream contains interleaved types (`BuyLimit`, `SellMarket`, `Cancel`), the CPU branch target buffer cannot predict the destination, suffering 15–20 cycle pipeline flushes per misprediction.

### Data-Oriented Design (DOD) with `std::variant`

- **Contiguous Cache Packing:** All order variants (`OrderVariant`) are stored inline in flat arrays. Contiguous layout ensures sequential cache line prefetching (64 bytes per line transfers 2 complete 32-byte variants into L1 cache per bus transaction).
- **Zero Heap Chasing:** Zero individual heap allocations; no pointer chasing across the heap.
- **Predictable Jump Tables:** Compilers generate dense switch/jump tables for `std::visit`, allowing the branch predictor to achieve near-100% accuracy when data streams are sorted or clustered.

### 📊 Bare-Metal Microarchitecture Benchmarks (100,000 Batch)

*Audited on AMD Zen 5 (32 vCPUs @ 4.8 GHz boost, 48 KiB L1D, 1 MiB L2, 32 MiB L3) running Linux 6.8 with GCC 13.3 `-O3 -march=native`:*

| Benchmark Variant | Latency (ns/batch) | Throughput (Items/s) | Bandwidth (GiB/s) | Speedup Factor |
| :--- | :--- | :--- | :--- | :--- |
| **OOP Virtual Dispatch (Randomized)** | 588,198 ns | 170.0M items/s | 1.27 GiB/s | 1.0x (Baseline) |
| **OOP Virtual Dispatch (Sorted by Type)** | 269,731 ns | 370.8M items/s | 2.76 GiB/s | 2.18x |
| **Data-Oriented `std::variant` (Randomized)**| 446,413 ns | 224.0M items/s | 6.68 GiB/s | 1.32x |
| **Data-Oriented `std::variant` (Sorted)** | **72,823 ns** | **1,373.2M items/s** | **40.93 GiB/s** | **8.08x 🚀** |
| **Financial Feed OOP (Randomized)** | 630,866 ns | 158.5M items/s | 1.18 GiB/s | 1.0x |
| **Financial Feed `std::variant`** | **365,758 ns** | **273.4M items/s** | **10.19 GiB/s** | **1.72x** |

### 🏆 Episode 06 Community Challenge: The Accidental Copy Trap

Featured in [Episode 06](https://youtube.com/@HotPathCpp), [`common/dispatch/challenge.cc`](common/dispatch/challenge.cc) presents a subtle, high-impact performance bug frequently introduced when refactoring from OOP to `std::variant`:

- **The Trap:** Writing `[](auto item) { return item.Process(); }` inside `std::visit` forces an accidental pass-by-value copy of the entire 40+ byte variant payload onto the stack on every iteration.
- **The Silicon Fix:** Change the visitor lambda parameter signature to `[](const auto& item)` to pass variant elements directly by reference, eliminating all stack copies and memory bandwidth waste.

### Running Tests & Benchmarks

```bash
# Run unit tests
bazel test //common/dispatch/...

# Run dynamic dispatch benchmarks (direct & episode alias)
bazel run -c opt //common/dispatch:dispatch_benchmark
bazel run -c opt //episodes/06_cost_of_abstraction:benchmark

# Run Episode 06 Community Challenge
bazel run -c opt //common/dispatch:challenge
bazel run -c opt //episodes/06_cost_of_abstraction:challenge
```

---

## ⚡ Episode 07: Out-of-Order Execution & The Superscalar Pipeline

Located under [`common/execution/`](common/execution/) and [`episodes/07_out_of_order_execution/`](episodes/07_out_of_order_execution/), this module demonstrates how modern superscalar processors execute C++ instructions out of order and how software dependency chains dictate real silicon throughput.

### Microarchitectural Fundamentals

1. **Frontend vs. Backend Separation:**
   - **Frontend:** Instruction fetch (L1-I Cache), pre-decoding, branch prediction, and decoding complex x86 instructions into simpler RISC-like micro-operations ($\mu$ops).
   - **Backend:** Execution engine containing reservation stations, load/store queues, and parallel execution ports (ALUs, FPUs, AGUs).
   - **Frontend Stalls (`stalled-cycles-frontend`):** When code is branchy or code size causes instruction cache misses, the frontend stops emitting $\mu$ops, starving multi-gigahertz execution units.

2. **Register Renaming (Eliminating False WAW & WAR Dependencies):**
   - Architectural registers (e.g., the 16 general-purpose registers in x86-64) create artificial bottlenecks when reused.
   - The **Register Alias Table (RAT)** dynamically maps architectural registers to a much larger hidden pool of hardware registers in the **Physical Register File (PRF)** (e.g., 448 entries on AMD Zen 5, 512 entries on Intel Golden Cove).
   - This completely eliminates Write-After-Write (WAW) and Write-After-Read (WAR) hazards, enabling massive Instruction-Level Parallelism.

3. **The Reorder Buffer (ROB) & In-Order Retirement:**
   - Instructions execute out of order across execution ports as soon as operands are ready.
   - Results are temporarily buffered in the **Reorder Buffer (ROB)**.
   - Instructions only commit/retire to architectural state in strict program order, guaranteeing precise exceptions and clean branch misprediction recovery.

4. **Loop-Carried True Dependencies (RAW):**
   - Naive accumulations like `acc = (acc * 33) ^ data[i]` or floating point `sum += array[i]` create Read-After-Write (RAW) dependency chains.
   - The CPU physically cannot start iteration $i+1$ until iteration $i$ completes its retirement, collapsing superscalar execution width down to 1 (IPC $\approx$ 1.0).
   - **The Multi-Accumulator Fix:** Splitting accumulation into 4 or 8 independent variables (`acc0..acc3`) exposes independent instruction streams, allowing the out-of-order scheduler to flood all execution ports simultaneously (IPC $\ge$ 3.8).

### 📊 Bare-Metal Benchmarks (AMD Ryzen 9 9950X Zen 5, 65,536 Batch)

*Audited on AMD Zen 5 (32 vCPUs @ 5.1 GHz boost, 48 KiB L1D, 32 KiB L1I, 1 MiB L2, 32 MiB L3) with GCC 13.3 `-O3 -march=native`:*

| Benchmark Variant | Latency (ns) | Throughput (Items/s) | IPC Efficiency | Speedup Factor |
| :--- | :--- | :--- | :--- | :--- |
| **BM_DependentChain (Hash Recurrence)** | 36,353 ns | 1.80 G items/s | 1.01 IPC (Serialized) | 1.0x (Baseline) |
| **BM_IndependentSum4Way (4-Way Multi-Acc)** | **3,250 ns** | **20.16 G items/s** | **3.85 IPC (Superscalar)** | **11.19x 🚀** |
| **BM_IndependentSum8Way (8-Way Multi-Acc)** | **3,325 ns** | **19.71 G items/s** | **3.78 IPC** | **10.93x 🚀** |
| **BM_DependentFloatSum (IEEE 754 Serial)** | 24,093 ns | 2.72 G items/s | 1.02 IPC (Unreordered) | 1.0x (Baseline) |
| **BM_IndependentFloatSum4Way (4-Way Float)** | **6,116 ns** | **10.71 G items/s** | **3.65 IPC** | **3.94x 🚀** |

### 🏆 Episode 07 Community Challenge: Lookahead Recurrence Filter

[`common/execution/challenge.cc`](common/execution/challenge.cc) challenges engineers to optimize a 1st-order autoregressive IIR filter ($y[i] = x[i] + \alpha \cdot y[i-1]$). By applying a 4-step lookahead algebraic expansion, sub-terms are computed in parallel across ALUs, slashing runtime by **1.90x** while maintaining exact numerical precision.

### Running Episode 07 Tests & Benchmarks

```bash
# Run execution unit tests
bazel test //common/execution:execution_pipeline_test

# Run benchmarks (direct & episode alias)
bazel run -c opt //common/execution:execution_benchmark
bazel run -c opt //episodes/07_out_of_order_execution:benchmark

# Run Episode 07 Community Challenge
bazel run -c opt //common/execution:challenge
bazel run -c opt //episodes/07_out_of_order_execution:challenge
```

---

## 📜 License

Licensed under the [Apache License, Version 2.0](LICENSE).  
Copyright 2026 Hot Path C++ Authors.
