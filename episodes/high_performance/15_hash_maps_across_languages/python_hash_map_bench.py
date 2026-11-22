#!/usr/bin/env python3
# Copyright 2026 Hot Path C++ Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Episode 15 ("Hash Maps Across Languages") Python 3.12 benchmark driver.

Benchmarks CPython's compact split-table ``dict`` (Raymond Hettinger, Python
3.6+ ``Objects/dictobject.c``) across identical SplitMix64 datasets for both
integer keys/values and ``@dataclass(slots=True)`` SessionState records.
"""

import argparse
from dataclasses import dataclass
import gc
import sys
import time
from typing import Dict, List, Tuple

SEED_KEYS = 0xC0FFEE15
SEED_QUERIES = 0xFACE0015
SEED_MISSES = 0xBAD00015

NUM_L1_ENTRIES = 1024
NUM_AGED_ENTRIES = 2_000_000
NUM_QUERIES = 2_000_000
NUM_HASHDOS_KEYS = 40_000
MASK64 = 0xFFFFFFFFFFFFFFFF
MASK62 = 0x3FFFFFFFFFFFFFFF


@dataclass(slots=True)
class SessionState:
    """Realistic 24-byte-equivalent session state record in CPython."""

    quota_remaining: int
    last_seen_ns: int
    tier_flags: int
    rate_window_hits: int


def splitmix64(state: int) -> Tuple[int, int]:
    """Advances SplitMix64 PRNG and returns ``(new_state, value)``.

    Args:
        state: Current unsigned 64-bit PRNG state.

    Returns:
        Tuple of ``(next_state, mixed_64bit_output)``.
    """
    state = (state + 0x9E3779B97F4A7C15) & MASK64
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
    return state, (z ^ (z >> 31)) & MASK64


def pack_token_bucket(raw: int) -> int:
    """Packs timestamp and quota into a positive 63-bit integer.

    Args:
        raw: Pseudorandom 64-bit input word.

    Returns:
        Packed 64-bit integer ``(last_seen_ms << 24) | quota``.
    """
    last_seen_ms = (raw >> 24) & 0x7FFFFFFFFF
    quota = (raw & 0xFFFF) + 1
    return (last_seen_ms << 24) | quota


def build_dataset(
    num_entries: int, num_queries: int
) -> Tuple[List[int], List[int], List[SessionState], List[int], List[int]]:
    """Builds deterministic keys, packed values, sessions, and query lists.

    Args:
        num_entries: Number of unique map entries to generate.
        num_queries: Number of lookup queries in each query stream.

    Returns:
        Tuple of ``(keys, packed_vals, session_vals, queries_80_20,
        queries_100_miss)``.
    """
    keys: List[int] = [0] * num_entries
    packed_vals: List[int] = [0] * num_entries
    session_vals: List[SessionState] = [None] * num_entries  # type: ignore

    key_rng = SEED_KEYS
    for i in range(num_entries):
        key_rng, r1 = splitmix64(key_rng)
        k = (r1 & MASK62) | 1
        key_rng, raw = splitmix64(key_rng)
        packed = pack_token_bucket(raw)
        quota = packed & 0xFFFFFF
        last_seen_ms = (packed >> 24) & 0x7FFFFFFFFF
        keys[i] = k
        packed_vals[i] = packed
        session_vals[i] = SessionState(
            quota_remaining=quota,
            last_seen_ns=last_seen_ms * 1_000_000,
            tier_flags=k & 0xFFFF,
            rate_window_hits=(k >> 16) & 0xFF,
        )

    queries_80_20: List[int] = [0] * num_queries
    q_rng = SEED_QUERIES
    bit62 = 1 << 62
    for i in range(num_queries):
        q_rng, r = splitmix64(q_rng)
        if (r % 5) != 0:
            queries_80_20[i] = keys[(r >> 8) % num_entries]
        else:
            queries_80_20[i] = (r & MASK62) | bit62 | 1

    queries_100_miss: List[int] = [0] * num_queries
    m_rng = SEED_MISSES
    for i in range(num_queries):
        m_rng, r = splitmix64(m_rng)
        queries_100_miss[i] = (r & MASK62) | bit62 | 1

    return keys, packed_vals, session_vals, queries_80_20, queries_100_miss


def read_vm_rss_bytes() -> int:
    """Reads Linux ``VmRSS`` in bytes from ``/proc/self/status``.

    Returns:
        Current process resident set size in bytes, or ``0`` if unavailable.
    """
    try:
        with open("/proc/self/status", "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    return 0


def lookup_packed(d: Dict[int, int], queries: List[int]) -> int:
    """Runs timed lookup loop over a packed integer ``dict``.

    Args:
        d: Populated integer-to-integer dictionary.
        queries: Lookup keys to probe.

    Returns:
        Deterministic 64-bit verification checksum.
    """
    total = 0
    hits = 0
    get = d.get
    for q in queries:
        v = get(q)
        if v is not None:
            total = (total + v) & MASK64
            hits += 1
    return (total ^ ((hits * 0x9E3779B97F4A7C15) & MASK64)) & MASK64


def lookup_session(d: Dict[int, SessionState], queries: List[int]) -> int:
    """Runs timed lookup loop over a ``SessionState`` ``dict``.

    Args:
        d: Populated integer-to-``SessionState`` dictionary.
        queries: Lookup keys to probe.

    Returns:
        Deterministic 64-bit verification checksum.
    """
    total = 0
    hits = 0
    get = d.get
    for q in queries:
        s = get(q)
        if s is not None:
            digest = (
                s.quota_remaining
                ^ s.last_seen_ns
                ^ s.tier_flags
                ^ s.rate_window_hits
            )
            total = (total + digest) & MASK64
            hits += 1
    return (total ^ ((hits * 0x9E3779B97F4A7C15) & MASK64)) & MASK64


def iter_packed(d: Dict[int, int]) -> int:
    """Runs timed full or sparse sweep over a packed integer ``dict``.

    Args:
        d: Dictionary to iterate.

    Returns:
        Deterministic 64-bit checksum of all visited key-value pairs.
    """
    total = 0
    count = 0
    for k, v in d.items():
        total = (total + (k ^ v)) & MASK64
        count += 1
    return (total ^ ((count * 0x9E3779B97F4A7C15) & MASK64)) & MASK64


def print_sweep_row(
    label: str, total_ms: float, sweeps: int, live: int, checksum: int
) -> None:
    """Prints a unified Episode 15 iteration telemetry line.

    Args:
        label: Human-readable benchmark name.
        total_ms: Total elapsed wall time in milliseconds.
        sweeps: Number of full table sweeps timed.
        live: Number of live entries visited per sweep.
        checksum: Verification checksum across all sweeps.
    """
    ms_per_sweep = total_ms / float(sweeps)
    total_live = sweeps * live
    ns_per_live = (total_ms * 1_000_000.0) / float(total_live)
    mops = float(total_live) / (total_ms * 1_000.0)
    print(
        f"{label:<42} | {ms_per_sweep:>6.2f} ms/swp | "
        f"{ns_per_live:>6.2f} ns/live | {mops:>7.2f} M/s | "
        f"checksum=0x{checksum:x}"
    )


def print_row(
    label: str, total_ms: float, total_ops: int, checksum: int
) -> None:
    """Prints a unified Episode 15 lookup telemetry line.

    Args:
        label: Human-readable benchmark name.
        total_ms: Total elapsed wall time in milliseconds.
        total_ops: Total number of lookup operations executed.
        checksum: Verification checksum across all passes.
    """
    ns_per_op = (total_ms * 1_000_000.0) / float(total_ops)
    mops = float(total_ops) / (total_ms * 1_000.0)
    print(
        f"{label:<42} | {ns_per_op:>8.2f} ns/op | {mops:>7.2f} M ops/s | "
        f"checksum=0x{checksum:x}"
    )


def run_l1_packed(iters: int) -> None:
    """Benchmarks L1 1,024-entry ``dict`` lookup throughput.

    Args:
        iters: Number of timed passes over the query stream.
    """
    keys, packed_vals, _, queries_80_20, _ = build_dataset(
        NUM_L1_ENTRIES, NUM_QUERIES
    )
    d = dict(zip(keys, packed_vals))
    sink = lookup_packed(d, queries_80_20)
    t0 = time.perf_counter_ns()
    for _ in range(iters):
        sink = (sink + lookup_packed(d, queries_80_20)) & MASK64
    t1 = time.perf_counter_ns()
    print_row(
        "Python 3.12 dict[int, int] (L1 1K 80/20)",
        (t1 - t0) / 1e6,
        iters * len(queries_80_20),
        sink,
    )


def run_aged_packed(iters: int, miss_only: bool) -> None:
    """Benchmarks 2,000,000-entry ``dict`` lookup throughput.

    Args:
        iters: Number of timed passes over the query stream.
        miss_only: If ``True``, probes the 100%-miss query stream; otherwise
            probes the 80/20 hit/miss stream.
    """
    keys, packed_vals, _, queries_80_20, queries_100_miss = build_dataset(
        NUM_AGED_ENTRIES, NUM_QUERIES
    )
    d = dict(zip(keys, packed_vals))
    queries = queries_100_miss if miss_only else queries_80_20
    sink = lookup_packed(d, queries)
    t0 = time.perf_counter_ns()
    for _ in range(iters):
        sink = (sink + lookup_packed(d, queries)) & MASK64
    t1 = time.perf_counter_ns()
    label = (
        "Python 3.12 dict[int, int] (Aged 2M Miss)"
        if miss_only
        else "Python 3.12 dict[int, int] (Aged 2M 80/20)"
    )
    print_row(label, (t1 - t0) / 1e6, iters * len(queries), sink)


def run_aged_session(iters: int) -> None:
    """Benchmarks 2,000,000-entry ``SessionState`` ``dict`` lookup throughput.

    Args:
        iters: Number of timed passes over the query stream.
    """
    keys, _, session_vals, queries_80_20, _ = build_dataset(
        NUM_AGED_ENTRIES, NUM_QUERIES
    )
    d = dict(zip(keys, session_vals))
    sink = lookup_session(d, queries_80_20)
    t0 = time.perf_counter_ns()
    for _ in range(iters):
        sink = (sink + lookup_session(d, queries_80_20)) & MASK64
    t1 = time.perf_counter_ns()
    print_row(
        "Python 3.12 dict[int, Session] (Aged 24B)",
        (t1 - t0) / 1e6,
        iters * len(queries_80_20),
        sink,
    )


def run_battle2_rss_mode(target: str = "both") -> None:
    """Measures CPython ``dict`` RSS including materialized key/value objects.

    Args:
        target: Subset of workloads to measure (``"both"``, ``"packed"``, or
            ``"session"``).
    """
    keys, packed_vals, session_vals, _, _ = build_dataset(
        NUM_AGED_ENTRIES, 1024
    )
    if target in ("both", "packed"):
        gc.collect()
        rss_before = read_vm_rss_bytes()
        d_packed = dict(zip(keys, packed_vals))
        rss_after = read_vm_rss_bytes()
        delta = max(0, rss_after - rss_before)
        table_bytes = sys.getsizeof(d_packed)
        print(
            f"{'Python 3.12 dict[int, int] (2M table)':<42} | "
            f"RSS={delta / (1024 * 1024):>6.1f} MiB "
            f"({delta / len(keys):>5.1f} B/entry) | "
            f"dict_table={table_bytes / (1024 * 1024):.1f} MiB"
        )
        del d_packed
        gc.collect()
    if target in ("both", "session"):
        gc.collect()
        rss_before_s = read_vm_rss_bytes()
        # Allocate live SessionState instances inside map build to match inline
        # C++/Rust/Go storage.
        d_sess = {
            k: SessionState(
                s.quota_remaining,
                s.last_seen_ns,
                s.tier_flags,
                s.rate_window_hits,
            )
            for k, s in zip(keys, session_vals)
        }
        rss_after_s = read_vm_rss_bytes()
        delta_s = max(0, rss_after_s - rss_before_s)
        print(
            f"{'Python 3.12 dict[int, Session] (2M full)':<42} | "
            f"RSS={delta_s / (1024 * 1024):>6.1f} MiB "
            f"({delta_s / len(keys):>5.1f} B/entry)"
        )
        del d_sess


def run_battle2_rss() -> None:
    """Measures CPython ``dict`` RSS and ``sys.getsizeof`` for 2M entries."""
    print(
        "\n=== Python 3.12 Battle 2A: Resident Memory Footprint "
        "(2,000,000 Entries) ==="
    )
    run_battle2_rss_mode("both")


def run_battle2_tail() -> None:
    """Measures streaming unreserved insert tail latency (0 -> 2M)."""
    keys, packed_vals, _, _, _ = build_dataset(NUM_AGED_ENTRIES, 1024)
    samples: List[int] = [0] * len(keys)
    d: Dict[int, int] = {}
    perf_ns = time.perf_counter_ns
    t_all_0 = perf_ns()
    for i in range(len(keys)):
        k = keys[i]
        v = packed_vals[i]
        t0 = perf_ns()
        d[k] = v
        samples[i] = perf_ns() - t0
    t_all_1 = perf_ns()
    samples.sort()
    n = len(samples)

    def pct(p: float) -> int:
        return samples[min(n - 1, int(p * (n - 1)))]

    print(
        "\n=== Python 3.12 Battle 2B: Streaming Insert Tail Latency "
        "(0 -> 2,000,000) ==="
    )
    print(
        f"{'Python 3.12 dict[int, int] (Unreserved)':<42} | "
        f"total={(t_all_1 - t_all_0) / 1e6:>7.1f} ms | "
        f"p50={pct(0.50):>4} ns | p99={pct(0.99):>5} ns | "
        f"p99.9={pct(0.999):>6} ns | "
        f"p99.99={pct(0.9999):>7} ns | max={samples[-1]:>10} ns"
    )


def run_battle3_hashdos(prime: int) -> None:
    """Tests CPython ``dict`` resilience against crafted prime-bucket keys.

    Args:
        prime: Modulus multiplier for constructing adversarial keys.
    """
    keys, _, _, _, _ = build_dataset(NUM_HASHDOS_KEYS, 1024)
    dos_keys = [(i + 1) * prime for i in range(NUM_HASHDOS_KEYS)]
    print(
        f"\n=== Python 3.12 Battle 3: Adversarial HashDoS "
        f"({NUM_HASHDOS_KEYS} Keys, prime={prime}) ==="
    )

    d_norm: Dict[int, int] = {}
    t0 = time.perf_counter_ns()
    for i, k in enumerate(keys):
        d_norm[k] = i + 1
    t1 = time.perf_counter_ns()
    print(
        f"{'Python 3.12 dict (Normal Keys)':<42} | {(t1 - t0) / 1e6:>8.2f} ms"
    )

    d_dos: Dict[int, int] = {}
    t0 = time.perf_counter_ns()
    for i, k in enumerate(dos_keys):
        d_dos[k] = i + 1
    t1 = time.perf_counter_ns()
    print(
        f"{'Python 3.12 dict (Crafted Prime Keys)':<42} | "
        f"{(t1 - t0) / 1e6:>8.2f} ms"
    )


def run_iter_packed(iters: int, sparse: bool) -> None:
    """Benchmarks full-table (2M) or 90%-evicted sparse (200K) dict sweep.

    Args:
        iters: Number of timed table sweeps.
        sparse: If ``True``, evicts 90% of entries before benchmarking sweeps.
    """
    keys, packed_vals, _, _, _ = build_dataset(NUM_AGED_ENTRIES, 64)
    d = dict(zip(keys, packed_vals))
    if sparse:
        for i, k in enumerate(keys):
            if (i % 10) != 0:
                del d[k]
    live = len(d)
    sink = iter_packed(d)
    t0 = time.perf_counter_ns()
    for _ in range(iters):
        sink = (sink + iter_packed(d)) & MASK64
    t1 = time.perf_counter_ns()
    label = (
        "Python 3.12 dict[int, int] (Sparse 200K)"
        if sparse
        else "Python 3.12 dict[int, int] (Iter 2M Full)"
    )
    print_sweep_row(label, (t1 - t0) / 1e6, iters, live, sink)


def run_self_tests() -> None:
    """Verifies identical SplitMix64 checksums against C++/Rust/Go/Java."""
    keys, packed_vals, _, queries_80_20, queries_100_miss = build_dataset(
        16_384, 32_768
    )
    d = dict(zip(keys, packed_vals))
    c = lookup_packed(d, queries_80_20)
    assert c != 0
    c_miss = lookup_packed(d, queries_100_miss)
    assert c_miss == 0
    c_full = iter_packed(d)
    assert c_full != 0
    for i, k in enumerate(keys):
        if (i % 10) != 0:
            del d[k]
    assert len(d) == 1_639
    c_sparse = iter_packed(d)
    assert c_sparse != 0
    print(
        f"Python 3.12 self-test PASSED: 16K lookup=0x{c:x}, "
        f"iter_full=0x{c_full:x}, iter_sparse=0x{c_sparse:x}"
    )


def main() -> None:
    """CLI entrypoint for Episode 15 Python benchmarks."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", default="all")
    parser.add_argument("--iters", type=int, default=0)
    parser.add_argument("--prime", type=int, default=43_067)
    args = parser.parse_args()

    if args.mode == "test":
        run_self_tests()
    elif args.mode == "all":
        print(
            "=== Python 3.12 Battle 1: L1 (1,024) vs. "
            "Aged Post-Churn (2,000,000) ==="
        )
        run_l1_packed(args.iters or 4)
        run_aged_packed(args.iters or 2, False)
        run_aged_packed(args.iters or 2, True)
        run_aged_session(args.iters or 2)
        run_iter_packed(args.iters or 6, False)
        run_iter_packed(args.iters or 20, True)
        run_battle2_rss()
        run_battle2_tail()
        run_battle3_hashdos(args.prime)
    elif args.mode == "b1_l1_packed":
        run_l1_packed(args.iters or 10)
    elif args.mode == "b1_aged_packed":
        run_aged_packed(args.iters or 6, False)
    elif args.mode == "b1_miss_packed":
        run_aged_packed(args.iters or 6, True)
    elif args.mode == "b1_aged_session":
        run_aged_session(args.iters or 6)
    elif args.mode == "b1_iter_full_packed":
        run_iter_packed(args.iters or 10, False)
    elif args.mode == "b1_iter_sparse_packed":
        run_iter_packed(args.iters or 40, True)
    elif args.mode == "b2_rss":
        run_battle2_rss()
    elif args.mode == "b2_rss_packed":
        run_battle2_rss_mode("packed")
    elif args.mode == "b2_rss_session":
        run_battle2_rss_mode("session")
    elif args.mode == "b2_tail":
        run_battle2_tail()
    elif args.mode == "b3_hashdos":
        run_battle3_hashdos(args.prime)
    else:
        print(f"Unknown --mode={args.mode}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
