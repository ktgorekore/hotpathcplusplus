# Broadcast Ring Buffer Benchmark Results

**Date:** 2026-03-16 15:25 UTC
**System:** 32 X 4633 MHz (AMD Ryzen 9 9950X), 64GB RAM, Linux
**Memory Allocator:** tcmalloc

## Overview

The `BroadcastRingBuffer` is a high-performance Inter-Process Communication (IPC) mechanism designed for Single-Producer, Multi-Consumer (SPMC) workloads.

The implementation uses **Virtual Memory Mirroring** ("Magic Mirror") to eliminate padding and simplify wrap-around logic. It has recently been migrated to a **Flow-Controlled SPMC** architecture with zero message loss (backpressure) and **Zero-Copy** wait-free consumer reads.

### Benchmark Scenarios

1.  **Produce:** Measures the time to reserve space, write a payload, and commit a message. Includes checking consumer backpressure cursors.
2.  **RoundTripZeroCopy:** Measures a complete cycle: Produce -> PollZeroCopy -> Advance. This represents the minimum overhead for a safe, reliable IPC exchange.
3.  **PollZeroCopyBatch (100):** Measures the time to consume a batch of 100 messages using `PollZeroCopy`. This represents the amortized cost of consumption, which is dramatically improved for large payloads because there is no data copying.

## Results

| Benchmark                   | Payload (Bytes) | Latency (ns)      | Throughput (Bytes/sec) |
| :-------------------------- | :-------------- | :---------------- | :--------------------- |
| **Produce**                 | 8               | 26.1              | 292.7 MiB/s            |
|                             | 64              | 25.8              | 2.31 GiB/s             |
|                             | 512             | 27.7              | 17.20 GiB/s            |
|                             | 4096            | 60.2              | 63.48 GiB/s            |
|                             | 8192            | 86.6              | **88.06 GiB/s**        |
| **RoundTripZeroCopy**       | 8               | 51.3              | 148.6 MiB/s            |
|                             | 64              | 50.9              | 1.17 GiB/s             |
|                             | 512             | 52.9              | 9.02 GiB/s             |
|                             | 4096            | 92.5              | 41.25 GiB/s            |
|                             | 8192            | 123               | **62.13 GiB/s**        |
| **PollZeroCopyBatch (100)** | 8               | 2622 (26.2ns/msg) | 291.2 MiB/s            |
|                             | 64              | 2617 (26.1ns/msg) | 2.27 GiB/s             |
|                             | 512             | 2658 (26.5ns/msg) | 18.00 GiB/s            |
|                             | 4096            | 2697 (26.9ns/msg) | 142.5 GiB/s            |
|                             | 8192            | 2763 (27.6ns/msg) | **280.08 GiB/s**       |

## Interpretation

1.  **Reliable Producer:** The `Produce` benchmark shows a slight increase in latency (~26ns vs ~9ns historically) due to the necessity of scanning consumer cursors for backpressure. However, this trade-off provides a mathematical guarantee of zero data loss under burst loads.
2.  **Magic Mirror Efficiency:** The throughput for large payloads remains stellar.
3.  **Zero-Copy Revolution:** The `PollZeroCopyBatch` benchmark showcases the massive advantage of the new SPMC model. Previously, copying an 8KB payload safely required 897ns/msg. Now, safely returning a direct zero-copy span to the same 8KB payload takes only **27.6ns/msg**. This yields an effective theoretical throughput of 280 GiB/s for the batch consumer, eliminating the allocation and memory bandwidth bottlenecks.

## Benchmark Results (April 4, 2026 - Post SeqLock & Eviction CAS Optimization)

**Date:** 2026-04-04
**System:** 32 X 5211 MHz (AMD Ryzen 9 9950X), Linux
**Changes:** Added SeqLock versioning to `FrameHeader`, ABA generation CAS to `ReaderSlot`, and `__rdtsc()` rate-limiting to producer spin-loop.

| Benchmark                   | Payload (Bytes) | Latency (ns)      | Throughput              |
| :-------------------------- | :-------------- | :---------------- | :---------------------- |
| **Produce**                 | 8               | 21.4              | 356.2 MiB/s             |
|                             | 64              | 21.7              | 2.74 GiB/s              |
|                             | 512             | 23.7              | 20.10 GiB/s             |
|                             | 4096            | 56.1              | 67.96 GiB/s             |
|                             | 8192            | 92.0              | **82.93 GiB/s**         |
| **RoundTripZeroCopy**       | 8               | 46.7              | 163.3 MiB/s             |
|                             | 64              | 47.6              | 1.25 GiB/s              |
|                             | 512             | 49.8              | 9.57 GiB/s              |
|                             | 4096            | 83.3              | 45.79 GiB/s             |
|                             | 8192            | 115               | **66.08 GiB/s**         |
| **PollZeroCopyBatch (100)** | 8               | 2609 (26.1ns/msg) | 292.4 MiB/s             |
|                             | 64              | 2593 (25.9ns/msg) | 2.30 GiB/s              |
|                             | 512             | 2606 (26.1ns/msg) | 18.30 GiB/s             |
|                             | 4096            | 2693 (26.9ns/msg) | 141.65 GiB/s            |
|                             | 8192            | 2736 (27.4ns/msg) | **278.82 GiB/s**        |

### Interpretation Update (Post Optimization)

Despite adding the atomic `generation_id` CAS check, the Sequence Lock version increments, and improving the hardware timestamps, performance remains exceptionally high and practically identical. The `__rdtsc()` optimization slightly improved `Produce` latency on small payloads (from ~26ns down to ~21.4ns), and `PollZeroCopyBatch` shows sub-30ns latencies are comfortably maintained (~27.4ns for 8KB messages). The system is now significantly more robust against zero-copy TOCTOU and eviction races without sacrificing its theoretical 280 GiB/s throughput.

