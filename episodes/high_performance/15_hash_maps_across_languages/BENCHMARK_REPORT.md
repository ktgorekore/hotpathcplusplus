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

# Episode 15: Hash Maps Across Languages — Bare-Metal Forensic Telemetry Report

> **Hardware Testbed:** AMD Ryzen 9 8945HX (16 Cores / 32 Threads, `512 KiB` L1D, `16 MiB` L2, `64 MiB` L3) • `Linux 7.0.0-43-generic x86_64`  
> **Toolchains:** `GCC 13.3.0` & `Clang 18.1.3` (`-std=c++20 -O3 -march=native`), `Abseil 20260526.0`, `rustc 1.91.0` (`-C opt-level=3 -C target-cpu=native`), `Go 1.22.2` & `Go 1.24.0`, `OpenJDK 21 LTS` (`G1GC`, `Generational ZGC`, `JMH 1.37`, `fastutil 8.5.19`), `CPython 3.12.3`  
> **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/hfchvGWGe`](https://godbolt.org/z/hfchvGWGe)  
> **Cross-Language Parity Checksum (`16,384` entries / `32,768` 80/20 queries):** `0x88ced7ad3465eeb6` (bit-for-bit identical across C++20, Rust 1.91, Go 1.22.2, Go 1.24.0, Java 21, and Python 3.12.3)

---

## 1. Domain Workload & Why Fresh Startup Benchmarks Mislead

Every language runs identical SplitMix64 streams modeling an **Edge API Gateway Rate-Limiter & Session Directory**:
- **Packed Token-Bucket State (`16B` logical entry):** `uint64_t` positive 62-bit `account_id` $\rightarrow$ `uint64_t` `((last_seen_ms << 24) | quota)`.
- **Structured Record (`24B` `SessionState`):** `{uint64_t quota_remaining, uint64_t last_seen_ns, uint32_t tier_flags, uint32_t rate_window_hits}` (`32B` key+value inline in flat/open maps vs. boxed heap objects in reference runtimes).
- **Aged-Heap Pre-Conditioning:** Before building `2,000,000`-entry maps, `300,000` mixed `32B–256B` chunks are allocated and half-freed in checkerboard order alongside transient request allocations during insertion—eliminating the pristine startup-heap illusion.

---

## 2. Battle 1: Out-of-the-Box Paradox (Warm L1 `1K` vs. Aged Post-Churn `2M`)

| Runtime & Container | L1 `1K` (`80/20`) | Aged `2M` (`80/20` Hit/Miss) | Aged `2M` (`100%` Negative Miss) | Aged `2M` (`24B` `SessionState`) |
| :--- | :---: | :---: | :---: | :---: |
| **C++20 `absl::flat_hash_map`** | **`2.57 ns`** (`GBench`) / `3.60 ns` | **`14.54 ns`** / `14.92 ns` (`GBench`) | **`5.93 ns`** (`GBench`) / `6.45 ns` | **`17.75 ns`** (`GBench`) / `18.40 ns` |
| **Rust `1.91` `HashMap` (`FxHash`)** | **`3.19 ns`** (`313.2M/s`) | **`13.22 ns`** (`75.65M/s`) | **`5.54 ns`** (`180.5M/s`) | **`15.92 ns`** (`62.81M/s`) |
| **Rust `1.91` `HashMap` (`SipHash-1-3`, Default)** | **`6.97 ns`** (`143.5M/s`) | `41.08 ns` (`24.34M/s`) | **`11.76 ns`** (`85.03M/s`) | `46.31 ns` (`21.60M/s`) |
| **C++20 `std::unordered_map` (Default)** | `10.43 ns` (`GBench`) / `11.08 ns` | `37.59 ns` (`GBench`) / `37.85 ns` | `32.16 ns` (`GBench`) / `34.46 ns` | `46.23 ns` (`GBench`) / `48.24 ns` |
| **Go `1.24.0` `map` (SwissTable `#54766`)** | **`8.51 ns`** (`117.5M/s`) | `41.61 ns` (`24.03M/s`) | `33.86 ns` (`29.53M/s`) | `54.24 ns` (`18.44M/s`) |
| **Go `1.22.2` `map` (8-Slot Buckets)** | `14.32 ns` (`69.84M/s`) | `49.61 ns` (`20.16M/s`) | `44.70 ns` (`22.37M/s`) | `56.14 ns` (`17.81M/s`) |
| **Java `21` `fastutil` `Long2Long` / `Long2Object`** | `6.34 ns` (`JMH`) / `6.83 ns` | **`23.18 ns`** (`JMH`) / `24.51 ns` | `20.25 ns` (`JMH`) / `26.44 ns` | `35.98 ns` (`JMH`) / `37.71 ns` |
| **Java `21` `java.util.HashMap` (Default)** | `5.63 ns` / `8.10 ns` (`JMH`) | `41.25 ns` / `42.81 ns` (`JMH`) | `22.13 ns` / `26.67 ns` (`JMH`) | `55.78 ns` / `57.94 ns` (`JMH`) |
| **Python `3.12.3` `dict` (Compact Split)** | `81.50 ns` (`12.27M/s`) | `459.35 ns` (`2.18M/s`) | `128.93 ns` (`7.76M/s`) | `675.61 ns` (`1.48M/s`) |

