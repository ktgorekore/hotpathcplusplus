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
> **Cross-Language Parity Checksum (`16,384` entries / `32,768` 80/20 queries):** `0x88ced7ad3465eeb6` (bit-for-bit identical across C++20, Rust 1.91, Go 1.22.2, Go 1.24.0, and Python 3.12.3; Java 21 `HashMap` / `fastutil` match bit-for-bit on every workload)

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
| **C++20 `absl::flat_hash_map`** | **`2.45 ns`** (`GBench`) / `3.42 ns` | **`13.16 ns`** (`GBench`) / `13.31 ns` | **`5.56 ns`** (`GBench`) / `5.88 ns` | **`16.99 ns`** / `17.08 ns` (`GBench`) |
| **Rust `1.91` `HashMap` (`FxHash`)** | **`3.07 ns`** (`325.5M/s`) | **`11.65 ns`** (`85.84M/s`) | **`4.02 ns`** (`248.6M/s`) | **`14.10 ns`** (`70.90M/s`) |
| **Rust `1.91` `HashMap` (`SipHash-1-3`, Default)** | **`6.60 ns`** (`151.5M/s`) | `32.92 ns` (`30.37M/s`) | **`9.39 ns`** (`106.5M/s`) | `40.56 ns` (`24.65M/s`) |
| **C++20 `std::unordered_map` (Default)** | `9.79 ns` (`GBench`) / `10.62 ns` | `33.58 ns` (`GBench`) / `35.10 ns` | `30.15 ns` (`GBench`) / `30.96 ns` | `43.77 ns` / `43.85 ns` (`GBench`) |
| **Go `1.24.0` `map` (SwissTable `#54766`)** | **`8.12 ns`** (`123.1M/s`) | `33.05 ns` (`30.25M/s`) | `26.69 ns` (`37.46M/s`) | `47.12 ns` (`21.22M/s`) |
| **Go `1.22.2` `map` (8-Slot Buckets)** | `13.45 ns` (`74.35M/s`) | `43.81 ns` (`22.83M/s`) | `41.01 ns` (`24.38M/s`) | `51.61 ns` (`19.38M/s`) |
| **Java `21` `fastutil` `Long2Long` / `Long2Object`** | `6.15 ns` (`JMH`) / `6.48 ns` | **`21.32 ns`** (`JMH`) / `21.49 ns` | `17.12 ns` (`JMH`) / `18.29 ns` | `32.72 ns` / `33.06 ns` (`JMH`) |
| **Java `21` `java.util.HashMap` (Default)** | `5.37 ns` / `7.83 ns` (`JMH`) | `34.45 ns` / `38.23 ns` (`JMH`) | `17.89 ns` / `22.07 ns` (`JMH`) | `47.35 ns` / `53.24 ns` (`JMH`) |
| **Python `3.12.3` `dict` (Compact Split)** | `74.06 ns` (`13.50M/s`) | `403.14 ns` (`2.48M/s`) | `112.60 ns` (`8.88M/s`) | `606.36 ns` (`1.65M/s`) |

