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

# Episode 16: Production C++ Hash Maps — Bare-Metal Forensic Telemetry Report

> **Episode 16 Video Thesis:** Stepping inside a realistic **Envoy-style Edge HTTP/RPC Gateway & Connection Router**, Episode 16 moves beyond toy integer benchmarks to answer a practical engineering question that comes up whenever a C++ service profiles a hot hash table: *what should we fix first before adding dependencies, when does a flat vector outperform hash maps, how do Abseil SwissTable, Meta Folly F14, and Boost.Unordered behave on large structs and millions of steady-state evictions, and how do we migrate off `std::unordered_map` without shipping silent Use-After-Free bugs?*
>
> **Hardware Testbed:** AMD Ryzen 9 8945HX (Zen 4, 16 Cores / 32 Threads, `512 KiB` L1D, `16 MiB` L2, `64 MiB` L3) • `Linux 7.0.0-43-generic x86_64` (`kernel.perf_event_paranoid = -1`)  
> **Toolchains & BCR Dependencies:** `GCC 13.3.0` & `Clang 18.1.3` (`-std=c++20 -O3 -march=native`), `Bazel 9.3.2` (Bzlmod), `Abseil 20260526.0` (containers, `absl::log`, `absl::flags`, `absl::StrFormat`, `absl::Span`), `Meta Folly 2026.08.31.00`, `Boost.Unordered 1.90.0.bcr.1`, `Google Benchmark 1.9.5`, `GoogleTest 1.18.0`  
> **Parity & Hygiene Verification:** `//common/cpp_hash_maps:cpp_hash_maps_test` passes bit-for-bit identical checksums across all 15 container variants under both `GCC 13.3.0` and `Clang 18.1.3`; `clang-format --dry-run --Werror -style=Google` passes with zero `<iostream>` usage (100% Abseil `LOG(INFO)`/`LOG(ERROR)` + `ABSL_FLAG`).  
> **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/babYarMhf`](https://godbolt.org/z/babYarMhf)  
> **Intel-Syntax Assembly Archive:** [`assembly/godbolt_comparison.s`](./assembly/godbolt_comparison.s) & [`assembly/godbolt_source.cc`](./assembly/godbolt_source.cc)

---

## 1. Story 1: "Before You Add a Single Dependency" — Hidden Lookup Allocations & GCC's `noexcept` Hash Trap

### Production Scenario
An Envoy-style HTTP/RPC gateway holds **`100,000` active routes** keyed by realistic **78–82 byte multi-tenant non-SSO strings** (e.g., `/api/v2/payments/prod-us-east-1/merchants/tenant-0004129/v1/transactions/authorize`, strictly exceeding GCC `libstdc++`'s 15-byte SSO buffer) mapped to a `24B` `RoutePolicy`, built on a fragmented aged heap (`250,000` mixed `32B–256B` churn chunks + interleaved request allocations). Incoming requests slice zero-copy `std::string_view` paths directly out of the socket receive buffer (`1,000,000` queries/pass across Zipfian $s=0.99$ `70/30` hot skew, Uniform aged `70/30`, and `100%` 404 bot-scan misses).

### 1A. Exact Global `operator new` Allocation Audit (`1,000,000` `const char*` Route Lookups — `live_alloc_audit`)

| Container Configuration | `operator new` Calls / 1M Finds | Heap Bytes Allocated | Parity Checksum |
| :--- | ---: | ---: | :---: |
| **`std::unordered_map<std::string, RoutePolicy>` (Default)** | **`1,000,000`** | **`81.67 MB`** | `0xec40b6c0b41bc2ec` |
| **`std::unordered_map` (Hash Only, Missing `std::equal_to<>`)** | **`1,000,000`** | **`81.67 MB`** | `0xec40b6c0b41bc2ec` |
| **`boost::unordered_flat_map<std::string, RoutePolicy>` (Default!)** | **`1,000,000`** | **`81.67 MB`** | `0xec40b6c0b41bc2ec` |
| **`std::unordered_map` + C++20 `is_transparent` (`P0919R3`/`P1690R1`)** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |
| **`boost::unordered_flat_map` + `TransparentBoostStringHash`** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |
| **`absl::flat_hash_map<std::string, RoutePolicy>` (Default)** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |
| **`folly::F14FastMap<std::string, RoutePolicy>` (Default)** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |

> **Practical Boost Gotcha:** Unlike `absl::flat_hash_map` and `folly::F14FastMap` (which enable transparent `std::string_view` lookup by default), `boost::unordered_flat_map<std::string, V>` defaults to `std::equal_to<Key>`—requiring an explicit transparent hasher (`using is_transparent = void; using is_avalanching = void;`) + `std::equal_to<>` to eliminate temporary `std::string` allocations.

### 1B. Throughput Across Zipfian Skew, Uniform Aged Heap & 100% Negative Misses (`challenge` & `Google Benchmark`)

| Container & Configuration | Zipfian `70/30` (`challenge` / `GBench`) | Uniform Aged `70/30` | `100%` 404 Negative Miss (`challenge` / `GBench`) |
| :--- | :---: | :---: | :---: |
| **`std::unordered_map` (Default, tmp `std::string`)** | `11.80 M/s` (`84.8 ns`) / `11.92 M/s` | `7.65 M/s` (`130.7 ns`) | `13.73 M/s` (`72.9 ns`) / `13.88 M/s` |
| **`std::unordered_map` (C++20 `is_transparent` — cached hash)** | **`16.00 M/s` (`62.5 ns`)** / **`18.09 M/s`** (`+36%–52%`) | **`13.43 M/s` (`74.4 ns`)** (`1.76×`) | **`24.48 M/s` (`40.9 ns`)** / **`22.47 M/s`** (**`1.78×`**) |
| **`std::unordered_map` (`noexcept` hash trap!)** | `13.68 M/s` (`73.1 ns`) | `10.48 M/s` (`95.4 ns`) | **`10.68 M/s` (`93.7 ns`)** / **`10.32 M/s`** (**`2.29×` slower** than cached!) |
| **`std::pmr::unordered_map` (Monotonic Arena, zero deps)** | **`24.33 M/s` (`41.1 ns`)** / `23.06 M/s` (`2.1×`) | **`22.34 M/s` (`44.8 ns`)** (`2.9×`) | `28.55 M/s` (`35.0 ns`) |
| **`absl::flat_hash_map` (16-way SwissTable SIMD)** | **`32.32 M/s` (`30.9 ns`)** / **`43.28 M/s`** (**`2.7×–3.6×`**) | **`24.54 M/s` (`40.8 ns`)** (**`3.2×`**) | **`106.93 M/s` (`9.4 ns`)** / `106.04 M/s` (**`7.8×`**) |
| **`folly::F14FastMap` (14-way `F14VectorMap`, `12` slots/line)** | `30.25 M/s` / **`35.04 M/s`** | `26.59 M/s` | **`117.31 M/s` (`8.5 ns`)** / **`120.95 M/s`** (**`8.5×–8.7×`**) |
| **`folly::F14ValueMap` (14-way inline)** | **`32.92 M/s`** / `33.90 M/s` | `25.09 M/s` | **`113.73 M/s` (`8.8 ns`)** |
| **`boost::unordered_flat_map` (15-way `group15` + `ofw`)** | `30.37 M/s` (`32.9 ns`) / `34.16 M/s` | **`26.64 M/s`** (`3.5×`) | **`85.97 M/s` (`11.6 ns`)** / **`89.23 M/s`** (**`6.3×–6.4×`**) |

> **Real-World GCC `libstdc++` Discovery (Verified Directly in `/usr/include/c++/13/bits/hashtable.h:47-52` & Live Godbolt [`z/babYarMhf`](https://godbolt.org/z/babYarMhf)):**
> GCC decides whether to cache the 64-bit hash inside each `std::unordered_map` node using `__cache_default = !(__is_fast_hash<_Hash> && __is_nothrow_invocable<...>)`. Because `std::__is_fast_hash<UserHash>` already defaults to `true` for all user-defined hashers (only `std::hash<std::string>` is explicitly specialized to `false`), omitting `noexcept` forces GCC to cache the 8-byte hash only because the C++ Standard forbids `erase(const_iterator)` from throwing. Adding `noexcept` drops hash-code caching (`_Hashtable_traits<false, ...>`), and because `libstdc++` chains all buckets into a single singly-linked list, detecting bucket boundaries during 404 misses forces `_ZSt11_Hash_bytes` to **re-hash stored heap strings on every traversed node**—deterministically collapsing 404 miss throughput **`2.29×`** (`24.48 M/s` $\rightarrow$ `10.68 M/s`; isolated `s1_noexcept_demo`: `22.19` $\rightarrow$ `10.96 M/s`; `Google Benchmark`: `22.47` $\rightarrow$ `10.32 M/s`, consistently **`~22%` slower** than heap-allocating temporary `std::string`s at `13.14–13.88 M/s`!).

### 1C. Hasher Disclosure & Normalized Shared-Hasher Control (`challenge --mode=s1_shared_hash_control` & `Google Benchmark`)
Table 1B intentionally benchmarks each library with its **production default string hasher** (`std::hash`, `absl::Hash`, `folly::f14::DefaultHasher`, `boost::hash` + `is_avalanching`) so headline results reflect what teams ship out of the box. To isolate **hash-table layout + SIMD probing** from hash-function speed, we also normalized all four containers onto the exact same `std::hash<std::string_view>` (`TransparentStdStringHash`, MurmurHash2):

| Container (Identical `std::hash<std::string_view>`) | Zipfian `70/30` | `100%` 404 Negative Miss (`challenge` / `GBench`) | Layout + SIMD Speedup vs. `std` |
| :--- | :---: | :---: | :---: |
| **`std::unordered_map` (`std::hash`, cached)** | `18.94 M/s` (`52.8 ns`) | `25.02 M/s` (`40.0 ns`) / `22.47 M/s` | `1.00×` baseline |
| **`absl::flat_hash_map` (`std::hash`)** | **`30.32 M/s` (`33.0 ns`)** | `64.75 M/s` (`15.4 ns`) / `67.19 M/s` | **`1.60×` Zipf / `2.59×` 404** |
| **`folly::F14FastMap` (`std::hash`)** | `26.66 M/s` (`37.5 ns`) | `77.34 M/s` (`12.9 ns`) / `78.25 M/s` | **`1.41×` Zipf / `3.09×` 404** |
| **`boost::unordered_flat_map` (`std::hash`)** | `26.99 M/s` (`37.1 ns`) | **`79.08 M/s` (`12.6 ns`)** / **`83.27 M/s`** | **`1.43×` Zipf / `3.16×` 404** |

> **Key Takeaway:** Roughly a third to half of the headline `3.5×–4.8×` 404-miss lead over transparent `std::unordered_map` comes from faster default string hashing/mixing, while **`1.4×–1.6×` on Zipfian traffic and `2.6×–3.2×` on 404 misses** come purely from flat cache layout and SIMD tag probing.

---

## 2. Story 2: "The Best Hash Map Might Not Be a Hash Map" — Per-Request Small Maps (`N = 4..128` HTTP/2 Headers)

### Production Scenario
An HTTP/2 proxy builds a short-lived per-request header table (`N` zero-copy `std::string_view` header names $\rightarrow$ values), performs **12 mixed lookups** (`8` hits + `4` misses), and destroys the table at request end.

### 2A. Exact Heap Allocations Per Single 16-Header Request (`live_alloc_audit`)
- **`FlatInlinedSmallMap<16>` (`absl::InlinedVector<pair, 16>`):** **`0` heap allocations (`0 B`)** — 100% stack-resident!
- **`StdVectorSmallMap` (`std::vector` + `.Reserve(16)`):** `1` allocation (`512 B`).
- **`folly::F14FastMap` (sub-chunk sizing):** `1` allocation (`640 B`).
- **`boost::unordered_flat_map`:** `1` allocation (`992 B`).
- **`absl::flat_hash_map`:** `1` allocation (`1,048 B`).
- **`std::unordered_map`:** **`17` heap allocations (`904 B`)** — 1 bucket array + 16 separate node `malloc`/`free` pairs *per HTTP request*!

### 2B. Full Per-Request Lifecycle Crossover Sweep (`Google Benchmark` `ns/req` & `M req/s`)

| `N` Headers | `FlatInlinedSmallMap<16>` (Stack Scan) | `StdVectorSmallMap` (Zero Deps) | `std::unordered_map` | `absl::flat_hash_map` | `folly::F14FastMap` | `boost::unordered_flat_map` |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`N = 4`** | **`37.5 ns`** (**`26.6 M/s`**) | `50.0 ns` (`20.0 M/s`) | `106 ns` (`9.43 M/s`) | `100 ns` (`9.97 M/s`) | `95.0 ns` (`10.5 M/s`) | `82.0 ns` (`12.2 M/s`) |
| **`N = 8`** | **`54.4 ns`** (**`18.4 M/s`**) | `57.8 ns` (`17.3 M/s`) | `175 ns` (`5.70 M/s`) | `131 ns` (`7.63 M/s`) | `121 ns` (`8.24 M/s`) | `103 ns` (`9.67 M/s`) |
| **`N = 16`** | **`124 ns`** (**`8.03 M/s`**, **`0` allocs**) | `123 ns` (`8.16 M/s`) | `403 ns` (`2.48 M/s`, `17` allocs) | `176 ns` (`5.69 M/s`) | `144 ns` (`6.94 M/s`) | `174 ns` (`5.74 M/s`) |
| **`N = 32`** | `539 ns` (`1.85 M/s`) | `503 ns` (`1.99 M/s`) | `700 ns` (`1.43 M/s`) | `256 ns` (`3.90 M/s`) | **`231 ns`** (**`4.34 M/s`**) | `250 ns` (`3.99 M/s`) |
| **`N = 64`** | `3,302 ns` (`0.30 M/s`) | `3,218 ns` (`0.31 M/s`) | `1,185 ns` (`0.84 M/s`) | `433 ns` (`2.31 M/s`) | **`362 ns`** (**`2.76 M/s`**) | `412 ns` (`2.42 M/s`) |
| **`N = 128`** | `10,602 ns` (`0.09 M/s`) | `10,430 ns` (`0.10 M/s`) | `3,882 ns` (`0.26 M/s`) | `831 ns` (`1.20 M/s`) | **`673 ns`** (**`1.49 M/s`**) | `746 ns` (`1.34 M/s`) |

> **Clear Empirical Crossover:** For `N <= 16`, a stack-inlined flat scan beats every hash map (`2.2×–3.2×` faster than `std::unordered_map`, zero heap allocations). Above `N ≈ 20–24`, $O(N^2)$ duplicate-checking during build makes SIMD flat hash maps (`F14FastMap`, `absl::flat_hash_map`, `boost::unordered_flat_map`) dominate by `2.3×–16×`.

---

## 3. Story 3: Paying Off Episode 15's Cliffhanger — The Large-Value RAM Cliff & Dense Iteration

### Production Scenario
Tracking **`1,000,000` active entries** (just past $87.5\% \times 2^{20} = 917,504$, forcing power-of-two flat tables to double to `2,097,152` slots at ~48% fill) AND **`800,000` entries** (pre-doubling ~76% fill best case for flat maps) across three realistic entry sizes: `32B` `CompactFlowStats`, `128B` `ConnectionMetadata`, and `256B` `UpstreamEndpointProfile`.

### 3A. Isolated Fresh-Process `VmRSS` Footprint (`challenge --mode=rss_*`)

| Container | `1M × 32B` (`30.5 MiB` raw) | `1M × 128B` (`122.1 MiB` raw) | `1M × 256B` (`244.1 MiB` raw) | `800K × 256B` (Pre-Double Best Case) |
| :--- | :---: | :---: | :---: | :---: |
| **`folly::F14FastMap` (`F14VectorMap`)** | **`38.6 MiB`** (`40.5 B/row`) | **`130.2 MiB`** (`136.5 B/row`) | **`252.3 MiB`** (`264.5 B/row`) | **`203.4 MiB`** (`266.7 B/row`) |
| **`std::unordered_map`** | `53.9 MiB` (`56.5 B`) | `145.5 MiB` (`152.5 B`) | `267.5 MiB` (`280.5 B`) | `214.0 MiB` (`280.5 B`) |
| **`folly::F14NodeMap` (Stable)** | — | `153.4 MiB` (`160.9 B`) | `275.5 MiB` (`288.9 B`) | — |
| **`boost::unordered_node_map` (Stable)** | — | `154.4 MiB` (`161.9 B`) | `276.5 MiB` (`289.9 B`) | — |
| **`absl::node_hash_map` (Stable)** | — | `155.4 MiB` (`163.0 B`) | `277.5 MiB` (`291.0 B`) | — |
| **`absl::flat_hash_map<K, unique_ptr<V>>`** | — | `156.2 MiB` (`163.7 B`) | `278.2 MiB` (`291.7 B`) | `212.4 MiB` (`278.4 B`) |
| **`folly::F14ValueMap` (Inline)** | `58.1 MiB` (`61.0 B`) | `226.1 MiB` (`237.1 B`) | `450.1 MiB` (`472.0 B`) | `450.1 MiB` (`590.0 B` — doubled at 786K!) |
| **`boost::unordered_flat_map` (Inline)** | `62.1 MiB` (`65.1 B`) | `242.1 MiB` (`253.9 B`) | `482.0 MiB` (`505.4 B`) | `241.1 MiB` (`316.0 B`) |
| **`absl::flat_hash_map` (Inline)** | `66.1 MiB` (`69.3 B`) | **`258.1 MiB`** (`270.7 B`) | **`514.0 MiB`** (**`538.9 B`** = `2.04×` F14Fast!) | `257.1 MiB` (`337.0 B`) |

### 3B. Honest Trade-Off on `1M × 256B` `UpstreamEndpointProfile`: Dense Snapshot Flush vs. Random Hits

| Container | Full-Table 1M Flush (`challenge` / `GBench`) | Random `80/20` Lookup (`256B`) | Memory (`1M × 256B`) | Why This Happens in Silicon |
| :--- | :---: | :---: | :---: | :--- |
| **`folly::F14FastMap` (`F14VectorMap`)** | **`5.13 ms` (`195.0 M/s`)** / **`4.92 ms`** | `35.02 M/s` (`28.6 ns`) | **`252.3 MiB`** (`-51%`) | Streams dense packed `244 MiB` vector (`4.81M` LLC misses); hits pay 1 extra 32-bit index hop |
| **`boost::unordered_flat_map`** | `17.68 ms` (`56.6 M/s`) / `17.6 ms` | **`43.36 M/s`** (`23.1 ns`) | `482.0 MiB` | Inline `256B` values eliminate hop on hits, but flush must skip `~1M` empty `256B` holes (`53.2M` LLC misses) |
| **`absl::flat_hash_map`** | `19.52 ms` (`51.2 M/s`) / `19.2 ms` | **`46.94 M/s`** (`21.3 ns`) | **`514.0 MiB`** | Fastest random hits (`1.34×` over F14Vector), at the cost of **`2.04×` RAM** and `3.8×` slower sweeps (`63.4M` LLC misses) |
| **`folly::F14ValueMap` (Inline)** | `15.82 ms` (`63.2 M/s`) | `31.97 M/s` (`31.3 ns`) | `450.1 MiB` | Shows why Meta auto-switches `F14FastMap` to `F14VectorMap` at `sizeof(value_type) >= 24` |
| **`absl::flat<K, unique_ptr<V>>`** | `21.28 ms` (`47.0 M/s`) | `30.91 M/s` (`32.4 ns`) | `278.2 MiB` | Saves `236 MiB` vs inline `absl::flat_hash_map` + preserves value pointers, with heap pointer chase |
| **`std::unordered_map`** | **`67.31 ms` (`14.9 M/s`)** / `64.8 ms` | `17.14 M/s` (`58.3 ns`) | `267.5 MiB` | **`13.1×` slower flush** than `F14FastMap` (`70.6M` LLC misses, `11.9M` dTLB misses) |

---

## 4. Story 4: Long-Running Steady-State Eviction Churn (Tombstones vs. Ref-Counted Overflow vs. Anti-Drift)

### Production Scenario
A long-lived sliding-window flow/connection tracker maintains a constant live working set (`32B` `CompactFlowStats`) while executing **`4,000,000` continuous `erase(oldest) + emplace(newest)` cycles**. Because `F14VectorMap` uses 12-slot 64-byte cache-line chunks (`kDesiredCapacity = 10`, verified in `F14Table.h:611-613`) whose doubling boundaries differ from `absl` (`2^20`), `boost` (`15 × 2^16`), and `F14ValueMap` (`14 × 2^16`), we honestly measure **both** high-occupancy bands so every engine is tested right near its own `~75%–85%` load ceiling:

### 4A. Band A (`780,000` Live Entries: `absl 74.4%`, `boost 79.3%`, `F14Value 85.0%`, `F14Vector 49.6%`)

| Container & Deletion Mechanism | Churn Rate | `p50` | `p99` | `p99.9` | `Max` Stall | `80/20` Before $\rightarrow$ After | `100%` Miss Before $\rightarrow$ After |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`std::unordered_map` (Heap Nodes)** | `3.91 M/s` | `221 ns` | `641 ns` | `911 ns` | `31.6 µs` | `31.7 -> 29.7 M/s` (`-6%`) | `38.9 -> 35.7 M/s` |
| **`absl::flat_hash_map` (`kDeleted` Tombstones)** | `9.41 M/s` | `60 ns` | `330 ns` | `410 ns` | **`5.77 ms`** | `89.2 -> 70.3 M/s` | **`171.9 -> 68.2 M/s`** (**`-60%` / `2.52×` slower**) |
| **`boost::unordered_flat_map` (`ofw` + Anti-Drift)** | `9.21 M/s` | `41 ns` | `331 ns` | `411 ns` | **`15.67 ms`** | **`85.0 -> 86.2 M/s`** (**`0%` drift!**) | **`268.7 -> 272.0 M/s`** (**`0%` — stays fresh!**) |
| **`folly::F14ValueMap` (RefCnt `ofw`, `85%` fill)** | `9.31 M/s` | `70 ns` | `340 ns` | `450 ns` | **`23.9 µs`** | `74.0 -> 47.1 M/s` | **`113.7 -> 31.9 M/s`** (**`-72%` / `3.57×` slower**) |
| **`folly::F14FastMap` (`F14Vector`, `50%` fill)** | **`18.36 M/s`** | **`30 ns`** | **`140 ns`** | **`301 ns`** | **`20.9 µs`** | `59.3 -> 60.1 M/s` | **`391.6 -> 337.1 M/s`** |

### 4B. Band B (`640,000` Live Entries: `F14VectorMap` Packed Tight at `81.4%` — `9.8/12` Slots/Chunk!)

| Container & Deletion Mechanism | Churn Rate | `p50` | `p99` | `p99.9` | `Max` Stall | `80/20` Before $\rightarrow$ After | `100%` Miss Before $\rightarrow$ After |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`std::unordered_map`** | `4.44 M/s` | `180 ns` | `601 ns` | `911 ns` | `44.4 µs` | `36.0 -> 33.8 M/s` | `44.8 -> 40.9 M/s` |
| **`absl::flat_hash_map` (`61.0%` fill)** | `10.40 M/s` | `50 ns` | `321 ns` | `401 ns` | **`5.05 ms`** | `98.6 -> 86.1 M/s` | **`247.3 -> 115.6 M/s`** (**`-53%`**) |
| **`folly::F14ValueMap` (`81.4%` of max)** | `11.13 M/s` | `50 ns` | `300 ns` | `421 ns` | **`26.7 µs`** | `90.9 -> 78.8 M/s` | `260.0 -> 117.6 M/s` (`-55%`) |
| **`boost::unordered_flat_map` (`65.1%` fill)** | `11.34 M/s` | **`40 ns`** | `301 ns` | `401 ns` | **`15.28 ms`** | `106.0 -> 92.6 M/s` | **`396.9 -> 201.9 M/s`** |
| **`folly::F14FastMap` (`F14Vector`, `81.4%` fill!)** | **`13.12 M/s`** | `50 ns` | **`171 ns`** | `401 ns` | **`27.8 µs`** | `77.7 -> 54.3 M/s` | **`143.2 -> 43.3 M/s`** (**`-70%` / `3.31×` drift!**) |

> **The Honest Algorithmic Trade-Off Exposed by Bare Metal:**
> 1. **Folly F14 (Reference-Counted Overflow):** Delivers the fastest churn (`13.1–18.4 M cyc/s`) and **zero rehash tail spikes (`21–28 µs` max — `180×–560×` lower than Abseil/Boost!)**, *but* because it never triggers an automatic cleanup rehash under constant size, packing F14 near its `81–85%` ceiling for millions of evictions allows probe-chain drift (`3.3×–3.6×` slower negative misses—solved by reserving ~20% headroom or periodic `rehash`).
> 2. **Boost.Unordered (`group15` + Anti-Drift):** Deliberately trades periodic **`15.3–15.7 ms` anti-drift rehash pauses** to completely eliminate long-term lookup drift (`268.7 -> 272.0 M/s` at 79.3% load!).
> 3. **Abseil SwissTable (`kDeleted` Tombstones):** Sits in between—periodic **`5.1–5.8 ms` `drop_deletes_without_resize()` pauses** while tombstones between cleanups cut negative-miss speed roughly in half (`-53%` to `-60%`).

---

## 5. Story 5: The Migration Landmine — Pointer Stability, ASan UAF & Industrial SIMD Node Maps

### 5A. Live Migration Bug Reproduction (`live_uaf_migration_repro`)
A realistic parent-tenant inheritance loop (`policies[child_id] = policies[1ULL];` — **Abseil Tip of the Week #136**, [`live_uaf_migration_repro.cc:122`](./live_uaf_migration_repro.cc)) compiles with **zero warnings under `-Wall -Wextra`** and ran safely for years on `std::unordered_map`. Under `absl::flat_hash_map`, `folly::F14FastMap`, and `boost::unordered_flat_map`, rehash relocates the backing array (`0x61608a1655a8 -> 0x7908808a59b0`)—triggering an immediate **`AddressSanitizer: heap-use-after-free` (`READ of size 24` inside freed `DeallocateBackingArray` at `live_uaf_migration_repro.cc:122`)** when tested under `-fsanitize=address` with `--trigger_uaf`.

### 5B. The Real Performance & RAM Cost of Keeping Pointer Stability (`1M × 128B ConnectionMetadata`)

| Container | Address Stability | `1M × 128B` RSS | `80/20` Lookups (`challenge` / `GBench`) | `100%` Negative Misses |
| :--- | :---: | :---: | :---: | :---: |
| **`std::unordered_map`** | Full (Key + Val) | `145.5 MiB` | `16.56 M/s` / `17.27 M/s` | `30.62 M/s` (`32.7 ns`) |
| **`absl::node_hash_map`** | **Full (Key + Val)** | `155.4 MiB` | **`33.25 M/s` / `42.25 M/s`** (`2.0×–2.4×`) | **`181.02 M/s`** (**`5.9×`**) |
| **`folly::F14NodeMap`** | **Full (Key + Val)** | **`153.4 MiB`** | **`37.06 M/s` / `42.35 M/s`** (`2.2×–2.5×`) | **`257.25 M/s`** (**`8.4×`**) |
| **`boost::unordered_node_map`** | **Full (Key + Val)** | `154.4 MiB` | **`39.37 M/s` / `39.24 M/s`** (`2.3×–2.4×`) | **`322.96 M/s`** (**`10.5×`**) |
| **`absl::flat<K, unique_ptr<V>>`** | Value Only | `156.2 MiB` | `34.88 M/s` / `37.76 M/s` (`2.1×–2.2×`) | **`271.48 M/s`** (**`8.9×`**) |
| *Reference: `folly::F14FastMap` (Flat/Vec)* | *None (Moves)* | *`130.2 MiB`* | *`41.77 M/s` / `47.51 M/s`* | *`241.48 M/s`* |
| *Reference: `boost::unordered_flat_map`* | *None (Moves)* | *`242.1 MiB`* | *`52.37 M/s` / `58.02 M/s`* | *`333.91 M/s`* |
| *Reference: `absl::flat_hash_map`* | *None (Moves)* | *`258.1 MiB`* | *`53.58 M/s` / `57.22 M/s`* | *`165.79 M/s`* |

> **Major Practical Takeaway:** Even when a service genuinely needs 100% pointer stability (or during staged migrations off `std::unordered_map`), **`folly::F14NodeMap`, `boost::unordered_node_map`, and `absl::node_hash_map` deliver `2.0×–2.5×` faster `80/20` lookups and `5.9×–10.5×` faster negative misses** for only `~8–10 MiB` extra metadata—because their probing layer is still SIMD open addressing!

---

## 6. Strictly Unmultiplexed Linux `perf stat` Hardware PMU Telemetry (Zen 4, 100% Uptime)

Three independent passes (`<= 4` hardware counters per pass, `--quiet` eliminating log I/O noise):

### 6A. Story 1 — Zipfian `70/30` & `100%` Miss Route Lookups (`20,000,000` Lookups)

| Workload & Container | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Zipf: `std::unordered_map` (Default Alloc)** | `11.38B` | `11.01B` | `0.97` | `49.22M` | `228.89M` | `30.30M` | `116.39M` |
| **Zipf: `std::unordered_map` (C++20 Cached)** | `7.79B` | **`6.53B`** (`-41%`) | `0.84` | `48.90M` | `226.03M` | `30.00M` | `118.91M` |
| **Zipf: `std::pmr::unordered_map` (Arena)** | `5.83B` | `6.34B` | `1.09` | `48.27M` | `233.64M` | **`10.09M`** (**`-66%`**) | `116.31M` |
| **Zipf: `absl::flat_hash_map`** | **`4.49B`** | **`4.42B`** | `0.98` | **`29.15M`** | `187.54M` | `12.56M` | `76.65M` |
| **Zipf: `folly::F14FastMap`** | `4.77B` | `5.49B` | `1.15` | `33.69M` | **`161.56M`** | `10.78M` | **`65.95M`** (`-43%`) |
| **Zipf: `boost::unordered_flat_map`** | `5.11B` | `6.43B` | **`1.26`** | `35.92M` | `181.61M` | `11.96M` | `70.32M` |
| **Miss: `std::unordered_map` (C++20 Cached)** | `6.17B` | `5.47B` | `0.89` | `42.30M` | `181.28M` | `29.92M` | `105.11M` |
| **Miss: `absl::flat_hash_map`** | `2.52B` | **`3.46B`** | `1.37` | `28.15M` | `146.12M` | `3.55M` | `47.57M` |
| **Miss: `folly::F14FastMap`** | **`2.44B`** | `4.28B` | `1.76` | **`26.04M`** | **`108.06M`** | **`2.40M`** | `30.33M` |
| **Miss: `boost::unordered_flat_map`** | `2.75B` | `5.52B` | **`2.01`** | `29.05M` | `112.27M` | `3.02M` | **`21.96M`** (`-79%`) |

### 6B. Story 3 (`20` Full Sweeps × `1M × 256B`) & Story 4 (`4M` Steady-State Churn Cycles)

| Workload & Container | Cycles | Instructions | IPC | Branch Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **Iter 256B: `std::unordered_map`** | `9.69B` | `1.74B` | **`0.18`** | `14.14M` | `11.87M` | `70.59M` |
| **Iter 256B: `absl::flat_hash_map`** | `3.93B` | `1.98B` | `0.50` | `39.85M` | `4.30M` | `63.39M` |
| **Iter 256B: `boost::unordered_flat_map`** | `3.77B` | `2.03B` | `0.54` | `22.53M` | `4.04M` | `53.16M` |
| **Iter 256B: `folly::F14FastMap` (Dense Vector)** | **`1.69B`** | **`1.22B`** | **`0.72`** | **`11.43M`** | **`1.68M`** | **`4.81M`** (**`-92.4%`** vs absl!) |
| **Churn 4M (`780K`): `std::unordered_map`** | `7.28B` | `2.17B` | `0.30` | `10.80M` | `17.68M` | `37.86M` |
| **Churn 4M (`780K`): `absl::flat_hash_map`** | `1.88B` | `1.84B` | `0.98` | `15.62M` | `7.84M` | `37.89M` |
| **Churn 4M (`780K`): `boost::unordered_flat_map`** | `1.58B` | `1.26B` | `0.80` | `9.55M` | `6.44M` | `24.00M` |
| **Churn 4M (`780K`): `folly::F14ValueMap` (`85%`)** | `2.02B` | `1.35B` | `0.67` | `12.07M` | `49.51M` | `102.27M` |
| **Churn 4M (`780K`): `folly::F14FastMap` (`50%`)** | **`0.94B`** | `1.30B` | **`1.39`** | **`3.51M`** | **`0.73M`** | **`11.84M`** |
| **Churn 4M (`640K`): `folly::F14FastMap` (`81%`)** | **`1.32B`** | `1.54B` | `1.17` | `11.40M` | **`0.69M`** | `37.19M` |

---

## 7. Verified Zen 4 Silicon Anatomy (`assembly/godbolt_comparison.s`)

All compiled production kernels are archived in [`assembly/godbolt_comparison.s`](./assembly/godbolt_comparison.s) and live on Compiler Explorer ([`https://godbolt.org/z/babYarMhf`](https://godbolt.org/z/babYarMhf)):
1. **`std::unordered_map` (`Ep16DisasmStdFindU64` / `LookupRoute_DefaultAlloc` / `LookupRoute_GccNoexceptTrap`):** `div r8` hardware division by prime `bucket_count` $\rightarrow$ linked-list pointer chase (`mov rdi, [rax]`); for default non-transparent `const char*` lookup, emits `call _Znwm` (`operator new`), `memcpy`, and `call _ZdlPv` (`operator delete`) with zero explicit `std::string` cast; adding `noexcept` to the custom transparent hasher emits three `_ZSt11_Hash_bytes` calls inside the bucket walk!
2. **`absl::flat_hash_map` (`Ep16DisasmAbslFlatFindU64`):** Salted `mulx` mix $\rightarrow$ `vpbroadcastb xmm0` + `vmovdqu xmm1, [r10+rdi]` + `vpcmpeqb` + `vpmovmskb` (16 slots) + `tzcnt`/`blsr`, and `vpsignb` + `vpmovmskb` testing for `kEmpty` (`0x80`).
3. **`folly::F14ValueMap` vs. `folly::F14FastMap` (`F14VectorMap`):**
   - `F14ValueMap` (`Ep16DisasmFollyF14ValueFindU64`): Hardware `crc32` mix $\rightarrow$ `imul rax, 0x1d0` (`464B` chunk) $\rightarrow$ `vpcmpeqb` + `vpmovmskb` masked with `0x3fff` (**14 slots**) $\rightarrow$ `cmp byte ptr [rdi+0xf], 0` (ref-counted overflow byte!).
   - `F14FastMap` (`Ep16DisasmFollyF14FastFindU64`): `shl rcx, 0x6` (**exact 64-byte cache-line chunk!**) $\rightarrow$ `vpcmpeqb` + `vpmovmskb` masked with `0xfff` (**12 4-byte indices** inside 1 cache line!) $\rightarrow$ `mov eax, [rcx+4*rax]` $\rightarrow$ dense vector dereference!