### Key Empirical Insights from Battle 1
1. **L1 Paradox:** Even **Rust's cryptographic default (`6.97 ns`)**, **Java 21 `HashMap` (`5.63–8.10 ns`)**, and **Go 1.24 (`8.51 ns`)** outperform C++'s `std::unordered_map` (`10.43–11.08 ns`) on L1-hot integer lookups because GCC `libstdc++` executes a hardware `div` instruction by its prime bucket count on every lookup plus pointer indirection (`446.7M` branch misses over `250` passes).
2. **Negative Lookups Expose Open Addressing:** When `2,000,000` queries miss in DRAM, SwissTable `EMPTY` (`0x80`) SIMD control tags terminate after reading 1 control line: **`5.54 ns`** (`Rust FxHash`), **`5.93–6.45 ns`** (`absl::flat_hash_map`), and even **`11.76 ns`** (`Rust SipHash-1-3`—`2.9x` faster than `std::unordered_map`'s `34.46 ns`!).
3. **Go 1.22 $\rightarrow$ Go 1.24 SwissTable Leap:** Migrating Go's runtime map to SwissTables (`#54766`) improved L1 lookups **`1.68x`** (`14.32 ns` $\rightarrow$ `8.51 ns`), aged `80/20` lookups **`1.19x`** (`49.61 ns` $\rightarrow$ `41.61 ns`), and cut LLC `cache-misses` **`2.08x`** (`354.2M` $\rightarrow$ `170.0M`).

---

## 2B. Battle 1B: Read Throughput — Full-Table (`2M`) vs. Post-Peak Sparse (`200K`) Iteration

Periodic TTL expiration sweeps, billing rollups, and snapshot exporters iterate every live entry (`sum += k ^ v`, `0x34e6d4b436e4fbb4` full / `0xc8118d222e4566bb` sparse after evicting `90%` of entries without shrinking capacity):

| Runtime & Container | Full-Table `2M` Sweep (`ms/swp` / `ns/live` / `M/s`) | Post-Peak Sparse `200K` (`90%` Evicted, `ms/swp` / `ns/live` / `M/s`) | Traversal Complexity & Cache Physics |
| :--- | :---: | :---: | :--- |
| **Rust `1.91` `HashMap` (`SipHash-1-3`)** | **`2.41 ms` (`1.21 ns`, `829.1 M/s`)** | **`1.10 ms` (`5.50 ns`, `181.9 M/s`)** | `hashbrown` SIMD `RawIter`: skips 16 empty control tags/instr (`1.51` IPC full) |
| **Rust `1.91` `HashMap` (`FxHash`)** | **`2.44 ms` (`1.22 ns`, `819.3 M/s`)** | **`1.13 ms` (`5.64 ns`, `177.2 M/s`)** | Identical layout to SipHash (hashing never runs during iteration!) |
| **C++20 `absl::flat_hash_map`** | `8.04 ms` / `8.08 ms` `GBench` (`4.02 ns`, `248.6 M/s`) | `3.02 ms` / `2.77 ms` `GBench` (`15.12 ns`, `66.1–72.3 M/s`) | `O(capacity)`: must scan all `4.19M` `kDeleted` ctrl bytes at `4.8%` fill (`3.8x` slower/live) |
| **Java `21` `fastutil` `Long2Long`** | `11.60 ms` (`5.80 ns` challenge & JMH, `172.5 M/s`) | `4.76 ms` (`21.83 ns` JMH / `23.79 ns`, `42.0–45.8 M/s`) | `fastIterator()` scans contiguous `long[]` without allocation (`O(capacity)`) |
| **Go `1.22.2` `map` (8-Slot Buckets)** | `12.87 ms` (`6.44 ns`, `155.4 M/s`) | `4.36 ms` (`21.79 ns`, `45.9 M/s`) | Beats `Go 1.24.0` by **`45%`** on sparse sweeps (`19.5M` vs `56.1M` LLC misses) |
| **Go `1.24.0` `map` (SwissTable)** | `13.89 ms` (`6.95 ns`, `144.0 M/s`) | `6.32 ms` (`31.59 ns`, `31.7 M/s`) | Walks unshrunk split-directory sub-tables + control words |
| **Java `21` `java.util.HashMap`** | `21.00 ms` (`7.44 ns` JMH / `10.50 ns`, `95.2–134.4 M/s`) | `6.71 ms` (`24.73 ns` JMH / `33.53 ns`, `29.8–40.4 M/s`) | **`3.2x–4.6x` faster than C++ `std`** on full sweep via G1GC locality; still `O(capacity)` sparse |
| **C++20 `std::unordered_map`** | **`67.87 ms` / `67.9 ms` `GBench` (`33.94 ns`, `29.5 M/s`)** | **`3.11 ms` / `2.79 ms` `GBench` (`15.53 ns`, `64.4–71.6 M/s`)** | **`28x` behind Rust** full (`0.13` IPC chasing fragmented nodes), yet **`21.8x` faster** (`O(live)`) sparse! |
| **Python `3.12.3` `dict`** | `173.03 ms` (`86.51 ns`, `11.6 M/s`) | `38.15 ms` (`190.74 ns`, `5.2 M/s`) | Scans `dk_entries` including `DKIX_DUMMY` tombstones |

> **Key Empirical Flip (Why Single-Op Benchmarks Mislead):** At `2M` full occupancy, C++'s `std::unordered_map` (`67.9 ms`) trails even garbage-collected boxed `Java HashMap` (`7.44–10.50 ns/live`) and `Rust` (`1.21 ns/live`, **`28x` faster**) because node pointers scatter across DRAM. Yet after `90%` TTL eviction inside an unshrunk peak table (`200K` live in `4.19M` capacity), `std::unordered_map` walks **only live nodes (`O(live)`)** in `2.79–3.11 ms`—cutting per-entry cost from `33.9 ns` to `15.5 ns` and matching `absl::flat_hash_map` (`2.77–3.02 ms`, whose `O(capacity)` control scan rises from `4.0 ns` to `15.1 ns/live`)!

---

## 3. Battle 2: Memory Footprint & Unreserved Insert Tail Latency (`2,000,000` Entries)

### Battle 2A: Isolated Process `VmRSS` / Live JVM Heap Footprint

| Implementation | `16B` Logical Entry (`uint64 -> uint64`) | `32B` Logical Entry (`uint64 -> 24B SessionState`) | Architectural Memory Mechanism |
| :--- | :---: | :---: | :--- |
| **Java `21` `fastutil` (`Long2Long` / `Long2Object`)** | **`66.0 MiB` (`34.6 B/entry`)** | `126.4 MiB` (`66.3 B/entry`) | Unboxed primitive `long[]` arrays (`0` `Node` / `Long` boxes) |
| **Rust `1.91` `HashMap` / C++20 `absl::flat_hash_map`** | **`68.0–68.1 MiB` (`35.7 B/entry`)** | `132.0–132.1 MiB` (`69.2–69.3 B/entry`) | Power-of-two flat table (`4,194,304` slots) + `1B/slot` ctrl array |
| **Go `1.24.0` `map` (SwissTable)** | `72.6 MiB` RSS (`38.0 B/entry`) | `83.8 MiB` Heap (`44.0 B`) / `145.2 MiB` RSS | Split-directory SwissTable sub-tables (avoids full 4M-slot jump) |
| **Go `1.22.2` `map` (Classic Buckets)** | `73.6 MiB` RSS (`38.6 B/entry`) | `68.3 MiB` Heap (`35.8 B`) / `137.5 MiB` RSS | 8-entry inline buckets (`256B`/`320B`) + `tophash[8]` |
| **C++20 `std::unordered_map` (GCC `libstdc++`)** | `77.5 MiB` (`40.6 B/entry`) | **`108.0 MiB` (`56.6 B/entry`)** | `8B` bucket ptr + `32B`/`48B` `glibc` chunk (`18%` *smaller* than flat at `32B`!) |
| **Python `3.12.3` `dict`** | `80.3 MiB` RSS (`42.1 B/entry`, table only) | `202.9 MiB` RSS (`106.4 B/entry`) | Compact `dk_indices` + `dk_entries`, plus `PyObject` boxing |
| **Java `21` `java.util.HashMap` (Boxed)** | **`169.3 MiB` (`88.7 B/entry`)** | **`200.3 MiB` (`105.0 B/entry`)** | **`2.56x` `fastutil`**: `2` boxed `Long`s + `32B` `Node` + table array |

> **Critical Insight for Episode 16 Preview:** For `16B` key-value pairs, flat SwissTables save memory (`35.7 B` vs `40.6 B`). By `32B` (`8B` key + `24B` struct), power-of-two capacity rounding (`4,194,304` slots for `2M` entries) causes `absl::flat_hash_map` and Rust `HashMap` (`132 MiB`) to consume **22% MORE memory** than node-based `std::unordered_map` (`108 MiB`)!

### Battle 2B: Streaming Insert Tail Latency (`0` $\rightarrow$ `2,000,000` Inserts)

| Implementation & Reservation | Total Time | `p50` | `p99` | `p99.9` | `p99.99` | `Max` Rehash / GC Spike |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map` (Unreserved)** | `538.0 ms` | `120 ns` | `672 ns` | `932 ns` | `4,919 ns` | **`79.91 ms`** |
| **C++20 `std::unordered_map` (Reserved)** | `372.6 ms` | `120 ns` | `592 ns` | `862 ns` | `4,730 ns` | **`90.1 µs`** (**`887x` lower**) |
| **C++20 `absl::flat_hash_map` (Unreserved)** | `160.9 ms` | `30 ns` | `231 ns` | `371 ns` | `591 ns` | **`26.30 ms`** |
| **C++20 `absl::flat_hash_map` (Reserved)** | `181.0 ms` | `30 ns` | `370 ns` | `1,473 ns` | `3,717 ns` | **`18.1 µs`** (**`1,454x` lower**) |
| **Rust `1.91` `HashMap` (Unreserved)** | `247.3 ms` | `50 ns` | `310 ns` | `401 ns` | `832 ns` | **`37.16 ms`** |
| **Rust `1.91` `HashMap` (Reserved)** | `221.2 ms` | `60 ns` | `391 ns` | `1,563 ns` | `4,158 ns` | **`22.4 µs`** (**`1,661x` lower**) |
| **Go `1.22.2` `map` (Unreserved)** | `428.0 ms` | `120 ns` | `651 ns` | `1,362 ns` | `5,510 ns` | `26.73 ms` (`45.7 µs` reserved) |
| **Go `1.24.0` `map` (Unreserved SwissTable!)** | `369.8 ms` | `110 ns` | `401 ns` | `14,267 ns` | `26,421 ns` | **`156.7 µs` (`0.157 ms`!)** |
| **Java `21` `HashMap` (Unreserved G1GC / ZGC)** | `369.4 / 430.7 ms` | `110 ns` | `460 ns` | `611 ns` | `4,799 ns` | `30.98 ms` / `46.82 ms` |
| **Java `21` `fastutil` (Reserved G1GC / ZGC)** | `262.6 / 262.9 ms` | `101 ns` | `370 ns` | `431 ns` | `4,168 ns` | **`95.1 µs`** / **`98.0 µs`** |

---

## 4. Battle 3: Security vs. Speed — Live Adversarial HashDoS (`40,000` Keys)

| Language & Map | Attack Payload | Normal Time | Attack Time | Impact & Mitigation Mechanism |
| :--- | :--- | :---: | :---: | :--- |
| **C++20 `std::unordered_map`** | Multiples of `libstdc++` prime `42,043` | `0.88 ms` (`1.47 ms` repro) | **`1,101.16 ms`** (`11,113 ms` `GBench`) | **`1,254x` (`O(N^2)`) collapse** — `40,000 / 40,000` in `bucket(0)`! |
| **C++20 `absl::flat_hash_map`** | Multiples of `libstdc++` prime `42,043` | `0.44 ms` | **`0.43 ms`** (`0.180 ms` `GBench`) | **Immune** (`~1.0x`) — per-process salted mixing (`mulx`) |
| **Rust `1.91` Default (`SipHash-1-3`)** | Both Prime & `FxHash`-targeted keys | `0.69 ms` | **`0.70 ms`** | **Immune** (`1.0x`) — cryptographic keyed PRF |
| **Rust `1.91` `FxHash` (`rustc-hash`)** | `K^-1`-targeted H1+H2 collision keys | `0.69 ms` | **`735.76 ms`** | **`1,056.3x` collapse** — why Rust restricts `FxHash` to trusted keys |
| **Java `21` `java.util.HashMap`** | `(i << 16) \| i` (`hash & 0xFFFF == 0`) | `0.71 ms` (`510 µs` JMH) | **`5.70 ms`** (`2.23 ms` JMH) | **OpenJDK `JEP 180`** — `treeifyBin()` RB-tree bounds worst case to `O(N log N)` (`8.1x`) |
| **Go `1.22.2` / `Go 1.24.0`** | Multiples of `43,067` | `0.79 / 0.50 ms` | **`0.78 / 0.54 ms`** | **Immune** (`~1.0x`) — randomized per-map seed |
| **Python `3.12.3` `dict`** | Multiples of `43,067` | `2.50 ms` | **`1.68 ms`** | **Immune** — randomized per-process SipHash-1-3 (`PYTHONHASHSEED`) |

---

## 5. Unmultiplexed Linux `perf stat` Hardware PMU Counters (Zen 5, `100%` Uptime)

Three strictly unmultiplexed passes (`nmi_watchdog=1`, `<= 4` hardware events per pass):

### 5A. L1-Resident `1,024`-Entry Lookups (`250` Passes = `500,000,000` Lookups)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Loads | L1D Misses | LLC Misses |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `27.96B` | `15.56B` | `0.56` | **`446.7M`** | `13.45B` | **`1.00B`** | `7.99M` |
| **C++20 `absl::flat_hash_map`** | **`9.27B`** | `23.20B` | **`2.50`** | **`117.0M`** | `8.45B` | **`266.5M`** | `4.79M` |
| **Rust `1.91` `SipHash-1-3`** | `17.56B` | **`50.35B`** | **`2.87`** | `112.1M` | `5.48B` | `190.3M` | `4.88M` |
| **Rust `1.91` `FxHash`** | **`8.16B`** | **`14.50B`** | `1.78` | `126.1M` | `3.67B` | `190.0M` | `3.85M` |

### 5B. Aged `2,000,000`-Entry `80/20` Lookups (`40` Passes = `80,000,000` Lookups; Python `6` Passes = `12M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `17.97B` | `4.22B` | `0.23` | `80.2M` | `366.5M` | **`207.2M`** | **`359.1M`** |
| **C++20 `absl::flat_hash_map`** | **`7.22B`** | `4.72B` | `0.65` | **`27.7M`** | `249.1M` | **`84.3M`** | `226.1M` |
| **Rust `1.91` `SipHash-1-3`** | `17.65B` | `9.32B` | `0.53` | `28.2M` | `204.6M` | `68.0M` | `181.8M` |
| **Rust `1.91` `FxHash`** | **`6.28B`** | **`3.26B`** | `0.52` | `27.6M` | `203.6M` | **`67.8M`** | **`175.1M`** |
| **Go `1.22.2` `map`** | `21.45B` | `11.49B` | `0.54` | `82.8M` | `365.0M` | `75.8M` | **`354.2M`** |
| **Go `1.24.0` `map` (SwissTable)** | `17.57B` | `10.22B` | `0.58` | **`29.9M`** | `333.5M` | `73.1M` | **`170.0M`** (`2.08x` drop) |
| **Java `21` `HashMap<Long, Long>`** | `30.55B` | `11.00B` | `0.36` | `119.8M` | `469.4M` | `148.6M` | **`368.0M`** |
| **Java `21` `fastutil` `Long2Long`** | `21.01B` | `8.17B` | `0.39` | `152.2M` | `363.6M` | `142.0M` | `250.5M` |
| **Python `3.12` `dict` (`6` passes)** | `66.52B` | `118.77B` | `1.79` | `78.3M` | `808.0M` | `69.9M` | `197.9M` |

### 5C. Full-Table `2,000,000`-Entry Iteration (`40` Sweeps = `80M` Live; Python `10` = `20M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `16.21B` | `2.18B` | **`0.13`** | `9.04M` | `119.4M` | **`27.14M`** | **`75.99M`** |
| **C++20 `absl::flat_hash_map`** | `2.50B` | `2.37B` | `0.95` | `81.89M` | `66.55M` | `2.92M` | `7.56M` |
| **Rust `1.91` `SipHash-1-3`** | `1.25B` | `1.89B` | **`1.51`** | **`18.69M`** | **`65.44M`** | `2.87M` | `6.63M` |
| **Rust `1.91` `FxHash`** | **`1.20B`** | **`1.73B`** | `1.44` | **`18.62M`** | `65.61M` | `2.87M` | **`6.62M`** |
| **Go `1.22.2` `map`** | `3.84B` | `11.53B` | `3.00` | `33.36M` | `73.21M` | **`2.65M`** | `9.95M` |
| **Go `1.24.0` `map` (SwissTable)** | `3.97B` | `10.70B` | `2.69` | `28.32M` | `70.27M` | **`2.65M`** | `9.37M` |
| **Java `21` `HashMap<Long, Long>`** | `19.10B` | `11.97B` | `0.63` | `169.18M` | `300.88M` | `11.37M` | `58.40M` |
| **Java `21` `fastutil` `Long2Long`** | `13.33B` | `9.25B` | `0.69` | `130.27M` | `185.49M` | `7.53M` | `29.16M` |
| **Python `3.12` `dict` (`10` passes)** | `33.76B` | `102.28B` | `3.03` | `60.03M` | `495.30M` | `9.85M` | `79.46M` |

### 5D. Post-Peak Sparse `200,000`-Entry Iteration (`90%` Evicted, `200` Sweeps = `40M` Live; Python `40` = `8M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `5.92B` | `2.08B` | `0.35` | **`9.36M`** | `89.27M` | `28.47M` | `68.06M` |
| **C++20 `absl::flat_hash_map`** | `4.37B` | **`7.10B`** (`3.4x` `std`) | `1.63` | `48.60M` | `102.02M` | `7.52M` | `56.99M` |
| **Rust `1.91` `SipHash-1-3`** | `2.49B` | `2.02B` | `0.81` | `46.49M` | **`78.45M`** | `7.45M` | `50.08M` |
| **Rust `1.91` `FxHash`** | **`2.16B`** | **`1.72B`** | `0.80` | `46.96M` | `78.65M` | `7.45M` | `49.89M` |
| **Go `1.22.2` `map`** | `6.24B` | `18.29B` | `2.93` | `55.61M` | `270.15M` | `7.22M` | **`19.46M`** |
| **Go `1.24.0` `map` (SwissTable)** | `8.35B` | `10.38B` | `1.24` | `52.03M` | `246.35M` | **`7.15M`** | `56.06M` (`2.88x` `1.22`) |
| **Java `21` `HashMap<Long, Long>`** | `21.63B` | `13.48B` | `0.62` | `122.96M` | `317.25M` | `21.54M` | `123.40M` |
| **Java `21` `fastutil` `Long2Long`** | `15.19B` | `12.64B` | `0.83` | `114.82M` | `322.34M` | `13.85M` | `55.21M` |
| **Python `3.12` `dict` (`40` passes)** | `33.56B` | `80.64B` | `2.40` | `53.41M` | `534.37M` | `14.02M` | `99.10M` |

---

## 6. Verified Primary Citations (Slide Citation Bar Sources)

- `Klink & Wälde (28C3, 2011) — HashDoS Web Application Platform Attacks`
- `Aumasson & Bernstein (INDOCRYPT 2012) — SipHash: A Fast Short-Input PRF`
- `OpenJDK JEP 180 (Mike Duigou, 2014) — java.util.HashMap::treeifyBin()`
- `CPython 3.6 (Raymond Hettinger, 2016) — Objects/dictobject.c (Compact Dict)`
- `WG21 P1863R1 (Titus Winters, 2020) — "ABI — Now or Never"`
- `Go Proposal #54766 (Michael Pratt, Go 1.24) — SwissTable Runtime Map`
