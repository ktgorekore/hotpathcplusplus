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
> **Toolchains & BCR Dependencies:** `GCC 13.3.0` & `Clang 18.1.3` (`-std=c++20 -O3 -march=native`), `Bazel 9.3.2` (Bzlmod), `Abseil 20260526.0`, `Meta Folly 2026.08.31.00`, `Boost.Unordered 1.90.0.bcr.1`, `Google Benchmark 1.9.5`, `GoogleTest 1.18.0`  
> **Parity Verification:** `//episodes/16_cpp_hash_maps_deep_dive:cpp_hash_maps_test` passes bit-for-bit identical checksums across all 12 container variants under both `GCC 13.3.0` (`0.5s`) and `Clang 18.1.3` (`0.7s`).  
> **Live Compiler Explorer (Godbolt):** [`https://godbolt.org/z/9zj7daGTq`](https://godbolt.org/z/9zj7daGTq)  
> **Intel-Syntax Assembly Archive:** [`assembly/godbolt_comparison.s`](./assembly/godbolt_comparison.s) & [`assembly/godbolt_source.cc`](./assembly/godbolt_source.cc)

---

## 1. Story 1: "Before You Add a Single Dependency" — Hidden Lookup Allocations & GCC's `noexcept` Hash Trap

### Production Scenario
An Envoy-style HTTP/RPC gateway holds **`100,000` active routes** keyed by realistic **36–42 byte non-SSO strings** (e.g., `/api/v2/payments/tenant-0004129/authorize`, strictly exceeding GCC `libstdc++`'s 15-byte SSO buffer) mapped to a `24B` `RoutePolicy`, built on a fragmented aged heap (`250,000` mixed `32B–256B` churn chunks + interleaved request allocations). Incoming requests slice zero-copy `std::string_view` paths directly out of the socket receive buffer (`1,000,000` queries/pass across Zipfian $s=0.99$ `70/30` hot skew, Uniform aged `70/30`, and `100%` 404 bot-scan misses).

### 1A. Exact Global `operator new` Allocation Audit (`1,000,000` Route Lookups — `live_alloc_audit`)

| Container Configuration | `operator new` Calls / 1M Finds | Heap Bytes Allocated | Parity Checksum |
| :--- | ---: | ---: | :---: |
| **`std::unordered_map<std::string, RoutePolicy>` (Default)** | **`1,000,000`** | **`42.43 MB`** | `0xec40b6c0b41bc2ec` |
| **`boost::unordered_flat_map<std::string, RoutePolicy>` (Default!)** | **`1,000,000`** | **`42.43 MB`** | `0xec40b6c0b41bc2ec` |
| **`std::unordered_map` + C++20 `is_transparent` (`P0919R3`/`P1690R1`)** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |
| **`boost::unordered_flat_map` + `TransparentBoostStringHash`** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |
| **`absl::flat_hash_map<std::string, RoutePolicy>` (Default)** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |
| **`folly::F14FastMap<std::string, RoutePolicy>` (Default)** | **`0`** | **`0 B`** | `0xec40b6c0b41bc2ec` |

> **Practical Boost Gotcha:** Unlike `absl::flat_hash_map` and `folly::F14FastMap` (which enable transparent `std::string_view` lookup by default), `boost::unordered_flat_map<std::string, V>` defaults to `std::equal_to<Key>`—requiring an explicit transparent hasher (`using is_transparent = void; using is_avalanching = void;`) + `std::equal_to<>` to eliminate temporary `std::string` allocations.

### 1B. Throughput Across Zipfian Skew, Uniform Aged Heap & 100% Negative Misses (`challenge` & `Google Benchmark`)

| Container & Configuration | Zipfian `70/30` (`challenge` / `GBench`) | Uniform Aged `70/30` | `100%` 404 Negative Miss (`challenge` / `GBench`) |
| :--- | :---: | :---: | :---: |
| **`std::unordered_map` (Default, tmp `std::string`)** | `11.74 M/s` (`85.2 ns`) / `12.09 M/s` | `7.02 M/s` (`142.5 ns`) | `13.02 M/s` (`76.8 ns`) / `13.15 M/s` |
| **`std::unordered_map` (C++20 `is_transparent` — cached hash)** | **`19.50 M/s` (`51.3 ns`)** / **`21.47 M/s`** (`+66%–78%`) | **`12.69 M/s` (`78.8 ns`)** (`+81%`) | **`28.54 M/s` (`35.0 ns`)** / **`26.79 M/s`** (**`2.19×`**) |
| **`std::unordered_map` (`noexcept` hash trap!)** | `15.18 M/s` (`65.9 ns`) | `10.23 M/s` (`97.7 ns`) | **`11.82 M/s` (`84.6 ns`)** (**`2.41×` slower** than cached!) |
| **`std::pmr::unordered_map` (Monotonic Arena, zero deps)** | **`25.88 M/s` (`38.6 ns`)** / `26.99 M/s` (`2.2×`) | **`24.44 M/s` (`40.9 ns`)** (`3.5×`) | `27.10 M/s` (`36.9 ns`) |
| **`absl::flat_hash_map` (16-way SwissTable SIMD)** | **`44.93 M/s` (`22.3 ns`)** / **`49.18 M/s`** (**`3.8×–4.1×`**) | **`37.74 M/s` (`26.5 ns`)** (**`5.4×`**) | **`119.99 M/s` (`8.3 ns`)** / `121.02 M/s` (**`9.2×`**) |
| **`folly::F14FastMap` (14-way `F14VectorMap`, `12` slots/line)** | `33.06–47.99 M/s` / `46.49 M/s` | `26.47–45.12 M/s` | **`180.51 M/s` (`5.5 ns`)** / **`185.81 M/s`** (**`13.9×–14.1×`**) |
| **`folly::F14ValueMap` (14-way inline)** | `34.59 M/s` / **`50.42 M/s`** | `24.69 M/s` | **`158.73 M/s` (`6.3 ns`)** |
| **`boost::unordered_flat_map` (15-way `group15` + `ofw`)** | `38.27 M/s` (`26.1 ns`) / `39.60 M/s` | `19.94–40.74 M/s` | **`116.63 M/s` (`8.6 ns`)** / **`125.54 M/s`** (**`9.0×–9.5×`**) |

> **Real-World GCC `libstdc++` Discovery (Verified Directly in `/usr/include/c++/13/bits/hashtable.h:47-52` & Disassembly):**
> GCC decides whether to cache the 64-bit hash inside each `std::unordered_map` node using `__cache_default = !(__is_fast_hash<_Hash> && __is_nothrow_invocable<...>)`. Because `std::__is_fast_hash<UserHash>` already defaults to `true` for all user-defined hashers (only `std::hash<std::string>` is explicitly specialized to `false`), omitting `noexcept` forces GCC to cache the 8-byte hash only because the C++ Standard forbids `erase(const_iterator)` from throwing. Adding `noexcept` drops hash-code caching (`_Hashtable_traits<false, ...>`), and because `libstdc++` chains all buckets into a single singly-linked list, detecting bucket boundaries during 404 misses forces `_ZSt11_Hash_bytes` to **re-hash stored heap strings on every traversed node**—collapsing 404 miss throughput **`2.41×`** (`28.54 M/s` $\rightarrow$ `11.82 M/s`, even slower than allocating temporary `std::string`s at `13.02 M/s`!).

### 1C. Hasher Disclosure & Normalized Shared-Hasher Control (`challenge --mode=s1_shared_hash_control`)
Table 1B intentionally benchmarks each library with its **production default string hasher** (`std::hash`, `absl::Hash`, `folly::f14::DefaultHasher`, `boost::hash` + `is_avalanching`) so headline results reflect what teams ship out of the box. To isolate **hash-table layout + SIMD probing** from hash-function speed, we also normalized all four containers onto the exact same `std::hash<std::string_view>` (`TransparentStdStringHash`, MurmurHash2):

| Container (Identical `std::hash<std::string_view>`) | Zipfian `70/30` | `100%` 404 Negative Miss | Layout + SIMD Speedup vs. `std` |
| :--- | :---: | :---: | :---: |
| **`std::unordered_map` (`std::hash`, cached)** | `18.37 M/s` (`54.4 ns`) | `29.28 M/s` (`34.2 ns`) | `1.00×` baseline |
| **`absl::flat_hash_map` (`std::hash`)** | **`35.86 M/s` (`27.9 ns`)** | `75.99 M/s` (`13.2 ns`) | **`1.95×` Zipf / `2.60×` 404** |
| **`boost::unordered_flat_map` (`std::hash`)** | `35.33 M/s` (`28.3 ns`) | `86.92 M/s` (`11.5 ns`) | **`1.92×` Zipf / `2.97×` 404** |
| **`folly::F14FastMap` (`std::hash`)** | `29.59 M/s` (`33.8 ns`) | **`92.76 M/s` (`10.8 ns`)** | **`1.61×` Zipf / `3.17×` 404** |

> **Key Takeaway:** Roughly half of the headline `4×–6×` 404-miss lead over transparent `std::unordered_map` comes from faster default string hashing/mixing, while **`1.6×–2.0×` on Zipfian traffic and `2.6×–3.2×` on 404 misses** come purely from flat cache layout and SIMD tag probing.

---

## 2. Story 2: "The Best Hash Map Might Not Be a Hash Map" — Per-Request Small Maps (`N = 4..128` HTTP/2 Headers)

### Production Scenario
An HTTP/2 proxy builds a short-lived per-request header table (`N` zero-copy `std::string_view` header names $\rightarrow$ values), performs **12 mixed lookups** (`8` hits + `4` misses), and destroys the table at request end.

### 2A. Exact Heap Allocations Per Single 16-Header Request (`live_alloc_audit`)
- **`FlatInlinedSmallMap<16>` (`absl::InlinedVector<pair, 16>`):** **`0` heap allocations (`0 B`)** — 100% stack-resident!
- **`StdVectorSmallMap` (`std::vector` + `.reserve(16)`):** `1` allocation (`512 B`).
- **`folly::F14FastMap` (sub-chunk sizing):** `1` allocation (`640 B`).
- **`boost::unordered_flat_map`:** `1` allocation (`992 B`).
- **`absl::flat_hash_map`:** `1` allocation (`1,048 B`).
- **`std::unordered_map`:** **`17` heap allocations (`904 B`)** — 1 bucket array + 16 separate node `malloc`/`free` pairs *per HTTP request*!

### 2B. Full Per-Request Lifecycle Crossover Sweep (`Google Benchmark` / `challenge` `ns/req` & `M req/s`)

| `N` Headers | `FlatInlinedSmallMap<16>` (Stack Scan) | `StdVectorSmallMap` (Zero Deps) | `std::unordered_map` | `absl::flat_hash_map` | `folly::F14FastMap` | `boost::unordered_flat_map` |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`N = 4`** | **`36.9 ns`** (**`27.1 M/s`**) | `44.8 ns` (`22.3 M/s`) | `104 ns` (`9.67 M/s`) | `102 ns` (`9.85 M/s`) | `96.4 ns` (`10.4 M/s`) | `83.2 ns` (`12.0 M/s`) |
| **`N = 8`** | **`52.5 ns`** (**`19.0 M/s`**) | `64.7 ns` (`15.5 M/s`) | `175 ns` (`5.73 M/s`) | `135 ns` (`7.40 M/s`) | `123 ns` (`8.11 M/s`) | `105 ns` (`9.54 M/s`) |
| **`N = 16`** | **`118 ns`** (**`8.49 M/s`**, **`0` allocs**) | `147 ns` (`6.82 M/s`) | `391 ns` (`2.56 M/s`, `17` allocs) | `183 ns` (`5.45 M/s`) | `144 ns` (`6.95 M/s`) | `177 ns` (`5.66 M/s`) |
| **`N = 32`** | `534 ns` (`1.87 M/s`) | `579 ns` (`1.73 M/s`) | `698 ns` (`1.43 M/s`) | `258 ns` (`3.89 M/s`) | **`233 ns`** (**`4.30 M/s`**) | `295 ns` (`3.39 M/s`) |
| **`N = 64`** | `3,262 ns` (`0.31 M/s`) | `3,512 ns` (`0.28 M/s`) | `1,231 ns` (`0.81 M/s`) | `479 ns` (`2.09 M/s`) | **`368 ns`** (**`2.72 M/s`**) | `414 ns` (`2.42 M/s`) |
| **`N = 128`** | `10,807 ns` (`0.09 M/s`) | `11,782 ns` (`0.08 M/s`) | `3,844 ns` (`0.26 M/s`) | `806 ns` (`1.24 M/s`) | **`690 ns`** (**`1.45 M/s`**) | `778 ns` (`1.29 M/s`) |

> **Clear Empirical Crossover:** For `N <= 16`, a stack-inlined flat scan beats every hash map (`2.2×–3.3×` faster than `std::unordered_map`, zero heap allocations). Above `N ≈ 20–24`, $O(N^2)$ duplicate-checking during build makes SIMD flat hash maps (`F14FastMap`, `absl::flat_hash_map`, `boost::unordered_flat_map`) dominate by `2.3×–15×`.

---

## 3. Story 3: Paying Off Episode 15's Cliffhanger — The Large-Value RAM Cliff & Dense Iteration

### Production Scenario
Tracking **`1,000,000` active entries** (just past $87.5\% \times 2^{20} = 917,504$, forcing power-of-two flat tables to double to `2,097,152` slots at ~48% fill) AND **`800,000` entries** (pre-doubling ~76% fill best case for flat maps) across three realistic entry sizes: `32B` `CompactFlowStats`, `128B` `ConnectionMetadata`, and `256B` `UpstreamEndpointProfile`.

### 3A. Isolated Fresh-Process `VmRSS` Footprint (`challenge --mode=rss_*`)

| Container | `1M × 32B` (`30.5 MiB` raw) | `1M × 128B` (`122.1 MiB` raw) | `1M × 256B` (`244.1 MiB` raw) | `800K × 256B` (Pre-Double Best Case) |
| :--- | :---: | :---: | :---: | :---: |
| **`folly::F14FastMap` (`F14VectorMap`)** | **`38.6 MiB`** (`40.5 B/row`) | **`130.1 MiB`** (`136.5 B/row`) | **`252.2 MiB`** (`264.5 B/row`) | **`203.4 MiB`** (`266.6 B/row`) |
| **`std::unordered_map`** | `53.9 MiB` (`56.5 B`) | `145.5 MiB` (`152.5 B`) | `267.5 MiB` (`280.5 B`) | `213.9 MiB` (`280.4 B`) |
| **`folly::F14NodeMap` (Stable)** | — | `153.4 MiB` (`160.8 B`) | `275.5 MiB` (`288.8 B`) | — |
| **`boost::unordered_node_map` (Stable)** | — | `154.4 MiB` (`161.9 B`) | `276.5 MiB` (`289.9 B`) | — |
| **`absl::node_hash_map` (Stable)** | — | `155.4 MiB` (`162.9 B`) | `277.5 MiB` (`290.9 B`) | — |
| **`absl::flat_hash_map<K, unique_ptr<V>>`** | — | `156.1 MiB` (`163.7 B`) | `278.2 MiB` (`291.7 B`) | `212.4 MiB` (`278.4 B`) |
| **`folly::F14ValueMap` (Inline)** | `58.1 MiB` (`60.9 B`) | `226.1 MiB` (`237.1 B`) | `450.1 MiB` (`471.9 B`) | `450.1 MiB` (`589.9 B` — doubled at 786K!) |
| **`boost::unordered_flat_map` (Inline)** | `62.1 MiB` (`65.1 B`) | `242.1 MiB` (`253.8 B`) | `481.9 MiB` (`505.4 B`) | `241.1 MiB` (`316.0 B`) |
| **`absl::flat_hash_map` (Inline)** | `66.1 MiB` (`69.3 B`) | **`258.1 MiB`** (`270.6 B`) | **`513.9 MiB`** (**`538.9 B`** = `2.04×` F14Fast!) | `257.1 MiB` (`336.9 B`) |

### 3B. Honest Trade-Off on `1M × 256B` `UpstreamEndpointProfile`: Dense Snapshot Flush vs. Random Hits

| Container | Full-Table 1M Flush (`challenge` / `GBench`) | Random `80/20` Lookup (`256B`) | Memory (`1M × 256B`) | Why This Happens in Silicon |
| :--- | :---: | :---: | :---: | :--- |
| **`folly::F14FastMap` (`F14VectorMap`)** | **`5.22 ms` (`191.5 M/s`)** / **`5.15 ms`** | `23.38 M/s` (`42.8 ns`) | **`252.2 MiB`** (`-51%`) | Streams dense packed `244 MiB` vector (`4.63M` LLC misses); hits pay 1 extra 32-bit index hop |
| **`boost::unordered_flat_map`** | `19.20 ms` (`52.1 M/s`) / `18.3 ms` | **`38.39 M/s`** (`26.1 ns`) | `481.9 MiB` | Inline `256B` values eliminate hop on hits, but flush must skip `~1M` empty `256B` holes (`52.8M` LLC misses) |
| **`absl::flat_hash_map`** | `19.71 ms` (`50.7 M/s`) / `20.2 ms` | **`41.56 M/s`** (`24.1 ns`) | **`513.9 MiB`** | Fastest random hits (`1.78×` over F14Vector), at the cost of **`2.04×` RAM** and `3.8×` slower sweeps (`62.8M` LLC misses) |
| **`folly::F14ValueMap` (Inline)** | `21.62 ms` (`46.3 M/s`) | `30.22 M/s` (`33.1 ns`) | `450.1 MiB` | Shows why Meta auto-switches `F14FastMap` to `F14VectorMap` at `sizeof(value_type) >= 24` |
| **`absl::flat<K, unique_ptr<V>>`** | `28.49 ms` (`35.1 M/s`) | `20.80 M/s` (`48.1 ns`) | `278.2 MiB` | Saves `236 MiB` vs inline `absl::flat_hash_map` + preserves value pointers, with heap pointer chase |
| **`std::unordered_map`** | **`67.26 ms` (`14.9 M/s`)** / `71.8 ms` | `17.00 M/s` (`58.8 ns`) | `267.5 MiB` | **`12.9×` slower flush** than `F14FastMap` (`70.0M` LLC misses, `10.9M` dTLB misses) |

---

## 4. Story 4: Long-Running Steady-State Eviction Churn (Tombstones vs. Ref-Counted Overflow vs. Anti-Drift)

### Production Scenario
A long-lived sliding-window flow/connection tracker maintains a constant live working set (`32B` `CompactFlowStats`) while executing **`4,000,000` continuous `erase(oldest) + emplace(newest)` cycles**. Because `F14VectorMap` uses 12-slot 64-byte cache-line chunks (`kDesiredCapacity = 10`, verified in `F14Table.h:611-613`) whose doubling boundaries differ from `absl` (`2^20`), `boost` (`15 × 2^16`), and `F14ValueMap` (`14 × 2^16`), we honestly measure **both** high-occupancy bands so every engine is tested right near its own `~75%–85%` load ceiling:

### 4A. Band A (`780,000` Live Entries: `absl 74.4%`, `boost 79.3%`, `F14Value 85.0%`, `F14Vector 49.6%`)

| Container & Deletion Mechanism | Churn Rate | `p50` | `p99` | `p99.9` | `Max` Stall | `80/20` Before $\rightarrow$ After | `100%` Miss Before $\rightarrow$ After |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`std::unordered_map` (Heap Nodes)** | `2.83 M/s` | `291 ns` | `972 ns` | `1,473 ns` | `70.4 µs` | `24.6 -> 24.7 M/s` (`0%` drift) | `29.5 -> 28.3 M/s` |
| **`absl::flat_hash_map` (`kDeleted` Tombstones)** | `7.08 M/s` | `110 ns` | `401 ns` | `641 ns` | **`6.23 ms`** | `50.8 -> 46.4 M/s` | **`108.0 -> 52.3 M/s`** (**`-52%` / `2.07×` slower**) |
| **`boost::unordered_flat_map` (`ofw` + Anti-Drift)** | `7.38 M/s` | `50 ns` | `390 ns` | `611 ns` | **`20.51 ms`** | **`53.5 -> 53.0 M/s`** (**`-1%`!**) | **`211.5 -> 204.2 M/s`** (**`-3%` — stays fresh!**) |
| **`folly::F14ValueMap` (RefCnt `ofw`, `85%` fill)** | `8.01 M/s` | `71 ns` | `401 ns` | `621 ns` | `638.9 µs` | `56.5 -> 36.6 M/s` | **`95.5 -> 27.6 M/s`** (**`-71%` / `3.46×` slower**) |
| **`folly::F14FastMap` (`F14Vector`, `50%` fill)** | **`16.43 M/s`** | **`40 ns`** | **`190 ns`** | **`411 ns`** | **`21.3 µs`** | `50.1 -> 45.6 M/s` | **`318.9 -> 274.1 M/s`** |

### 4B. Band B (`640,000` Live Entries: `F14VectorMap` Packed Tight at `81.4%` — `9.8/12` Slots/Chunk!)

| Container & Deletion Mechanism | Churn Rate | `p50` | `p99` | `p99.9` | `Max` Stall | `80/20` Before $\rightarrow$ After | `100%` Miss Before $\rightarrow$ After |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`std::unordered_map`** | `3.14 M/s` | `260 ns` | `911 ns` | `1,393 ns` | `479.2 µs` | `24.5 -> 24.1 M/s` | `30.6 -> 28.7 M/s` |
| **`absl::flat_hash_map` (`61.0%` fill)** | `7.49 M/s` | `110 ns` | `401 ns` | `632 ns` | **`5.26 ms`** | `61.6 -> 50.6 M/s` | **`177.4 -> 90.7 M/s`** (**`-49%`**) |
| **`folly::F14ValueMap` (`81.4%` of max)** | `8.32 M/s` | `60 ns` | `401 ns` | `641 ns` | **`28.8 µs`** | `52.1 -> 48.3 M/s` | `203.2 -> 101.3 M/s` (`-50%`) |
| **`boost::unordered_flat_map` (`65.1%` fill)** | `10.73 M/s` | **`40 ns`** | `341 ns` | `531 ns` | **`18.50 ms`** | `86.3 -> 68.8 M/s` | **`336.2 -> 181.7 M/s`** |
| **`folly::F14FastMap` (`F14Vector`, `81.4%` fill!)** | **`12.10 M/s`** | `50 ns` | **`271 ns`** | **`481 ns`** | **`59.7 µs`** | `73.0 -> 45.6 M/s` | **`134.4 -> 41.1 M/s`** (**`-69%` / `3.27×` drift!**) |

> **The Honest Algorithmic Trade-Off Exposed by Bare Metal:**
> 1. **Folly F14 (Reference-Counted Overflow):** Delivers the fastest churn (`12.1–16.4 M cyc/s`) and **zero rehash tail spikes (`21–60 µs` max — `88×–310×` lower than Abseil/Boost!)**, *but* because it never triggers an automatic cleanup rehash under constant size, packing F14 near its `81–85%` ceiling for millions of evictions allows probe-chain drift (`3.3×–3.5×` slower negative misses—solved by reserving ~20% headroom or periodic `rehash`).
> 2. **Boost.Unordered (`group15` + Anti-Drift):** Deliberately trades periodic **`18.5–20.5 ms` anti-drift rehash pauses** to completely eliminate long-term lookup drift (`211.5 -> 204.2 M/s` at 79.3% load!).
> 3. **Abseil SwissTable (`kDeleted` Tombstones):** Sits in between—periodic **`5.3–6.2 ms` `drop_deletes_without_resize()` pauses** while tombstones between cleanups cut negative-miss speed roughly in half (`-49%` to `-52%`).

---

## 5. Story 5: The Migration Landmine — Pointer Stability, ASan UAF & Industrial SIMD Node Maps

### 5A. Live Migration Bug Reproduction (`live_uaf_migration_repro`)
A realistic parent-tenant inheritance loop (`const RoutePolicy& parent = policies.at(1); policies.emplace(child, parent);` — **Abseil Tip of the Week #136**) compiles with **zero warnings under `-Wall -Wextra`** and ran safely for years on `std::unordered_map`. Under `absl::flat_hash_map`, `folly::F14FastMap`, and `boost::unordered_flat_map`, rehash relocates the backing array (`0x5ea86f351618 -> 0x79788296a1b0`)—triggering an immediate **`AddressSanitizer: heap-use-after-free` (`READ of size 24` inside freed `DeallocateBackingArray`)** when tested under `-fsanitize=address`.

### 5B. The Real Performance & RAM Cost of Keeping Pointer Stability (`1M × 128B ConnectionMetadata`)

| Container | Address Stability | `1M × 128B` RSS | `80/20` Lookups (`challenge` / `GBench`) | `100%` Negative Misses |
| :--- | :---: | :---: | :---: | :---: |
| **`std::unordered_map`** | Full (Key + Val) | `145.5 MiB` | `13.58 M/s` / `13.62 M/s` | `32.24 M/s` (`31.0 ns`) |
| **`absl::node_hash_map`** | **Full (Key + Val)** | `155.4 MiB` | **`29.70 M/s` / `44.20 M/s`** (`2.2×–3.2×`) | **`162.93 M/s`** (**`5.1×`**) |
| **`folly::F14NodeMap`** | **Full (Key + Val)** | **`153.4 MiB`** | **`37.92 M/s` / `33.00 M/s`** (`2.4×–2.8×`) | **`255.28 M/s`** (**`7.9×`**) |
| **`boost::unordered_node_map`** | **Full (Key + Val)** | `154.4 MiB` | **`37.40 M/s` / `39.22 M/s`** (`2.8×–2.9×`) | **`318.88 M/s`** (**`9.9×`**) |
| **`absl::flat<K, unique_ptr<V>>`** | Value Only | `156.1 MiB` | `27.51 M/s` / `36.38 M/s` (`2.0×–2.7×`) | **`235.74 M/s`** (**`7.3×`**) |
| *Reference: `folly::F14FastMap` (Flat/Vec)* | *None (Moves)* | *`130.1 MiB`* | *`42.04 M/s` / `51.66 M/s`* | *`236.12 M/s`* |
| *Reference: `boost::unordered_flat_map`* | *None (Moves)* | *`242.1 MiB`* | *`49.73 M/s` / `63.54 M/s`* | *`329.12 M/s`* |
| *Reference: `absl::flat_hash_map`* | *None (Moves)* | *`258.1 MiB`* | *`45.02 M/s` / `52.12 M/s`* | *`137.56 M/s`* |

> **Major Practical Takeaway:** Even when a service genuinely needs 100% pointer stability (or during staged migrations off `std::unordered_map`), **`folly::F14NodeMap`, `boost::unordered_node_map`, and `absl::node_hash_map` deliver `2.2×–3.2×` faster `80/20` lookups and `5×–10×` faster negative misses** for only `~8–10 MiB` extra metadata—because their probing layer is still SIMD open addressing!

---

## 6. Strictly Unmultiplexed Linux `perf stat` Hardware PMU Telemetry (Zen 4, 100% Uptime)

Three independent passes (`<= 4` hardware counters per pass):

### 6A. Story 1 — Zipfian `70/30` & `100%` Miss Route Lookups (`20,000,000` Lookups)

| Workload & Container | Cycles | Instructions | IPC | Branch Misses | L1D Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Zipf: `std::unordered_map` (Default Alloc)** | `9.01B` | `9.32B` | `1.03` | `46.03M` | `176.01M` | `27.01M` | `94.03M` |
| **Zipf: `std::unordered_map` (`noexcept` Trap)** | `6.87B` | `5.83B` | `0.85` | `49.77M` | `189.87M` | `29.68M` | `107.72M` |
| **Zipf: `std::unordered_map` (C++20 Cached)** | `7.00B` | **`4.86B`** (`-48%`) | `0.70` | `45.79M` | `175.18M` | `27.21M` | `95.32M` |
| **Zipf: `std::pmr::unordered_map` (Arena)** | `5.22B` | `5.63B` | `1.08` | `49.11M` | `195.10M` | **`3.49M`** (**`-87%`**) | `99.70M` |
| **Zipf: `absl::flat_hash_map`** | `3.93B` | **`3.70B`** | `0.94` | `25.09M` | `144.64M` | `11.44M` | `58.20M` |
| **Zipf: `folly::F14FastMap`** | **`3.61B`** | `4.10B` | `1.13` | **`22.79M`** | **`136.39M`** | `9.44M` | `56.16M` |
| **Zipf: `boost::unordered_flat_map`** | `3.70B` | `4.84B` | **`1.31`** | `28.87M` | `140.38M` | `10.68M` | **`54.03M`** |
| **Miss: `std::unordered_map` (`noexcept` Trap)** | `8.39B` | `6.21B` | `0.74` | `54.79M` | `181.80M` | `35.45M` | `123.68M` |
| **Miss: `std::unordered_map` (C++20 Cached)** | `5.31B` | `4.28B` | `0.81` | `42.72M` | `153.19M` | `28.43M` | `97.82M` |
| **Miss: `absl::flat_hash_map`** | `2.04B` | **`2.90B`** | `1.42` | `23.39M` | `117.42M` | `1.91M` | `37.65M` |
| **Miss: `folly::F14FastMap`** | **`1.76B`** | `3.06B` | `1.74` | **`15.93M`** | `85.58M` | **`1.46M`** | `25.52M` |
| **Miss: `boost::unordered_flat_map`** | `2.05B` | `4.12B` | **`2.01`** | `19.01M` | **`83.51M`** | `1.74M` | **`14.48M`** |

### 6B. Story 3 (`20` Full Sweeps × `1M × 256B`) & Story 4 (`4M` Steady-State Churn Cycles)

| Workload & Container | Cycles | Instructions | IPC | Branch Misses | `dTLB` Load Misses | LLC `cache-misses` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **Iter 256B: `std::unordered_map`** | `9.75B` | `1.71B` | **`0.18`** | `13.77M` | `10.88M` | `70.00M` |
| **Iter 256B: `absl::flat_hash_map`** | `3.81B` | `1.94B` | `0.51` | `39.47M` | `4.25M` | `62.81M` |
| **Iter 256B: `boost::unordered_flat_map`** | `3.60B` | `2.01B` | `0.56` | `22.25M` | `3.99M` | `52.80M` |
| **Iter 256B: `folly::F14FastMap` (Dense Vector)** | **`1.52B`** | **`1.19B`** | **`0.79`** | **`11.05M`** | **`1.65M`** | **`4.63M`** (**`-92.6%`** vs absl!) |
| **Churn 4M (`780K`): `std::unordered_map`** | `5.19B` | `2.16B` | `0.42` | `10.65M` | `17.55M` | `36.51M` |
| **Churn 4M (`780K`): `absl::flat_hash_map`** | `1.58B` | `1.78B` | `1.13` | `15.59M` | `7.96M` | `37.10M` |
| **Churn 4M (`780K`): `boost::unordered_flat_map`** | `1.50B` | `1.26B` | `0.84` | `9.59M` | `6.32M` | `24.07M` |
| **Churn 4M (`780K`): `folly::F14ValueMap` (`85%`)** | `1.75B` | `1.36B` | `0.78` | `12.03M` | `50.83M` | `105.48M` |
| **Churn 4M (`780K`): `folly::F14FastMap` (`50%`)** | **`0.69B`** | `1.33B` | **`1.93`** | **`3.44M`** | **`0.66M`** | **`11.71M`** |
| **Churn 4M (`640K`): `folly::F14FastMap` (`81%`)** | **`1.32B`** | `1.57B` | `1.19` | `11.29M` | **`0.78M`** | `41.06M` |

---

## 7. Verified Zen 4 Silicon Anatomy (`assembly/godbolt_comparison.s`)

All four compiled production kernels are archived in [`assembly/godbolt_comparison.s`](./assembly/godbolt_comparison.s):
1. **`std::unordered_map` (`Ep16DisasmStdFindU64`):** `div r8` hardware division by prime `bucket_count` $\rightarrow$ linked-list pointer chase (`mov rdi, [rax]`); for strings with default non-transparent lookup (`Ep16StdDefaultFindRoute`), emits `call _Znwm` (`operator new`), `memcpy`, and `call _ZdlPv` (`operator delete`) on every string `> 15B`.
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