### Key Empirical Insights from Battle 1
1. **L1 Paradox:** Even **Rust's cryptographic default (`6.60 ns`)**, **Java 21 `HashMap` (`5.37–7.83 ns`)**, and **Go 1.24 (`8.12 ns`)** outperform C++'s `std::unordered_map` (`9.79–10.62 ns`) on L1-hot integer lookups because GCC `libstdc++` executes a hardware `div` instruction by its prime bucket count on every lookup plus pointer indirection (`443.7M` branch misses over `250` passes).
2. **Negative Lookups Expose Open Addressing:** When `2,000,000` queries miss in DRAM, SwissTable `EMPTY` (`0x80`) SIMD control tags terminate after reading 1 control line: **`4.02 ns`** (`Rust FxHash`), **`5.56–5.88 ns`** (`absl::flat_hash_map`), and even **`9.39 ns`** (`Rust SipHash-1-3`—`3.3x` faster than `std::unordered_map`'s `30.96 ns`!).
3. **Go 1.22 $\rightarrow$ Go 1.24 SwissTable Leap:** Migrating Go's runtime map to SwissTables (`#54766`) improved L1 lookups **`1.66x`** (`13.45 ns` $\rightarrow$ `8.12 ns`), aged `80/20` lookups **`1.33x`** (`43.81 ns` $\rightarrow$ `33.05 ns`), and cut LLC `cache-misses` **`2.10x`** (`349.65M` $\rightarrow$ `166.20M`).

---

## 2B. Battle 1B: Read Throughput — Full-Table (`2M`) vs. Post-Peak Sparse (`200K`) Iteration

Periodic TTL expiration sweeps, billing rollups, and snapshot exporters iterate every live entry (`sum += k ^ v`, `0x408f34c03517a024` full / `0x9c5a519a826e497` sparse across C++/Rust/Go after evicting `90%` of entries without shrinking capacity):

| Runtime & Container | Full-Table `2M` Sweep (`ms/swp` / `ns/live` / `M/s`) | Post-Peak Sparse `200K` (`90%` Evicted, `ms/swp` / `ns/live` / `M/s`) | Traversal Complexity & Cache Physics |
| :--- | :---: | :---: | :--- |
| **Rust `1.91` `HashMap` (`SipHash-1-3`)** | **`2.43 ms` (`1.21 ns`, `823.1 M/s`)** | **`1.13 ms` (`5.67 ns`, `176.4 M/s`)** | `hashbrown` SIMD `RawIter`: skips 16 empty control tags/instr (`1.51` IPC full) |
| **Rust `1.91` `HashMap` (`FxHash`)** | **`2.44 ms` (`1.22 ns`, `821.0 M/s`)** | **`1.14 ms` (`5.68 ns`, `176.0 M/s`)** | Identical layout to SipHash (hashing never runs during iteration!) |
| **C++20 `absl::flat_hash_map`** | `8.09 ms` `GBench` / `8.26 ms` (`4.13 ns`, `242.0 M/s`) | `2.83 ms` `GBench` / `3.77 ms` (`18.86 ns`, `53.0–70.8 M/s`) | `O(capacity)`: must scan all `4.19M` `kDeleted` ctrl bytes at `4.8%` fill (`4.6x` slower/live) |
| **Java `21` `fastutil` `Long2Long`** | `11.72 ms` (`5.83 ns` JMH / `5.86 ns`, `170.7 M/s`) | `6.25 ms` (`22.29 ns` JMH / `31.27 ns`, `32.0–44.9 M/s`) | `fastIterator()` scans contiguous `long[]` without allocation (`O(capacity)`) |
| **Go `1.22.2` `map` (8-Slot Buckets)** | `12.91 ms` (`6.46 ns`, `154.9 M/s`) | `4.41 ms` (`22.06 ns`, `45.3 M/s`) | Beats `Go 1.24.0` by **`41%`** on sparse sweeps (`19.6M` vs `57.2M` LLC misses) |
| **Go `1.24.0` `map` (SwissTable)** | `13.92 ms` (`6.96 ns`, `143.7 M/s`) | `6.22 ms` (`31.10 ns`, `32.2 M/s`) | Walks unshrunk split-directory sub-tables + control words |
| **Java `21` `java.util.HashMap`** | `38.32 ms` (`7.40 ns` JMH / `19.16 ns`, `52.2–135.2 M/s`) | `9.24 ms` (`25.87 ns` JMH / `46.19 ns`, `21.7–38.7 M/s`) | `O(capacity)` bucket array scan (`G1GC` young locality boosts unaged JMH sweep) |
| **C++20 `std::unordered_map`** | **`64.6 ms` `GBench` / `73.99 ms` (`37.00 ns`, `27.0 M/s`)** | **`2.73 ms` `GBench` / `3.57 ms` (`17.86 ns`, `56.0–73.3 M/s`)** | **`30x` behind Rust** full (`0.13` IPC chasing fragmented nodes), yet **BEATS `absl`** (`O(live)`) sparse! |
| **Python `3.12.3` `dict`** | `174.06 ms` (`87.03 ns`, `11.5 M/s`) | `38.57 ms` (`192.84 ns`, `5.2 M/s`) | Scans `dk_entries` including `DKIX_DUMMY` tombstones |

> **Key Empirical Flip (Why Single-Op Benchmarks Mislead):** At `2M` full occupancy, C++'s `std::unordered_map` (`64.6–73.99 ms`) trails even `Java fastutil` (`11.72 ms`) and `Rust` (`2.43 ms`, **`30x` faster**) because node pointers scatter across DRAM. Yet after `90%` TTL eviction inside an unshrunk peak table (`200K` live in `4.19M` capacity), `std::unordered_map` walks **only live nodes (`O(live)`)** in **`2.73 ms` `GBench` / `3.57 ms`**—cutting per-entry cost in half and beating `absl::flat_hash_map` (`2.83 ms` `GBench` / `3.77 ms`, whose `O(capacity)` control scan degrades **`4.6x`** per live element)!

---

## 3. Battle 2: Memory Footprint & Unreserved Insert Tail Latency (`2,000,000` Entries)

### Battle 2A: Isolated Process `VmRSS` / Live JVM Heap Footprint

| Implementation | `16B` Logical Entry (`uint64 -> uint64`) | `32B` Logical Entry (`uint64 -> 24B SessionState`) | Architectural Memory Mechanism |
| :--- | :---: | :---: | :--- |
| **Java `21` `fastutil` (`Long2Long` / `Long2Object`)** | **`66.0 MiB` (`34.6 B/entry`)** | `126.4 MiB` (`66.3 B/entry`) | Unboxed primitive `long[]` arrays (`0` `Node` / `Long` boxes) |
| **Rust `1.91` `HashMap` / C++20 `absl::flat_hash_map`** | **`68.1 MiB` (`35.7 B/entry`)** | `132.0–132.1 MiB` (`69.2–69.3 B/entry`) | Power-of-two flat table (`4,194,304` slots) + `1B/slot` ctrl array |
| **Go `1.24.0` `map` (SwissTable)** | `72.2 MiB` Heap / `72.5 MiB` RSS (`38.0 B/entry`) | `83.8 MiB` Heap / `145.2 MiB` RSS (`44.0–76.1 B/entry`) | Split-directory SwissTable sub-tables (avoids full 4M-slot jump) |
| **Go `1.22.2` `map` (Classic Buckets)** | `73.6 MiB` RSS / `76.6 MiB` Heap (`38.6–40.2 B/entry`) | `68.3 MiB` Heap / `137.4 MiB` RSS (`35.8–72.0 B/entry`) | 8-entry inline buckets (`256B`/`320B`) + `tophash[8]` |
| **C++20 `std::unordered_map` (GCC `libstdc++`)** | `77.5 MiB` (`40.6 B/entry`) | **`108.0 MiB` (`56.6 B/entry`)** | `8B` bucket ptr + `32B`/`48B` `glibc` chunk (`18%` *smaller* than flat at `32B`!) |
| **Python `3.12.3` `dict`** | `80.3 MiB` RSS (`42.1 B/entry`, table only) | `202.9 MiB` RSS (`106.4 B/entry`) | Compact `dk_indices` + `dk_entries`, plus `PyObject` boxing |
| **Java `21` `java.util.HashMap` (Boxed)** | **`169.3 MiB` (`88.7 B/entry`)** | **`200.3 MiB` (`105.0 B/entry`)** | **`2.56x` `fastutil`**: `2` boxed `Long`s + `32B` `Node` + table array |

> **Critical Insight for Episode 16 Preview:** For `16B` key-value pairs, flat SwissTables save memory (`35.7 B` vs `40.6 B`). By `32B` (`8B` key + `24B` struct), power-of-two capacity rounding (`4,194,304` slots for `2M` entries) causes `absl::flat_hash_map` and Rust `HashMap` (`132 MiB`) to consume **22% MORE memory** than node-based `std::unordered_map` (`108 MiB`)!

### Battle 2B: Streaming Insert Tail Latency (`0` $\rightarrow$ `2,000,000` Inserts)

| Implementation & Reservation | Total Time | `p50` | `p99` | `p99.9` | `p99.99` | `Max` Rehash / GC Spike |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map` (Unreserved)** | `396.8 ms` | `80 ns` | `581 ns` | `772 ns` | `3,937 ns` | **`65.76 ms`** |
| **C++20 `std::unordered_map` (Reserved)** | `289.4 ms` | `110 ns` | `521 ns` | `721 ns` | `3,978 ns` | **`26.2 µs`** (**`2,510x` lower**) |
| **C++20 `absl::flat_hash_map` (Unreserved)** | `152.8 ms` | `30 ns` | `131 ns` | `361 ns` | `471 ns` | **`25.00 ms`** |
| **C++20 `absl::flat_hash_map` (Reserved)** | `144.0 ms` | `30 ns` | `140 ns` | `370 ns` | `451 ns` | **`41.3 µs`** (**`605x` lower**) |
| **Rust `1.91` `HashMap` (Unreserved)** | `222.7 ms` | `40 ns` | `271 ns` | `380 ns` | `611 ns` | **`36.19 ms`** |
| **Rust `1.91` `HashMap` (Reserved)** | `190.2 ms` | `50 ns` | `361 ns` | `1,423 ns` | `3,577 ns` | **`18.6 µs`** (**`1,944x` lower**) |
| **Go `1.22.2` `map` (Unreserved)** | `327.7 ms` | `51 ns` | `571 ns` | `1,022 ns` | `4,549 ns` | `13.05 ms` (`46.8 µs` reserved) |
| **Go `1.24.0` `map` (Unreserved SwissTable!)** | `290.9 ms` | `50 ns` | `331 ns` | `13,175 ns` | `24,437 ns` | **`185.5 µs` (`0.186 ms`!)** |
| **Java `21` `HashMap` (Unreserved G1GC / ZGC)** | `318.3 / 362.9 ms` | `50 / 100 ns` | `411 / 441 ns` | `601 ns` | `3,346 / 4,398 ns` | `57.72 ms` / `47.69 ms` |
| **Java `21` `fastutil` (Reserved G1GC / ZGC)** | `205.7 / 233.3 ms` | `100 ns` | `350 / 361 ns` | `401 / 441 ns` | `992 / 2,776 ns` | **`23.1 µs`** / **`87.6 µs`** |

---

## 4. Battle 3: Security vs. Speed — Live Adversarial HashDoS (`40,000` Keys)

| Language & Map | Attack Payload | Normal Time | Attack Time | Impact & Mitigation Mechanism |
| :--- | :--- | :---: | :---: | :--- |
| **C++20 `std::unordered_map`** | Multiples of `libstdc++` prime `42,043` | `0.85 ms` (`1.99 ms` repro) | **`1,047.92 ms`** (`11,465 ms` `GBench`) | **`1,227.5x` (`O(N^2)`) collapse** — `40,000 / 40,000` in `bucket(0)`! |
| **C++20 `absl::flat_hash_map`** | Multiples of `libstdc++` prime `42,043` | `0.44 ms` | **`0.45 ms`** (`0.170 ms` `GBench`) | **Immune** (`~1.0x`) — per-process salted mixing (`mulx`) |
| **Rust `1.91` Default (`SipHash-1-3`)** | Both Prime & `FxHash`-targeted keys | `0.68 ms` | **`0.65–0.68 ms`** | **Immune** (`1.0x`) — cryptographic keyed PRF |
| **Rust `1.91` `FxHash` (`rustc-hash`)** | `K^-1`-targeted H1+H2 collision keys | `0.68 ms` | **`709.18 ms`** | **`1,091.6x` collapse** — why Rust restricts `FxHash` to trusted keys |
| **Java `21` `java.util.HashMap`** | `(i << 16) \| i` (`hash & 0xFFFF == 0`) | `1.17 ms` (`492 µs` JMH) | **`6.00 ms`** (`2.16 ms` JMH) | **OpenJDK `JEP 180`** — `treeifyBin()` RB-tree bounds worst case to `O(N log N)` (`5.1x`) |
| **Go `1.22.2` / `Go 1.24.0`** | Multiples of `43,067` | `0.74 / 0.55 ms` | **`0.75 / 0.77 ms`** | **Immune** (`~1.0x–1.4x`) — randomized per-map seed |
| **Python `3.12.3` `dict`** | Multiples of `43,067` | `2.44 ms` | **`1.76 ms`** | **Immune** — randomized per-process SipHash-1-3 (`PYTHONHASHSEED`) |

---

## 5. Unmultiplexed Linux `perf stat` Hardware PMU Counters (Zen 5, `100%` Uptime)

Three strictly unmultiplexed passes (`<= 4` hardware events per pass):

### 5A. L1-Resident `1,024`-Entry Lookups (`250` Passes = `500,000,000` Lookups)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Loads | L1D Misses | LLC Misses |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `28.27B` | `15.56B` | `0.55` | **`443.66M`** | `12.10B` | **`999.71M`** | `7.17M` |
| **C++20 `absl::flat_hash_map`** | **`9.27B`** | `23.20B` | **`2.50`** | **`118.66M`** | `8.51B` | **`266.97M`** | `4.48M` |
| **Rust `1.91` `SipHash-1-3`** | `17.52B` | **`50.35B`** | **`2.87`** | `114.11M` | `5.50B` | `194.86M` | `4.82M` |
| **Rust `1.91` `FxHash`** | **`8.22B`** | **`14.50B`** | `1.76` | `127.73M` | `3.67B` | `189.80M` | `3.74M` |

### 5B. Aged `2,000,000`-Entry `80/20` Lookups (`40` Passes = `80,000,000` Lookups; Python `6` Passes = `12M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `17.41B` | `4.22B` | `0.24` | `79.16M` | `366.07M` | **`200.40M`** | **`361.37M`** |
| **C++20 `absl::flat_hash_map`** | **`6.73B`** | `4.72B` | **`0.70`** | **`27.86M`** | `249.09M` | **`84.16M`** | `227.48M` |
| **Rust `1.91` `SipHash-1-3`** | `15.28B` | `9.32B` | `0.61` | `28.38M` | `204.59M` | `67.89M` | `181.50M` |
| **Rust `1.91` `FxHash`** | **`5.87B`** | **`3.27B`** | `0.56` | `27.91M` | `203.51M` | **`67.64M`** | **`174.78M`** |
| **Go `1.22.2` `map`** | `20.70B` | `11.51B` | `0.56` | `82.37M` | `363.85M` | `75.64M` | **`349.65M`** |
| **Go `1.24.0` `map` (SwissTable)** | `15.88B` | `10.21B` | `0.64` | **`29.82M`** | `332.68M` | `71.33M` | **`166.20M`** (`2.10x` drop) |
| **Java `21` `HashMap<Long, Long>`** | `26.85B` | `10.91B` | `0.41` | `119.14M` | `466.26M` | `147.61M` | **`361.95M`** |
| **Java `21` `fastutil` `Long2Long`** | `19.69B` | `8.17B` | `0.42` | `151.93M` | `364.71M` | `148.84M` | `242.71M` |
| **Python `3.12` `dict` (`6` passes)** | `62.21B` | `118.56B` | `1.91` | `76.77M` | `908.62M` | `70.48M` | `193.53M` |

### 5C. Full-Table `2,000,000`-Entry Iteration (`40` Sweeps = `80M` Live; Python `10` = `20M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `16.33B` | `2.18B` | **`0.13`** | `8.98M` | `119.45M` | **`27.05M`** | **`76.17M`** |
| **C++20 `absl::flat_hash_map`** | `2.57B` | `2.37B` | `0.92` | `82.90M` | `66.45M` | `2.94M` | `7.96M` |
| **Rust `1.91` `SipHash-1-3`** | `1.26B` | `1.91B` | **`1.51`** | **`18.78M`** | **`65.59M`** | `2.89M` | **`6.24M`** |
| **Rust `1.91` `FxHash`** | **`1.22B`** | **`1.72B`** | `1.41` | **`18.60M`** | `65.57M` | `2.88M` | `6.77M` |
| **Go `1.22.2` `map`** | `3.87B` | `11.52B` | `2.98` | `32.68M` | `73.16M` | **`2.69M`** | `10.04M` |
| **Go `1.24.0` `map` (SwissTable)** | `3.98B` | `10.69B` | `2.69` | `28.25M` | `70.17M` | **`2.65M`** | `10.03M` |
| **Java `21` `HashMap<Long, Long>`** | `19.02B` | `11.98B` | `0.63` | `168.66M` | `298.91M` | `12.76M` | `60.70M` |
| **Java `21` `fastutil` `Long2Long`** | `12.79B` | `9.26B` | `0.72` | `130.30M` | `193.17M` | `8.83M` | `28.55M` |
| **Python `3.12` `dict` (`10` passes)** | `34.06B` | `102.45B` | `3.01` | `60.42M` | `497.26M` | `9.93M` | `80.28M` |

### 5D. Post-Peak Sparse `200,000`-Entry Iteration (`90%` Evicted, `200` Sweeps = `40M` Live; Python `40` = `8M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `5.97B` | `2.09B` | `0.35` | **`9.38M`** | `89.37M` | `27.53M` | `68.14M` |
| **C++20 `absl::flat_hash_map`** | `5.03B` | **`7.09B`** (`3.4x` `std`) | `1.41` | `48.61M` | `101.86M` | `7.53M` | `56.15M` |
| **Rust `1.91` `SipHash-1-3`** | `2.50B` | `2.01B` | `0.80` | `46.65M` | **`78.43M`** | `7.48M` | `50.18M` |
| **Rust `1.91` `FxHash`** | **`2.21B`** | **`1.71B`** | `0.77` | `46.65M` | `78.46M` | `7.47M` | `49.85M` |
| **Go `1.22.2` `map`** | `6.16B` | `18.28B` | `2.97` | `55.55M` | `270.80M` | `7.25M` | **`19.62M`** |
| **Go `1.24.0` `map` (SwissTable)** | `8.23B` | `10.38B` | `1.26` | `51.93M` | `246.68M` | **`7.10M`** | `57.19M` (`2.91x` `1.22`) |
| **Java `21` `HashMap<Long, Long>`** | `21.73B` | `13.42B` | `0.62` | `122.03M` | `319.20M` | `22.26M` | `123.40M` |
| **Java `21` `fastutil` `Long2Long`** | `15.45B` | `12.63B` | `0.82` | `139.77M` | `319.95M` | `12.43M` | `51.23M` |
| **Python `3.12` `dict` (`40` passes)** | `34.18B` | `80.87B` | `2.37` | `53.56M` | `476.10M` | `14.53M` | `100.83M` |

---

## 6. Verified Primary Citations (Slide Citation Bar Sources)

- `Klink & Wälde (28C3, 2011) — HashDoS Web Application Platform Attacks`
- `Aumasson & Bernstein (INDOCRYPT 2012) — SipHash: A Fast Short-Input PRF`
- `OpenJDK JEP 180 (Mike Duigou, 2014) — java.util.HashMap::treeifyBin()`
- `CPython 3.6 (Raymond Hettinger, 2016) — Objects/dictobject.c (Compact Dict)`
- `WG21 P1863R1 (Titus Winters, 2020) — "ABI — Now or Never"`
- `Go Proposal #54766 (Michael Pratt, Go 1.24) — SwissTable Runtime Map`
