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

> **Episode 15 Video Focus:** Deep architectural study of **C++20 (`std::unordered_map` & `absl::flat_hash_map`) vs. Rust 1.91 (`HashMap` with `SipHash-1-3` & `FxHash`) vs. Java 21 LTS (`java.util.HashMap` & `fastutil Long2LongOpenHashMap`)** (additional Go 1.22/1.24 and Python 3.12 reference rows are preserved below for full lab reproducibility).  
> **Hardware Testbed:** AMD Ryzen 9 8945HX (Zen 4, 16 Cores / 32 Threads, `512 KiB` L1D, `16 MiB` L2, `64 MiB` L3) • `Linux 7.0.0-43-generic x86_64` (`kernel.perf_event_paranoid = -1`)  
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

| Runtime & Container | L1 `1K` (`80/20` Req/s) | Aged `2M` (`80/20` Hit/Miss) | Aged `2M` (`100%` Negative Miss) | Aged `2M` (`24B` `SessionState`) |
| :--- | :---: | :---: | :---: | :---: |
| **C++20 `absl::flat_hash_map`** | **`288.4M/s`** (`407.9M/s` `GBench`) | **`68.94M/s`** (`76.21M/s` `GBench`) | **`161.7M/s`** (`180.0M/s` `GBench`) | **`53.87M/s`** (`58.71M/s` `GBench`) |
| **Rust `1.91` `HashMap` (`FxHash`)** | **`325.5M/s`** (`3.07 ns`) | **`83.03M/s`** (`12.04 ns`) | **`270.9M/s`** (`3.69 ns`) | **`67.35M/s`** (`14.85 ns`) |
| **Rust `1.91` `HashMap` (`SipHash-1-3`, Default)** | **`152.2M/s`** (`6.57 ns`) | `30.62M/s` (`32.65 ns`) | **`109.2M/s`** (`9.16 ns`) | `23.70M/s` (`42.19 ns`) |
| **C++20 `std::unordered_map` (Default)** | `94.28M/s` (`102.2M/s` `GBench`) | `26.56M/s` (`30.30M/s` `GBench`) | `28.83M/s` (`34.32M/s` `GBench`) | `21.26M/s` (`23.41M/s` `GBench`) |
| **Go `1.24.0` `map` (SwissTable `#54766`)** | **`125.8M/s`** (`7.95 ns`) | `29.08M/s` (`34.38 ns`) | `36.57M/s` (`27.35 ns`) | `20.70M/s` (`48.32 ns`) |
| **Go `1.22.2` `map` (8-Slot Buckets)** | `73.67M/s` (`13.57 ns`) | `21.74M/s` (`45.99 ns`) | `22.50M/s` (`44.45 ns`) | `18.95M/s` (`52.78 ns`) |
| **Java `21` `fastutil` `Long2Long` / `Long2Object`** | `154.3M/s` (`162.8M/s` `JMH`) | **`47.18M/s`** (`46.96M/s` `JMH`) | `60.70M/s` (`58.55M/s` `JMH`) | `30.47M/s` (`30.57M/s` `JMH`) |
| **Java `21` `java.util.HashMap` (Default)** | `186.5M/s` (`127.1M/s` `JMH`) | `29.12M/s` (`26.53M/s` `JMH`) | `60.85M/s` (`46.14M/s` `JMH`) | `20.86M/s` (`18.95M/s` `JMH`) |
| **Python `3.12.3` `dict` (Compact Split)** | `13.40M/s` (`74.64 ns`) | `2.53M/s` (`394.7 ns`) | `9.38M/s` (`106.6 ns`) | `1.66M/s` (`603.5 ns`) |

