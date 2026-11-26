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

# Episode 17 — Bare-Metal Empirical Telemetry & Hardware PMU Report (`RealtimeGatewayService` — C++20, Rust 1.91 & Java 21 LTS)

> **Host Machine:** AMD Ryzen 9 8945HX (`Zen 4`, `16C/32T`, `32 KiB` L1D, `1 MiB` L2/core, `32 MiB × 2` L3) • `Linux 7.0.0-43-generic x86_64` (`perf_event_paranoid = -1`, quiet-CPU dedicated capture)
> **Toolchains:** `g++ 13.3.0`, `clang++ 18.1.3`, `Bazel 9.3.3` (`-std=c++20 -O3 -march=native`), `rustc 1.91.0 (-C opt-level=3 -C target-cpu=native)`, `OpenJDK 21.0.13 (-Xms2g -Xmx2g -XX:+AlwaysPreTouch -XX:+UseG1GC; 4B Compressed OOPs active < 32 GiB)`
> **BCR Libraries:** `abseil-cpp 20260526.0`, `folly 2026.08.31.00`, `boost.circular_buffer 1.90.0.bcr.1`, `boost.intrusive 1.90.0.bcr.1`
> **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/9e95Kc4hh`](https://godbolt.org/z/9e95Kc4hh) (`assembly/godbolt_comparison.s`)
> **Cross-Language Checksum Verification:** Zero warnings across C++ (`-Wall -Wextra`), Rust (`-D warnings`), and Java (`-Xlint:all -Werror`); `sequence_containers_test` passes on both GCC 13.3 and Clang 18.1, and **C++20, Rust 1.91, and Java 21 LTS produce identical 64-bit order-sensitive checksums** across Stories 1A (`0x000000aa76e018ef`), 1B (`0x00000ccd3ff50a68`), 2B (`0x9328422387174739` / `0xbe2d031d3124650a`), and 3 (`0xecb8d2992aa87a72`).

---

## Story 1: Memory Layout vs. Big-O (`SubscriptionRule` Alert Router & Dual-Queue `ActiveConnection`s)

### 1A.1 — Aged-Heap `64B` `SubscriptionRule` Full-Table Evaluation Sweep (`500` Sweeps, Pre-Timed Heap Aging)

*(Why a sequence instead of a `priority_queue` or `HashMap`: every incoming telemetry tick must evaluate dynamic predicates across all active tenant subscription rules; binary heaps only expose `top()`—and standard binary heaps in C++, Rust, and Java are backed by contiguous arrays anyway.)*

| Active Rule Count (`N`) | C++20 `std::vector` (GCC 13 / Clang 18) | C++20 `std::list` (GCC 13 / Clang 18) | Rust 1.91 `-C opt-level=3` (`Vec` vs. `LinkedList`) | Java 21 LTS Aged Heap (`ArrayList` vs. `LinkedList`) |
| :--- | :---: | :---: | :---: | :---: |
| **`N = 1,024` (`512K` evals)** | `0.249` / **`0.227 ms`** (`2.26 G/s`; `791 ns`/sweep gbench) | `2.335` / `2.404 ms` (`9.4×–10.6×` slower) | **`0.227`** vs. `2.453 ms` (`10.79×` slower) | `0.560` vs. `1.749 ms` (`3.13×` slower) |
| **`N = 4,096` (`2.05M` evals)** | `1.019` / **`0.848 ms`** (`2.41 G/s`; `3.18 µs`/sweep gbench) | `10.045` / `9.729 ms` (`9.9×–11.5×` slower) | **`0.907`** vs. `9.466 ms` (`10.44×` slower) | `2.370` vs. `8.377 ms` (`3.53×` slower) |
| **`N = 16,384` (`8.19M` evals)** | `4.852` / **`4.542 ms`** (`1.80 G/s`; `13.2 µs`/sweep gbench) | `84.066` / `84.417 ms` (**`17.33×–18.59×` slower**) | **`4.371`** vs. `52.565 ms` (**`12.03×` slower**) | `14.770` vs. `38.208 ms` (`2.59×` slower; `3.2×` `4B`-OOP hop vs. C++/Rust) |

*(Note: Aged `boost::intrusive::list` full scans match `std::list` at `84.65 ms` GCC / `85.21 ms` Clang because scattered heap nodes defeat hardware stride prefetching.)*

### 1A.2 — Zen 4 Unmultiplexed `perf stat` PMU Proof (`N = 16,384` Aged Rule Sweep, `--iters=2` = `4,000` Passes)

| Container (`N = 16,384`) | Cycles | Instructions | **IPC** | `L1-dcache-load-misses` | **LLC `cache-misses`** | **`dTLB-load-misses`** |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **`std::vector<SubscriptionRule>`** | **`276.57 M`** | `810.41 M` | **`2.93`** | `67.66 M` | **`4.04 M`** | **`38,898`** |
| **`std::list<SubscriptionRule>`** | **`3,757.79 M` (`13.6×`)** | `822.11 M` (`+1.4%`) | **`0.22`** | `145.75 M` (`2.2×`) | **`134.91 M` (`33.4×`)** | **`7,451,692` (`191.6×`)** |

### 1A.3 — Fair Mid-Table Rule Unsubscribe Showdown (`N = 8,192` Rules, `512` Unsubscribes + `65` Sweeps, `cs = 0x000000aa76e018ef`)

| Unsubscribe Architecture | Order Preserved? | GCC 13.3 (`challenge` / `gbench`) | Clang 18.1 | Rust 1.91 (`-C opt-level=3`) | Java 21 LTS |
| :--- | :---: | :---: | :---: | :---: | :---: |
| `LinkedList` / `std::list` (linear search + $O(1)$ `erase`) | Yes | `15.485 ms` / `16.39 ms` (`36.3×` vs sparse) | `14.656 ms` (`63.2×` vs sparse) | — | `5.362 ms` (`6.01×` vs sparse) |
| **`std::list` + `absl::flat_hash_map` (true $O(1)$ find + $O(1)$ `erase`)** | Yes | `2.046 ms` / `4.56 ms` (`4.80×–9.47×` vs sparse) | `2.192 ms` (`9.45×` vs sparse) | — | — |
| Contiguous `vector` / `Vec` / `ArrayList` (ordered `memmove` `erase`) | Yes | `2.147 ms` / `2.10 ms` | `1.857 ms` | `2.329 ms` | `1.846 ms` *(shifts `4B` OOPs)* |
| Contiguous `vector` / `Vec` (unindexed swap-and-pop) | No | `1.250 ms` / `1.30 ms` | `1.066 ms` | `1.474 ms` | — |
| **Contiguous `vector` / `Vec` ($O(1)$ indexed tombstone + batch compact)** | **Yes** | **`0.769 ms`** / `0.76 ms` (`2.66×` faster than idx list) | **`0.545 ms`** (`4.02×` faster) | **`0.868 ms`** | `4.155 ms` *(boxed `HashMap`)* |
| **Sparse-Set `vector` + `map` ($O(1)$ indexed swap-and-pop)** | **No** | **`0.427 ms`** / `0.48 ms` (**`4.80×–9.47×`** faster) | **`0.232 ms`** (**`9.45×`** faster) | **`0.564 ms`** | **`0.892 ms`** (**`6.01×`** faster) |

### 1B — Where Intrusive & Slab-Linked Lists Reverse the Trade-Off (`16,384` Pinned `256B` `ActiveConnection`s, Dual LRU + Timeout, `200,000` Events, `cs = 0x00000ccd3ff50a68`)

| Language & Dual-Queue Engine | Median Time | Throughput (`M events/s`) | Heap Allocs (`live_alloc_audit`) | Speedup vs. Indexed Array Shift |
| :--- | :---: | :---: | :---: | :---: |
| **C++20 `boost::intrusive::list` (dual embedded hooks)** | **`1.288 ms` (GCC) / `1.463 ms` (Clang)** *(1.35 ms gbench)* | **`136.7 – 155.3 M/s`** | **`1` slab (`0` wrapper nodes)** | **`248× – 360×` faster** |
| C++20 `std::list<ActiveConnection*>` (`splice` + iterators) | `2.567 ms` (GCC) / `2.623 ms` (Clang) | `76.3 – 77.9 M/s` | `32,771` (`+32,768` wrapper nodes) | `138× – 181×` faster |
| **C++20 `std::vector<u32>` (`O(1)` stored idx + `memmove`, zero `find`!)** | `464.16 ms` (GCC) / `362.92 ms` (Clang) *(388.6 ms gbench)* | `0.43 – 0.55 M/s` | `5` (`8.59B` insns in `perf stat`) | `1.00×` *(pure shift bottleneck)* |
| C++20 `std::vector<ActiveConnection*>` (`std::find` + `rotate`) | `539.53 ms` (GCC) / `548.66 ms` (Clang) | `0.36 – 0.37 M/s` | `3` (`9.23B` insns in `perf stat`) | `0.86×` (`375×–419×` slower) |
| **Rust 1.91 Safe Slab-Index Dual List (`u32` hooks, `0 unsafe`)** | **`0.877 ms`** | **`228.14 M/s`** | **`1` `Vec` slab (`0` node allocs)** | **`685×` faster** (vs. `600.85 ms` idx shift / `468.45 ms` rotate) |
| **Java 21 LTS Caffeine/Netty Intrusive Dual References** | **`4.241 ms`** | **`47.16 M/s`** | **`0` `LinkedList$Node` wrappers** | **`118×` faster** (vs. `499.79 ms` `int[]` shift / `649.21 ms` `ArrayList`) |

*(Engineering boundary: when only coarse idle-timeout eviction is required without strict LRU ordering, a hashed timer wheel or periodic sweep avoids maintaining doubly-linked lists altogether.)*

---

## Story 2: `std::deque` Across Compilers & Contiguous Ring Buffers (`C++20`, `Rust 1.91`, `Java 21 LTS`)

### 2A — Idle Per-Connection Outbound Queue Memory Footprint (`1,000`, `10,000` & `100,000` Empty Queues)

| Language / Container | `sizeof` | `::operator new` (`10K` Empty) | Isolated Footprint (`100K` Empty) | Cross-Platform / Runtime Behavior |
| :--- | :---: | :---: | :---: | :--- |
| **C++20 `std::deque<OutboundMessage>` (`libstdc++`)** | **`80 B`** | **`20,001`** (`2` per empty queue) | **`+68.61 MiB` `VmRSS`** *(`~686 KiB` @ `1K`)* | Eager `64B` map + `512B` chunk when empty; `libc++` uses `0B` empty, jumps to `~400 MiB` (`4,096B` block) on 1st msg & retains spare blocks; `MSVC` uses `_Block_size = 1` (`>8B`) & holds peak blocks until `shrink_to_fit()` |
| C++20 `boost::circular_buffer_space_optimized` | `56 B` | **`1`** (outer vector only) | **`+5.34 MiB` `VmRSS`** | Lazy idle allocation up to bounded max capacity *(overwrites oldest when full)* |
| **C++20 `CompactRingDeque<OutboundMessage>`** | **`32 B`** | **`1`** (outer vector only) | **`+3.18 MiB` `VmRSS` (`21.6×` leaner)** | `0` empty allocs; dynamically grows power-of-2 ring without overwriting |
| **Rust 1.91 `VecDeque<OutboundMessage>`** | **`32 B`** | **`0`** per queue | Outer `Vec` only (`~3.05 MiB`) | `VecDeque::new()` leaves capacity at `0` until first push; `as_slices()` enables vectored `writev` |
| **Java 21 LTS `new ArrayDeque<>()` vs. `new ArrayList<>()`** | `24 B` obj | `100,000` `Object[17]` arrays (`88B` w/ `4B` OOPs) | **`+7.81 MiB`** vs. **`+2.67 MiB`** JVM heap | `ArrayDeque()` eagerly allocates `Object[17]` (`17 × 4B` compressed OOPs); `ArrayList()` shares static `Object[0]` |

### 2B — `8,000,000` Steady-State FIFO Messages (Single Channel `cs=0x9328422387174739` & `1,024`-Socket Gateway Mux `cs=0xbe2d031d3124650a`)

| Workload & Engine (`8,000,000` Messages) | Steady-State Allocs | GCC 13.3 | Clang 18.1 | Throughput (`M msgs/s`) & Zen 4 PMU (`1,024`-Mux, `--iters=2`) |
| :--- | :---: | :---: | :---: | :--- |
| **Single Queue (`depth=1,024`):** C++ `std::deque` (`libstdc++`) | **`1,000,000` (`488.3 MiB`)** | `48.68 ms` | `51.92 ms` | `154.1 – 164.4 M/s` *(masked by 1 L1-hot `tcache` chunk)* |
| **Single Queue (`depth=1,024`):** C++ `boost::circular_buffer` | **`0`** *(bounded)* | **`36.98 ms`** | `39.21 ms` | `204.0 – 216.4 M/s` |
| **Single Queue (`depth=1,024`):** C++ `CompactRingDeque` | **`0`** *(growable)* | `50.40 ms` | **`18.73 ms`** | `158.8` (GCC) / **`427.2 M/s` (Clang)** |
| **Single Queue (`depth=1,024`):** Rust 1.91 `VecDeque` | **`0`** | — | **`19.94 ms`** *(LLVM)* | **`401.3 M/s`** |
| **Single Queue (`depth=1,024`):** Java 21 `ArrayDeque` vs. `LinkedList` | `8M` TLAB msgs (+`8M` Nodes) | `39.37 ms` vs. `51.15 ms` | — | **`203.2 M/s`** vs. `156.4 M/s` (`1.30×` faster) |
| **`1,024` Active Queues (`depth=16`):** C++ `std::deque` (`libstdc++`) | **`1,000,000`** | `63.01 ms` | `68.72 ms` | `116.4 – 127.0 M/s` (`1.95B` insns, `207.4M` branches, `73.7M` L1D, **`4.75M` LLC misses**) |
| **`1,024` Active Queues (`depth=16`):** C++ **`CompactRingDeque`** | **`0`** | **`46.23 ms` (`+36%`)** | **`42.89 ms` (`+60%`)** | **`173.1 – 186.5 M/s`** (**`-42%` insns, `-83%` branches, `-49%` L1D, `-81%` LLC misses / `5.3×` fewer!**) |
| **`1,024` Active Queues (`depth=16`):** **Rust 1.91 `VecDeque`** | **`0`** | — | **`37.16 ms`** *(LLVM)* | **`215.3 M/s`** |
| **`1,024` Active Queues (`depth=16`):** **Java 21 `ArrayDeque`** | `0` backing resizes | `60.48 ms` | — | **`132.3 M/s`** |

---

## Story 3: `150,000` Slot-Indexed `ClientSession` Reconnect Wave & The 3-Language Resize Triangle (`cs = 0xecb8d2992aa87a72`)

| Language & Session Table Variant (`N = 150,000`) | `nothrow_move` | `::operator new` Calls | Alloc Bytes | Final `cap` | GCC 13.3 | Clang 18.1 | Ingestion Throughput |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| C++ `std::vector + std::queue` (unreserved Rule-of-Zero) | **`FALSE`** *(libstdc++/MSVC)* | **`1,948,591`** | **`505.85 MiB`** | `262,144` | `182.11 ms` *(252.9 ms gbench)* | `183.03 ms` | **`0.59 – 0.82 M/s`** (`12.81M` LLC, `116.4K` faults) |
| C++ `std::vector + std::queue` (`.reserve(150'000)` **control**) | **`FALSE`** | **`900,001`** *(+450K move allocs)* | `230.60 MiB` | `150,000` | `51.40 ms` | `52.23 ms` | `2.87 – 2.92 M/s` |
| C++ `std::vector + Custom Move` (missing `noexcept`) | **`FALSE`** | `1,236,448` *(+786K copy allocs)* | `198.21 MiB` | `262,144` | `41.50 ms` | `42.61 ms` | `3.52 – 3.61 M/s` |
| **C++ `std::vector + CompactRingDeque` (`2.0×` Rule-of-Zero)** | **`true`** | **`450,019` (`-76.9%`)** | **`105.21 MiB`** | `262,144` | **`19.17 ms`** | **`20.96 ms`** | **`7.16 – 7.82 M/s` (`8.7×–9.5×` faster; `-90%` LLC, `-73%` faults)** |
| **C++ `std::vector<std::unique_ptr<Session>>` (pointer control)** | **`true`** | `600,019` *(+150K `make_unique`)* | `72.09 MiB` | `262,144` | `26.83 ms` | `27.28 ms` | `5.50 – 5.59 M/s` *(trades Story 1 scan locality for 8B ptr moves)* |
| **C++ `folly::fbvector + CompactRingDeque` (`1.5×` growth)** | **`true`** | `450,000` *(+C malloc)* | `53.22 MiB` | **`210,048` (`-20%` slack)** | **`19.46 ms`** | **`19.45 ms`** | **`7.71 M/s`** |
| C++ `std::vector + CompactRingDeque` (`.reserve(150'000)`) | **`true`** | **`450,001`** | `68.09 MiB` | **`150,000`** | **`17.03 ms`** | **`17.70 ms`** | **`8.47 – 8.81 M/s`** |
| **C++ `folly::fbvector + SessionRelocatable` (`IsRelocatable`)** | **`true`** | `300,000` *(inline URL)* | `38.15 MiB` | **`157,824` (`-40%` slack)** | **`9.86 ms`** | **`10.45 ms`** | **`14.36 – 15.21 M/s`** |
| **Rust 1.91 `Vec<ClientSession>`** (unres / `Box<T>` / `with_cap`) | *(bitwise `memcpy`)* | — | — | `262,144` / `150,000` | — | `41.04` / `47.06` / `40.70 ms` | **`3.66`** / `3.19` / **`3.69 M/s`** (`0` copy fallback) |
| **Java 21 LTS `ArrayList<ClientSession>`** (unres `1.5×` / pre-sized) | *(`4B` OOP `arraycopy`)* | `0` session copies | — | `1.5×` / `150,000` | **`9.13` / `8.63 ms`** *(`8.68` / `8.40 ms` standalone)* | — | **`16.43 – 17.86 M/s`** (TLAB bump alloc + `4B` OOP copy) |