## Benchmark Results (July 20, 2026 - PollCopy Migration)

**Date:** 2026-07-20
**System:** 32 X 4866 MHz (AMD Ryzen 9 9950X), Linux
**Changes:** Migrated `PollZeroCopy` to `PollCopy` in Python bindings and order polling, relying on internal consistency checks and removing manual `SeqLock` TOCTOU verification in the consumer layers.

| Benchmark                   | Payload (Bytes) | Latency (ns)      | Throughput              |
| :-------------------------- | :-------------- | :---------------- | :---------------------- |
| **Produce**                 | 8               | 21.4              | 356.57 MiB/s            |
|                             | 64              | 21.5              | 2.77 GiB/s              |
|                             | 512             | 23.1              | 20.64 GiB/s             |
|                             | 4096            | 54.4              | 70.12 GiB/s             |
|                             | 8192            | 93.3              | **81.77 GiB/s**         |
| **RoundTripZeroCopy**       | 8               | 47.2              | 161.69 MiB/s            |
|                             | 64              | 48.0              | 1.24 GiB/s              |
|                             | 512             | 49.1              | 9.72 GiB/s              |
|                             | 4096            | 84.2              | 45.31 GiB/s             |
|                             | 8192            | 159               | **47.93 GiB/s**         |
| **PollZeroCopyBatch (100)** | 8               | 2600 (26.0ns/msg) | 293.45 MiB/s            |
|                             | 64              | 2558 (25.6ns/msg) | 2.33 GiB/s              |
|                             | 512             | 2575 (25.8ns/msg) | 18.52 GiB/s             |
|                             | 4096            | 2649 (26.5ns/msg) | 144.02 GiB/s            |
|                             | 8192            | 2659 (26.6ns/msg) | **286.93 GiB/s**        |
| **RoundTripCopy**           | 8               | 55.0              | 138.68 MiB/s            |
|                             | 64              | 55.8              | 1.07 GiB/s              |
|                             | 512             | 62.5              | 7.63 GiB/s              |
|                             | 4096            | 126               | 30.24 GiB/s             |
|                             | 8192            | 190               | **40.23 GiB/s**         |
**Environment:** 32-core AMD Ryzen (5.1 GHz), Processwrapper-sandbox, Bazel `-c opt`.
**Changes:** Migrated `PollZeroCopy` to `PollCopy` in Python bindings and order polling, relying on internal consistency checks and removing manual `SeqLock` TOCTOU verification in the consumer layers. Implemented a native `PollCopyBatch` API and refactored `PollCopy` to use pre-allocated `SafeMessage` references, drastically reducing `std::vector` allocations and batching atomic cursor updates.

### 1. Throughput & Latency (Single Producer, Single Consumer)

| **Benchmark**               | **Payload (B)** | **Time/Op (ns)**  | **Throughput**          |
|-----------------------------|-----------------|-------------------|-------------------------|
| **Produce**                 | 8               | 21.2              | 360.05 MiB/s            |
| **Produce**                 | 64              | 21.5              | 2.77 GiB/s              |
| **Produce**                 | 512             | 23.7              | 20.17 GiB/s             |
| **Produce**                 | 4096            | 50.7              | 75.31 GiB/s             |
| **Produce**                 | 8192            | 77.9              | 98.10 GiB/s             |
| **RoundTripZeroCopy**       | 8               | 46.6              | 163.95 MiB/s            |
| **RoundTripZeroCopy**       | 4096            | 79.6              | 47.95 GiB/s             |
| **RoundTripCopy**           | 8               | 56.1              | 136.07 MiB/s            |
| **RoundTripCopy**           | 4096            | 136               | 28.09 GiB/s             |

### 2. Batch Processing (100 Messages per Batch)

| **Benchmark**               | **Payload (B)** | **Time/Batch (ns)** | **Throughput**          |
|-----------------------------|-----------------|-------------------|-------------------------|
| **PollZeroCopyBatch (100)** | 8               | 2655 (26.6ns/msg) | 287.44 MiB/s            |
| **PollZeroCopyBatch (100)** | 8192            | 2688 (26.9ns/msg) | 285.68 GiB/s            |
| **PollCopyBatch (100)**     | 8               | 608 (6.1ns/msg)   | 1.22 GiB/s              |
| **PollCopyBatch (100)**     | 512             | 940 (9.4ns/msg)   | 50.85 GiB/s             |
| **PollCopyBatch (100)**     | 8192            | 12069 (120.7ns/msg)| 63.20 GiB/s             |

### Analysis

The recent modifications introducing pre-allocated `SafeMessage` references and a native `PollCopyBatch` API have drastically altered the performance profile for small payloads. 

By eliminating the continuous `std::vector` allocation overhead and reusing underlying capacities, the `PollCopyBatch` API now achieves an astonishing **6.1ns per message** for 8-byte payloads (up from ~34.9ns previously). This throughput (~1.22 GiB/s for tiny payloads) actually *surpasses* the `PollZeroCopyBatch` benchmark (~287 MiB/s). 

This counter-intuitive result is due to the batched atomic cursor update. `PollCopyBatch` reads all messages and advances the reader's cursor in a *single* atomic compare-and-swap (CAS) operation at the end of the batch. In contrast, the zero-copy benchmark must call `Advance` after every message to relinquish backpressure. For small payloads, the memory copying cost is negligible compared to the atomic synchronization cost.

For larger payloads (e.g., 8192 bytes), the memory copy overhead dominates, and `PollCopyBatch` throughput drops to ~63.2 GiB/s, while zero-copy maintains its peak throughput of ~285 GiB/s.