4. **`boost::unordered_flat_map` (`Ep16DisasmBoostFlatFindU64`):** Fibonacci `mulx` mix $\rightarrow$ `vpbroadcastd` + `vpcmpeqb` + `vpmovmskb` masked with `0x7fff` (**15 slots**) $\rightarrow$ `and cl, byte ptr [rsi+0xf]` testing the `(hash & 7)`-th bit of the overflow byte (`ofw`), zero tombstones!

---

## 8. Verified Primary Sources (Slide Citation Bars)

- `Matt Kulukundis (CppCon 2017) — Google Fleet Telemetry (~1% CPU, >4% RAM in C++ Hash Tables)`
- `WG21 P0919R3 & P1690R1 (C++20) — Heterogeneous Lookup for Unordered Containers`
- `GCC 13 libstdc++ — bits/hashtable.h (__cache_default & __is_fast_hash)`
- `Nathan Bronson & Xiao Shi (Meta Engineering, Apr 2019) — folly/container/F14Map.h & F14Table.h`
- `Joaquín M. López Muñoz (Boost 1.81–1.90) — boost/unordered/detail/foa/core.hpp (group15 & Anti-Drift)`
- `Abseil Tip of the Week #136 & Containers Guide — Pointer Stability, node_hash_map & unique_ptr<V>`
- `WG21 P1863R1 (Titus Winters, 2020) — "ABI — Now or Never"`
