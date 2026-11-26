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

# Episode 17: Sequence Containers on Real Hardware (`RealtimeGatewayService` — C++20, Rust 1.91 & Java 21 LTS)

> **Official Companion Code, Benchmarks, Dual-Compiler Assembly & Linux `perf stat` Suite for [Hot Path Engineering (`@HotPathEng`)](https://youtube.com/@HotPathEng) Episode 17.**  
> **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/9e95Kc4hh`](https://godbolt.org/z/9e95Kc4hh)  
> **Full Bare-Metal Telemetry Report:** [`BENCHMARK_REPORT.md`](./BENCHMARK_REPORT.md)

All three backend stories live inside one cohesive **`RealtimeGatewayService`** architecture, verified on bare-metal AMD Ryzen 9 8945HX (`Zen 4`) with matching 64-bit order-sensitive checksums across **C++20 (GCC 13.3 & Clang 18.1)**, **Rust 1.91 (`-C opt-level=3`)**, and **OpenJDK 21 LTS (`-Xms2g -Xmx2g -XX:+AlwaysPreTouch -XX:+UseG1GC`)**:

1. **Story 1 — Memory Layout vs. Big-O (`SubscriptionRule` Alert Router & Dual-Queue `ActiveConnection`s):**
   - **1A (Full-Table Rule Sweeps & Mid-Table Unsubscribes):** Scanning `16,384` `64B` `SubscriptionRule` records across a pre-aged heap drops `std::list` IPC from **`2.93`** (`std::vector`, `4.54–4.85 ms`, Rust `Vec` `4.37 ms`) to **`0.22`** (`84.1–84.4 ms`, **`17.3×–18.6×` slower**) due to **`33.4×` more LLC misses** and **`191.6×` more `dTLB` page-walk misses**. Across `512` mid-table unsubscribes + `65` sweeps (`N = 8,192`), **Sparse-Set `vector` + `map` (`0.23–0.43 ms` C++, `0.56 ms` Rust, `0.89 ms` Java)** and order-preserving **Tombstone + batch `std::erase_if` (`0.55–0.77 ms` C++, `0.87 ms` Rust)** decisively beat `std::list` even when paired with an $O(1)$ `absl::flat_hash_map` iterator index (`2.05–2.19 ms`).
   - **1B (Where Intrusive & Slab-Linked Lists Reverse the Trade-Off):** When `16,384` OS-pinned `256B` `ActiveConnection` structs move across both an LRU eviction list and an idle-timeout list (`200,000` events), even an **`O(1)` indexed `std::vector<uint32_t>` shift with zero linear search** stalls for **`362.9–464.2 ms`** (`499.8 ms` Java, `600.8 ms` Rust), while embedded intrusive links finish in **`1.29–1.46 ms`** in C++ (`boost::intrusive::list`, `1` slab alloc, **`248×–360×` faster**), **`0.877 ms`** in 100% Safe Rust (`u32` slab hooks, **`685×` faster**), and **`4.24 ms`** in Java 21 (Caffeine-style intrusive refs, **`118×` faster**).
2. **Story 2 — `std::deque` Across Compilers & Contiguous Ring Buffers:**
   - **2A (Idle Per-Connection Queues):** `100,000` empty `std::deque<OutboundMessage>` instances consume **`+68.61 MiB` `VmRSS`** on GNU `libstdc++` (`200,001` heap allocations) vs. **`+3.18 MiB`** (`21.6×` leaner) for growable `CompactRingDeque`, `0` per-queue allocs for Rust `VecDeque::new()`, and `+7.81 MiB` for Java's eager `ArrayDeque` `Object[17]` (`88B` under `4B` Compressed OOPs).
   - **2B (`8,000,000`-Message FIFO Stream):** Single-queue `glibc tcache` recycling masks `std::deque`'s `1,000,000` (`488.3 MiB`) chunk allocations; multiplexing across `1,024` active sockets exposes the allocator and chunk-map tax—switching to `CompactRingDeque` boosts C++ throughput **`+36%` (GCC) to `+60%` (Clang)** (`-42%` instructions, `-83%` branches, `-49%` L1D misses, **`-81%` LLC misses / `5.3×` fewer**), with Rust `VecDeque` leading at **`37.16 ms` (`215.3 M msgs/s`)**.
3. **Story 3 — `150,000`-Session Reconnect Wave & The 3-Language Resize Triangle:**
   - Embedding `std::queue` (`std::deque`) inside a Rule-of-Zero `ClientSession` makes `std::is_nothrow_move_constructible_v` evaluate to `true` on macOS `libc++`, yet **`false`** on Linux `libstdc++` and Windows `MSVC STL`—triggering **`1,948,591` heap allocations (`505.85 MiB`, `182.1 ms`, `0.82 M/s`)** on unreserved growth and still leaving **`900,001` allocations (`51.4 ms`)** even with `.reserve(150'000)`.
   - Swapping in `CompactRingDeque` drops allocations **`-76.9%`** to **`450,019`** (`19.2 ms`, **`9.5×` faster**; **`9.86 ms` / `15.2 M/s`** with `folly::fbvector` + `IsRelocatable`).
   - Direct pointer-indirection controls (`std::vector<std::unique_ptr<Session>>` at `26.8 ms` / `600,019` allocs; Rust `Vec<Box<ClientSession>>` at `47.1 ms` vs. `41.0 ms` inline `Vec`) contrast directly with **OpenJDK 21 `ArrayList<ClientSession>` (`8.68–9.13 ms`, `16.4–17.3 M/s`)**, which pairs `4B` Compressed OOP `System.arraycopy` with HotSpot TLAB bump allocation.

---

## Reproduction Commands

```bash
# 1. Run GoogleTest parity & compile-time static_assert checks (GCC & Clang)
bazel test -c opt //common/sequence_containers:sequence_containers_test
CC=clang CXX=clang++ bazel test -c opt //common/sequence_containers:sequence_containers_test

# 2. Run isolated ::operator new allocation audit & VmRSS snapshot
bazel run -c opt //episodes/high_performance/17_sequence_containers_physics:live_alloc_audit
bazel run -c opt //episodes/high_performance/17_sequence_containers_physics:challenge -- --mode=story2a_rss

# 3. Run full C++20 bare-metal challenge & official Google Benchmark suite
bazel run -c opt //episodes/high_performance/17_sequence_containers_physics:challenge -- --mode=all
bazel run -c opt //episodes/high_performance/17_sequence_containers_physics:benchmark

# 4. Run Rust 1.91 (-C opt-level=3) & OpenJDK 21 LTS 3-story parity harnesses
rustc -C opt-level=3 -C target-cpu=native \
  episodes/high_performance/17_sequence_containers_physics/rust_sequence_bench.rs -o /tmp/ep17_rust && /tmp/ep17_rust
bazel run -c opt //episodes/high_performance/17_sequence_containers_physics:java_challenge

# 5. Run automated end-to-end forensic + 3-pass unmultiplexed Linux perf stat suite
./episodes/high_performance/17_sequence_containers_physics/run_ep17_forensics.sh
```