### Key Empirical Insights from Battle 1
1. **L1 Paradox:** Even **Rust's cryptographic default (`152.2M/s`, `6.57 ns`)**, **Java 21 `HashMap` (`186.5M/s`)**, and **Go 1.24 (`125.8M/s`)** outperform C++'s `std::unordered_map` (`94.3M/s`, `10.61 ns`) on L1-hot integer lookups because GCC `libstdc++` executes a hardware `div` instruction by its prime bucket count on every lookup plus pointer indirection (`443.4M` branch misses over `250` passes).
2. **Negative Lookups Expose Open Addressing:** When `2,000,000` queries miss in DRAM, SwissTable `EMPTY` (`0x80`) SIMD control tags terminate after reading 1 control line: **`270.9M/s`** (`Rust FxHash`), **`161.7M/s` / `180.0M/s` `GBench`** (`absl::flat_hash_map`), and even **`109.2M/s`** (`Rust SipHash-1-3`—**`3.8x`** faster than `std::unordered_map`'s `28.83M/s`!).
3. **Go 1.22 $\rightarrow$ Go 1.24 SwissTable Leap:** Migrating Go's runtime map to SwissTables (`#54766`) improved L1 lookups **`1.71x`** (`73.7M/s` $\rightarrow$ `125.8M/s`), aged `80/20` lookups **`1.34x`** (`21.74M/s` $\rightarrow$ `29.08M/s`), and cut LLC `cache-misses` **`2.13x`** (`350.74M` $\rightarrow$ `164.55M`).

---

## 2B. Battle 1B: Read Throughput — Full-Table (`2M`) vs. Post-Peak Sparse (`200K`) Iteration

Periodic TTL expiration sweeps, billing rollups, and snapshot exporters iterate every live entry (`sum += k ^ v`, checksum `0x408f34c03517a024` full / `0x9c5a519a826e497` sparse across C++/Rust/Go after evicting `90%` of entries without shrinking capacity):

| Runtime & Container | Full-Table `2M` Sweep (`ms/swp` / `M/s`) | Post-Peak Sparse `200K` (`90%` Evicted, `ms/swp` / `M/s`) | Traversal Complexity & Cache Physics |
| :--- | :---: | :---: | :--- |
| **Rust `1.91` `HashMap` (`SipHash-1-3`)** | **`2.45 ms`** (`816.1 M/s`) | `1.26 ms` (`158.6 M/s`) | `hashbrown` SIMD `RawIter`: skips 16 empty control tags/instr (`1.51` IPC full) |
| **Rust `1.91` `HashMap` (`FxHash`)** | **`2.49 ms`** (`803.3 M/s`) | **`1.15 ms`** (`174.6 M/s`) | Identical layout to SipHash (hashing never runs during iteration!) |
| **C++20 `absl::flat_hash_map`** | `8.34 ms` (`8.04 ms` `GBench`, `239.8 M/s`) | `3.94 ms` (`2.60 ms` L3-warm `GBench`, `50.7–76.9 M/s`) | `O(capacity)`: must execute **`7.10B` insns** scanning all `4.19M` ctrl bytes at `4.8%` fill (`4.7x` slower/live) |
| **Java `21` `fastutil` `Long2Long`** | `11.62 ms` (`172.1 M/s`, `5.76 ns` `JMH`) | `6.21 ms` (`32.2 M/s`, `22.37 ns` `JMH`) | `fastIterator()` scans contiguous `long[]` without allocation (`O(capacity)`) |
| **Go `1.22.2` `map` (8-Slot Buckets)** | `13.01 ms` (`153.8 M/s`) | `4.44 ms` (`45.0 M/s`) | Beats `Go 1.24.0` by **`41%`** on sparse sweeps (`19.48M` vs `56.01M` LLC misses) |
| **Go `1.24.0` `map` (SwissTable)** | `13.76 ms` (`145.4 M/s`) | `6.28 ms` (`31.9 M/s`) | Walks unshrunk split-directory sub-tables + control words |
| **Java `21` `java.util.HashMap`** | `39.08 ms` (`51.2 M/s`, `7.40 ns` `JMH`) | `9.10 ms` (`22.0 M/s`, `25.30 ns` `JMH`) | `O(capacity)` bucket array scan (`G1GC` young locality boosts unaged `JMH` sweep) |
| **C++20 `std::unordered_map`** | **`78.83 ms`** (`61.7 ms` `GBench`, `25.4 M/s`) | **`3.86 ms`** (`3.80 ms` `GBench`, `51.8–52.7 M/s`) | **`32x` behind Rust** full (`0.13` IPC chasing fragmented nodes), yet **BEATS `absl`** (`O(live)` `2.08B` insns) standalone sparse! |
| **Python `3.12.3` `dict`** | `191.51 ms` (`10.4 M/s`) | `37.93 ms` (`5.3 M/s`) | Scans `dk_entries` including `DKIX_DUMMY` tombstones |

> **Key Empirical Flip (Why Single-Op Benchmarks Mislead):** At `2M` full occupancy, C++'s `std::unordered_map` (`78.83 ms`, `61.7 ms` `GBench`) trails even `Java fastutil` (`11.62 ms`) and `Rust` (`2.45 ms`, **`32x` faster**) because node pointers scatter across DRAM. Yet after `90%` TTL eviction inside an unshrunk peak table (`200K` live in `4.19M` capacity), `std::unordered_map` walks **only live nodes (`O(live)`, `2.08B` instructions)**, speeding up **`20.4x`** to **`3.86 ms`** and beating standalone `absl::flat_hash_map` (`3.94 ms`, which executes **`7.10B` instructions**—`3.41x` more—scanning empty/deleted control slots, though tight L3-warm `GBench` loops run `2.60 ms` vs `3.80 ms`)!

---

## 3. Battle 2: Memory Footprint & Unreserved Insert Tail Latency (`2,000,000` Entries)

### Battle 2A: Isolated Process `VmRSS` / Live JVM Heap Footprint

| Implementation | `16B` Logical Entry (`uint64 -> uint64`) | `32B` Logical Entry (`uint64 -> 24B SessionState`) | Architectural Memory Mechanism |
| :--- | :---: | :---: | :--- |
| **Java `21` `fastutil` (`Long2Long` / `Long2Object`)** | **`66.0 MiB` (`34.6 B/entry`)** | `126.4 MiB` (`66.3 B/entry`) | Unboxed primitive `long[]` arrays (`0` `Node` / `Long` boxes) |
| **Rust `1.91` `HashMap` / C++20 `absl::flat_hash_map`** | **`68.1 MiB` (`35.7 B/entry`)** | `132.0–132.1 MiB` (`69.2–69.3 B/entry`) | Power-of-two flat table (`4,194,304` slots) + `1B/slot` ctrl array |
| **Go `1.22.2` `map` (Classic Buckets)** | `71.2 MiB` RSS / `76.6 MiB` Heap (`37.3–40.2 B/entry`) | `68.3 MiB` Heap / `137.5 MiB` RSS (`35.8–72.1 B/entry`) | 8-entry inline buckets (`256B`/`320B`) + `tophash[8]` |
| **Go `1.24.0` `map` (SwissTable)** | `72.2 MiB` Heap / `72.5 MiB` RSS (`37.8–38.0 B/entry`) | `83.8 MiB` Heap / `145.1 MiB` RSS (`44.0–76.1 B/entry`) | Split-directory SwissTable sub-tables (avoids full 4M-slot jump) |
| **C++20 `std::unordered_map` (GCC `libstdc++`)** | `77.5 MiB` (`40.6 B/entry`) | **`108.0 MiB` (`56.6 B/entry`)** | `8B` bucket ptr + `32B`/`48B` `glibc` chunk (`18%` *smaller* than flat at `32B`!) |
| **Python `3.12.3` `dict`** | `80.3 MiB` RSS (`42.1 B/entry`, table only) | `202.9 MiB` RSS (`106.4 B/entry`) | Compact `dk_indices` + `dk_entries`, plus `PyObject` boxing |
| **Java `21` `java.util.HashMap` (Boxed)** | **`169.3 MiB` (`88.7 B/entry`)** | **`200.3 MiB` (`105.0 B/entry`)** | **`2.56x` `fastutil`**: `2` boxed `Long`s + `32B` `Node` + table array |

> **Critical Insight for Episode 16 Preview:** For `16B` key-value pairs, flat SwissTables save memory (`35.7 B` vs `40.6 B`). By `32B` (`8B` key + `24B` struct), power-of-two capacity rounding (`4,194,304` slots for `2M` entries) causes `absl::flat_hash_map` and Rust `HashMap` (`132.0–132.1 MiB`) to consume **22% MORE memory** than node-based `std::unordered_map` (`108.0 MiB`)!

### Battle 2B: Streaming Insert Tail Latency (`0` $\rightarrow$ `2,000,000` Inserts)

| Implementation & Reservation | Total Time | `p50` | `p99` | `p99.9` | `p99.99` | `Max` Rehash / GC Spike |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map` (Unreserved)** | `386.1 ms` | `80 ns` | `581 ns` | `791 ns` | `4,038 ns` | **`70.00 ms`** |
| **C++20 `std::unordered_map` (Reserved)** | `265.6 ms` | `100 ns` | `491 ns` | `691 ns` | `3,858 ns` | **`157.7 µs`** (**`444x` lower**) |
| **C++20 `absl::flat_hash_map` (Unreserved)** | `145.8 ms` | `30 ns` | `130 ns` | `350 ns` | `451 ns` | **`25.16 ms`** |
| **C++20 `absl::flat_hash_map` (Reserved)** | `136.4 ms` | `30 ns` | `130 ns` | `350 ns` | `431 ns` | **`171.0 µs`** (**`147x` lower**) |
| **Rust `1.91` `HashMap` (Unreserved)** | `221.6 ms` | `40 ns` | `271 ns` | `381 ns` | `761 ns` | **`35.71 ms`** |
| **Rust `1.91` `HashMap` (Reserved)** | `191.2 ms` | `50 ns` | `360 ns` | `1,413 ns` | `3,507 ns` | **`15.6 µs`** (**`2,289x` lower**) |
| **Go `1.22.2` `map` (Unreserved)** | `339.4 ms` | `60 ns` | `601 ns` | `1,052 ns` | `4,659 ns` | `14.14 ms` (`74.5 µs` reserved) |
| **Go `1.24.0` `map` (Unreserved SwissTable!)** | `294.2 ms` | `50 ns` | `340 ns` | `13,156 ns` | `24,728 ns` | **`134.2 µs` (`0.134 ms`!)** |
| **Java `21` `HashMap` (Unreserved G1GC / ZGC)** | `311.2 / 360.3 ms` | `40 / 100 ns` | `401 / 441 ns` | `571 / 601 ns` | `3,025 / 4,389 ns` | **`57.80 ms`** / **`44.38 ms`** |
| **Java `21` `HashMap` (Reserved G1GC / ZGC)** | `180.2 / 258.3 ms` | `40 / 101 ns` | `351 / 411 ns` | `491 / 561 ns` | `1,052 / 3,587 ns` | **`351.6 µs`** / **`1.29 ms`** |
| **Java `21` `fastutil` (Unreserved G1GC / ZGC)** | `180.5 / 195.4 ms` | `40 / 40 ns` | `271 / 301 ns` | `400 / 401 ns` | `561 / 521 ns` | `13.28 ms` / `14.06 ms` |
| **Java `21` `fastutil` (Reserved G1GC / ZGC)** | `213.0 / 219.2 ms` | `100 ns` | `341 / 351 ns` | `401 ns` | `1,002 / 1,273 ns` | **`97.0 µs`** / **`6.8 µs`** |
| **Python `3.12.3` `dict` (Unreserved)** | `500.4 ms` | `80 ns` | `411 ns` | `541 ns` | `3,918 ns` | `37.32 ms` |

---

## 4. Battle 3: Security vs. Speed — Live Adversarial HashDoS (`40,000` Keys)

| Language & Map | Attack Payload | Normal Time | Attack Time | Impact & Mitigation Mechanism |
| :--- | :--- | :---: | :---: | :--- |
| **C++20 `std::unordered_map`** | Multiples of `libstdc++` prime `42,043` | `0.83 ms` (`1.21 ms` repro) | **`1,043.32 ms`** (`11,318 ms` `GBench`) | **`1,251x` (`O(N^2)`) collapse** — `40,000 / 40,000` in `bucket(0)`! |
| **C++20 `absl::flat_hash_map`** | Multiples of `libstdc++` prime `42,043` | `0.43 ms` | **`0.43 ms`** (`0.171 ms` `GBench`) | **Immune** (`1.0x`) — per-process salted mixing (`mulx`) |
| **Rust `1.91` Default (`SipHash-1-3`)** | Both Prime & `FxHash`-targeted keys | `0.67 ms` | **`0.64–0.67 ms`** | **Immune** (`1.0x`) — cryptographic keyed PRF |
| **Rust `1.91` `FxHash` (`rustc-hash`)** | `K^-1`-targeted H1+H2 collision keys | `0.67 ms` | **`689.36 ms`** | **`1,072x` collapse** — why Rust restricts `FxHash` to trusted keys |
| **Java `21` `java.util.HashMap`** | `(i << 16) \| i` (`hash & 0xFFFF == 0`) | `1.18 ms` (`489 µs` `JMH`) | **`5.86 ms`** (`2.14 ms` `JMH`) | **OpenJDK `JEP 180`** — `treeifyBin()` RB-tree bounds worst case to `O(N log N)` (`5.0x`) |
| **Go `1.22.2` / `Go 1.24.0`** | Multiples of `43,067` | `0.78 / 0.75 ms` | **`0.74 / 0.48 ms`** | **Immune** (`<= 1.0x`) — randomized per-map seed |
| **Python `3.12.3` `dict`** | Multiples of `43,067` | `2.53 ms` | **`1.75 ms`** | **Immune** — randomized per-process SipHash-1-3 (`PYTHONHASHSEED`) |

---

## 5. Unmultiplexed Linux `perf stat` Hardware PMU Counters (Zen 4, `100%` Uptime)

Three strictly unmultiplexed passes (`<= 4` hardware events per pass):

### 5A. L1-Resident `1,024`-Entry Lookups (`250` Passes = `500,000,000` Lookups)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Loads | L1D Misses | LLC Misses |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `28.27B` | `15.55B` | `0.55` | **`443.40M`** | `12.15B` | **`1,000.99M`** | `6.06M` |
| **C++20 `absl::flat_hash_map`** | **`9.28B`** | `23.20B` | **`2.50`** | **`118.68M`** | `8.51B` | **`264.03M`** | `4.62M` |
| **Rust `1.91` `SipHash-1-3`** | `17.51B` | **`50.35B`** | **`2.88`** | `113.85M` | `5.47B` | `192.49M` | `4.75M` |
| **Rust `1.91` `FxHash`** | **`8.23B`** | **`14.50B`** | `1.76` | `127.72M` | `3.67B` | `189.61M` | `3.69M` |

### 5B. Aged `2,000,000`-Entry `80/20` Lookups (`40` Passes = `80,000,000` Lookups; Python `6` Passes = `12M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `17.16B` | `4.22B` | `0.25` | `79.21M` | `365.22M` | **`206.86M`** | **`360.21M`** |
| **C++20 `absl::flat_hash_map`** | **`6.68B`** | `4.73B` | **`0.71`** | **`28.01M`** | `248.98M` | **`84.20M`** | `225.23M` |
| **Rust `1.91` `SipHash-1-3`** | `14.93B` | `9.30B` | `0.62` | `28.19M` | `205.84M` | `67.83M` | `180.30M` |
| **Rust `1.91` `FxHash`** | **`5.83B`** | **`3.26B`** | `0.56` | `27.85M` | `203.57M` | **`67.61M`** | **`174.34M`** |
| **Go `1.22.2` `map`** | `20.61B` | `11.48B` | `0.56` | `82.36M` | `364.03M` | `75.70M` | **`350.74M`** |
| **Go `1.24.0` `map` (SwissTable)** | `15.89B` | `10.22B` | `0.64` | **`29.84M`** | `332.52M` | `64.31M` | **`164.55M`** (`2.13x` drop) |
| **Java `21` `HashMap<Long, Long>`** | `27.96B` | `10.95B` | `0.39` | `119.39M` | `464.76M` | `146.85M` | **`359.19M`** |
| **Java `21` `fastutil` `Long2Long`** | `19.53B` | `8.16B` | `0.42` | `152.12M` | `359.53M` | `140.50M` | `239.85M` |
| **Python `3.12` `dict` (`6` passes)** | `62.81B` | `118.75B` | `1.89` | `77.28M` | `555.52M` | `69.31M` | `194.24M` |

### 5C. Full-Table `2,000,000`-Entry Iteration (`40` Sweeps = `80M` Live; Python `10` = `20M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `16.18B` | `2.18B` | **`0.13`** | `9.00M` | `119.44M` | **`27.84M`** | **`76.17M`** |
| **C++20 `absl::flat_hash_map`** | `2.56B` | `2.37B` | `0.93` | `82.82M` | `66.39M` | `2.90M` | `7.54M` |
| **Rust `1.91` `SipHash-1-3`** | `1.25B` | `1.89B` | **`1.51`** | **`18.62M`** | **`65.68M`** | `2.52M` | **`6.09M`** |
| **Rust `1.91` `FxHash`** | **`1.20B`** | **`1.72B`** | `1.43` | **`18.58M`** | `65.46M` | `2.87M` | `6.52M` |
| **Go `1.22.2` `map`** | `3.86B` | `11.52B` | `2.99` | `33.08M` | `72.74M` | **`2.67M`** | `9.83M` |
| **Go `1.24.0` `map` (SwissTable)** | `3.96B` | `10.69B` | `2.70` | `28.29M` | `70.34M` | **`2.66M`** | `9.99M` |
| **Java `21` `HashMap<Long, Long>`** | `18.30B` | `11.96B` | `0.65` | `169.13M` | `297.89M` | `11.84M` | `56.22M` |
| **Java `21` `fastutil` `Long2Long`** | `11.84B` | `9.24B` | `0.78` | `130.27M` | `186.72M` | `8.05M` | `27.96M` |
| **Python `3.12` `dict` (`10` passes)** | `35.07B` | `102.83B` | `2.93` | `60.09M` | `504.42M` | `9.91M` | `78.77M` |

### 5D. Post-Peak Sparse `200,000`-Entry Iteration (`90%` Evicted, `200` Sweeps = `40M` Live; Python `40` = `8M`)

| Implementation | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **C++20 `std::unordered_map`** | `5.98B` | **`2.08B`** | `0.35` | **`9.37M`** | `89.22M` | `17.35M` | `68.33M` |
| **C++20 `absl::flat_hash_map`** | `4.67B` | **`7.10B`** (`3.41x` `std`) | `1.52` | `48.69M` | `101.75M` | `7.53M` | `56.37M` |
| **Rust `1.91` `SipHash-1-3`** | `2.52B` | `2.01B` | `0.80` | `46.78M` | **`78.49M`** | `7.46M` | `50.05M` |
| **Rust `1.91` `FxHash`** | **`2.14B`** | **`1.70B`** | `0.80` | `46.61M` | `78.42M` | `7.45M` | `49.57M` |
| **Go `1.22.2` `map`** | `6.24B` | `18.28B` | `2.93` | `55.48M` | `270.03M` | `7.25M` | **`19.48M`** |
| **Go `1.24.0` `map` (SwissTable)** | `8.36B` | `10.37B` | `1.24` | `51.67M` | `246.22M` | **`7.14M`** | `56.01M` (`2.87x` `1.22`) |
| **Java `21` `HashMap<Long, Long>`** | `21.92B` | `13.43B` | `0.61` | `122.25M` | `316.79M` | `21.97M` | `122.19M` |
| **Java `21` `fastutil` `Long2Long`** | `15.62B` | `12.69B` | `0.81` | `114.39M` | `317.09M` | `13.01M` | `48.33M` |
| **Python `3.12` `dict` (`40` passes)** | `33.38B` | `80.95B` | `2.43` | `53.52M` | `490.42M` | `13.92M` | `99.50M` |

---

## 6. Verified Primary Citations (Slide Citation Bar Sources)

- `Klink & Wälde (28C3, 2011) — HashDoS Web Application Platform Attacks`
- `Aumasson & Bernstein (INDOCRYPT 2012) — SipHash: A Fast Short-Input PRF`
- `OpenJDK JEP 180 (Mike Duigou, 2014) — java.util.HashMap::treeifyBin()`
- `CPython 3.6 (Raymond Hettinger, 2016) — Objects/dictobject.c (Compact Dict)`
- `Matt Kulukundis (CppCon 2017) — Google Fleet Telemetry (~1% CPU, >4% RAM in C++ Hash Tables)`
- `WG21 P1863R1 (Titus Winters, 2020) — "ABI — Now or Never"`
- `Go Proposal #54766 (Michael Pratt, Go 1.24) — SwissTable Runtime Map`
