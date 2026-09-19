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
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

> **Official companion code repository for the [Hot Path C++ YouTube Channel](https://youtube.com/@HotPathCpp).**  
> We deconstruct bare-metal systems programming, high-frequency trading (HFT) patterns, CPU microarchitecture (caches, store buffers, PMUs), compiler optimizations, and modern C++20 low-latency idioms.

---

## 📺 Featured Deep-Dive Episode

| Episode | Title | Companion Video |
| :--- | :--- | :--- |
| **Episode 03** | **Zero-Copy IPC at 286 GiB/s in C++20 (The Linux MMU Magic Mirror Hack!)** | [![Watch on YouTube](https://img.shields.io/badge/Watch-YouTube_Video-red?logo=youtube)](https://youtube.com/@HotPathCpp) |

In Episode 03, Marcus and Byte deconstruct the industrial-grade broadcast ring buffer contained in this repository, showing how kernel-bypass POSIX shared memory and hardware MMU translation eliminate boundary wraparound branches entirely.

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
| **Traditional POSIX Pipe** | 8 Bytes | 1,240.0 ns | 6.2 MiB/s | 2 (read/write) | 4.8% |

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
bazel test //...
```

### 4. Run the Bare-Metal Microarchitecture Benchmarks
```bash
# Run full benchmark harness in optimized mode
bazel run -c opt //:benchmark

# Filter for a specific payload size (e.g. 8-byte messages)
bazel run -c opt //:benchmark -- --benchmark_filter=Produce/8$

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

## 📜 License

Licensed under the [Apache License, Version 2.0](LICENSE).  
Copyright 2026 Hot Path C++ Authors.
