// Copyright 2026 Hot Path C++ Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Episode 15 ("Hash Maps Across Languages") Rust 1.91 benchmark & PMU driver.
//!
//! Benchmarks Rust's default `std::collections::HashMap` (hashbrown SwissTable
//! + SipHash-1-3) alongside the compiler's fast non-cryptographic `FxHasher`
//! (`rustc-hash`, K = 0x517cc1b727220a95) across identical SplitMix64 datasets.

use std::collections::HashMap;
use std::env;
use std::fs;
use std::hash::{BuildHasherDefault, Hasher};
use std::hint::black_box;
use std::time::Instant;

const SEED_KEYS: u64 = 0xC0FFEE15;
const SEED_QUERIES: u64 = 0xFACE0015;
const SEED_MISSES: u64 = 0xBAD00015;
const SEED_CHURN: u64 = 0x99990015;

const NUM_L1_ENTRIES: usize = 1024;
const NUM_AGED_ENTRIES: usize = 2_000_000;
const NUM_QUERIES: usize = 2_000_000;
const NUM_HASHDOS_KEYS: usize = 40_000;
const FX_SEED: u64 = 0x517cc1b727220a95;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SessionState {
    pub quota_remaining: u64,
    pub last_seen_ns: u64,
    pub tier_flags: u32,
    pub rate_window_hits: u32,
}

/// Fast non-cryptographic hasher matching `rustc-hash` (`FxHash64`).
#[derive(Default)]
pub struct FxHasher {
    hash: u64,
}

impl FxHasher {
    #[inline(always)]
    fn add_to_hash(&mut self, word: u64) {
        self.hash = (self.hash.rotate_left(5) ^ word).wrapping_mul(FX_SEED);
    }
}

impl Hasher for FxHasher {
    #[inline(always)]
    fn finish(&self) -> u64 {
        self.hash
    }

    #[inline(always)]
    fn write(&mut self, bytes: &[u8]) {
        let mut i = 0;
        while i + 8 <= bytes.len() {
            let w = u64::from_ne_bytes(bytes[i..i + 8].try_into().unwrap());
            self.add_to_hash(w);
            i += 8;
        }
        if i < bytes.len() {
            let mut rem = 0u64;
            for (shift, b) in bytes[i..].iter().enumerate() {
                rem |= (*b as u64) << (shift * 8);
            }
            self.add_to_hash(rem);
        }
    }

    #[inline(always)]
    fn write_u64(&mut self, i: u64) {
        self.add_to_hash(i);
    }
}

type FxBuildHasher = BuildHasherDefault<FxHasher>;

#[inline(always)]
fn splitmix64(state: &mut u64) -> u64 {
    *state = state.wrapping_add(0x9e3779b97f4a7c15);
    let mut z = *state;
    z = (z ^ (z >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94d049bb133111eb);
    z ^ (z >> 31)
}

#[inline(always)]
fn make_present_key(state: &mut u64) -> u64 {
    (splitmix64(state) & 0x3FFF_FFFF_FFFF_FFFF) | 1
}

#[inline(always)]
fn make_absent_key(state: &mut u64) -> u64 {
    (splitmix64(state) & 0x3FFF_FFFF_FFFF_FFFF) | (1u64 << 62) | 1
}

#[inline(always)]
fn pack_token_bucket(raw: u64) -> u64 {
    let last_seen_ms = (raw >> 24) & 0x7F_FFFF_FFFF;
    let quota = (raw & 0xFFFF) + 1;
    (last_seen_ms << 24) | quota
}

#[inline(always)]
fn make_session_state(key: u64, packed: u64) -> SessionState {
    let quota = packed & 0xFFFFFF;
    let last_seen_ms = (packed >> 24) & 0x7F_FFFF_FFFF;
    SessionState {
        quota_remaining: quota,
        last_seen_ns: last_seen_ms * 1_000_000,
        tier_flags: (key & 0xFFFF) as u32,
        rate_window_hits: ((key >> 16) & 0xFF) as u32,
    }
}

#[inline(always)]
fn digest_session_state(s: &SessionState) -> u64 {
    s.quota_remaining ^ s.last_seen_ns ^ (s.tier_flags as u64) ^ (s.rate_window_hits as u64)
}

#[inline(always)]
fn finalize_checksum(sum: u64, hits: u64) -> u64 {
    sum ^ hits.wrapping_mul(0x9e3779b97f4a7c15)
}

struct GatewayDataset {
    keys: Vec<u64>,
    packed_vals: Vec<u64>,
    session_vals: Vec<SessionState>,
    queries_80_20: Vec<u64>,
    queries_100_miss: Vec<u64>,
}

fn build_dataset(num_entries: usize, num_queries: usize) -> GatewayDataset {
    let mut keys = Vec::with_capacity(num_entries);
    let mut packed_vals = Vec::with_capacity(num_entries);
    let mut session_vals = Vec::with_capacity(num_entries);

    let mut key_rng = SEED_KEYS;
    for _ in 0..num_entries {
        let k = make_present_key(&mut key_rng);
        let raw = splitmix64(&mut key_rng);
        let packed = pack_token_bucket(raw);
        keys.push(k);
        packed_vals.push(packed);
        session_vals.push(make_session_state(k, packed));
    }

    let mut queries_80_20 = Vec::with_capacity(num_queries);
    let mut q_rng = SEED_QUERIES;
    for _ in 0..num_queries {
        let r = splitmix64(&mut q_rng);
        if (r % 5) != 0 {
            let idx = ((r >> 8) as usize) % num_entries;
            queries_80_20.push(keys[idx]);
        } else {
            queries_80_20.push((r & 0x3FFF_FFFF_FFFF_FFFF) | (1u64 << 62) | 1);
        }
    }

    let mut queries_100_miss = Vec::with_capacity(num_queries);
    let mut m_rng = SEED_MISSES;
    for _ in 0..num_queries {
        queries_100_miss.push(make_absent_key(&mut m_rng));
    }

    GatewayDataset {
        keys,
        packed_vals,
        session_vals,
        queries_80_20,
        queries_100_miss,
    }
}

fn apply_heap_churn(chunk_count: usize) -> Vec<Option<Vec<u8>>> {
    let mut rng = SEED_CHURN;
    let mut chunks = Vec::with_capacity(chunk_count);
    for i in 0..chunk_count {
        let sz = 32 + (((splitmix64(&mut rng) & 7) as usize) * 32);
        let mut buf = vec![0u8; sz];
        buf[0] = (i & 0xFF) as u8;
        chunks.push(Some(buf));
    }
    for i in (0..chunks.len()).step_by(2) {
        chunks[i] = None;
    }
    chunks
}

fn read_vm_rss_bytes() -> u64 {
    if let Ok(text) = fs::read_to_string("/proc/self/status") {
        for line in text.lines() {
            if let Some(rest) = line.strip_prefix("VmRSS:") {
                let kb_str = rest.trim().split_whitespace().next().unwrap_or("0");
                if let Ok(kb) = kb_str.parse::<u64>() {
                    return kb * 1024;
                }
            }
        }
    }
    0
}

const fn mod_inverse_u64(k: u64) -> u64 {
    let mut inv = 1u64;
    let mut i = 0;
    while i < 6 {
        inv = inv.wrapping_mul(2u64.wrapping_sub(k.wrapping_mul(inv)));
        i += 1;
    }
    inv
}

fn build_fxhash_collision_keys(count: usize) -> Vec<u64> {
    let inv = mod_inverse_u64(FX_SEED);
    let mut out = Vec::with_capacity(count);
    for i in 1..=count {
        // Target FxHash output = (i << 16): low 16 bits == 0 AND top 32 bits == 0,
        // forcing identical H1 bucket AND identical H2 SIMD tag across hashbrown.
        let target = (i as u64) << 16;
        out.push(target.wrapping_mul(inv));
    }
    out
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_siphash_find_packed(
    map: &HashMap<u64, u64>,
    queries: *const u64,
    n: usize,
) -> u64 {
    let slice = unsafe { std::slice::from_raw_parts(queries, n) };
    let mut sum = 0u64;
    let mut hits = 0u64;
    for q in slice {
        if let Some(&v) = map.get(q) {
            sum = sum.wrapping_add(v);
            hits += 1;
        }
    }
    finalize_checksum(sum, hits)
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_fxhash_find_packed(
    map: &HashMap<u64, u64, FxBuildHasher>,
    queries: *const u64,
    n: usize,
) -> u64 {
    let slice = unsafe { std::slice::from_raw_parts(queries, n) };
    let mut sum = 0u64;
    let mut hits = 0u64;
    for q in slice {
        if let Some(&v) = map.get(q) {
            sum = sum.wrapping_add(v);
            hits += 1;
        }
    }
    finalize_checksum(sum, hits)
}

#[inline(never)]
fn find_session_sip(map: &HashMap<u64, SessionState>, queries: &[u64]) -> u64 {
    let mut sum = 0u64;
    let mut hits = 0u64;
    for q in queries {
        if let Some(v) = map.get(q) {
            sum = sum.wrapping_add(digest_session_state(v));
            hits += 1;
        }
    }
    finalize_checksum(sum, hits)
}

#[inline(never)]
fn find_session_fx(map: &HashMap<u64, SessionState, FxBuildHasher>, queries: &[u64]) -> u64 {
    let mut sum = 0u64;
    let mut hits = 0u64;
    for q in queries {
        if let Some(v) = map.get(q) {
            sum = sum.wrapping_add(digest_session_state(v));
            hits += 1;
        }
    }
    finalize_checksum(sum, hits)
}

fn print_lookup_row(label: &str, total_ms: f64, total_ops: usize, checksum: u64) {
    let ns_per_op = (total_ms * 1_000_000.0) / (total_ops as f64);
    let mops = (total_ops as f64) / (total_ms * 1_000.0);
    println!(
        "{:<42} | {:>8.2} ns/op | {:>7.2} M ops/s | checksum=0x{:x}",
        label, ns_per_op, mops, checksum
    );
}

fn run_l1_sip(iters: usize) {
    let ds = build_dataset(NUM_L1_ENTRIES, NUM_QUERIES);
    let _churn = apply_heap_churn(8_192);
    let mut map = HashMap::with_capacity(ds.keys.len());
    for i in 0..ds.keys.len() {
        map.insert(ds.keys[i], ds.packed_vals[i]);
    }
    drop(_churn);
    let mut sink =
        rust_siphash_find_packed(&map, ds.queries_80_20.as_ptr(), ds.queries_80_20.len());
    let t0 = Instant::now();
    for _ in 0..iters {
        let q = black_box(ds.queries_80_20.as_ptr());
        sink = sink.wrapping_add(black_box(rust_siphash_find_packed(
            &map,
            q,
            ds.queries_80_20.len(),
        )));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sink);
    print_lookup_row(
        "Rust 1.91 HashMap SipHash (L1 1K 80/20)",
        ms,
        iters * ds.queries_80_20.len(),
        sink,
    );
}

fn run_l1_fx(iters: usize) {
    let ds = build_dataset(NUM_L1_ENTRIES, NUM_QUERIES);
    let _churn = apply_heap_churn(8_192);
    let mut map: HashMap<u64, u64, FxBuildHasher> =
        HashMap::with_capacity_and_hasher(ds.keys.len(), FxBuildHasher::default());
    for i in 0..ds.keys.len() {
        map.insert(ds.keys[i], ds.packed_vals[i]);
    }
    drop(_churn);
    let mut sink = rust_fxhash_find_packed(&map, ds.queries_80_20.as_ptr(), ds.queries_80_20.len());
    let t0 = Instant::now();
    for _ in 0..iters {
        let q = black_box(ds.queries_80_20.as_ptr());
        sink = sink.wrapping_add(black_box(rust_fxhash_find_packed(
            &map,
            q,
            ds.queries_80_20.len(),
        )));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sink);
    print_lookup_row(
        "Rust 1.91 HashMap FxHash (L1 1K 80/20)",
        ms,
        iters * ds.queries_80_20.len(),
        sink,
    );
}

fn run_aged_sip(iters: usize, miss_only: bool) {
    let ds = build_dataset(NUM_AGED_ENTRIES, NUM_QUERIES);
    let churn = apply_heap_churn(300_000);
    let mut map = HashMap::with_capacity(ds.keys.len());
    for i in 0..ds.keys.len() {
        map.insert(ds.keys[i], ds.packed_vals[i]);
    }
    drop(churn);
    let queries = if miss_only {
        &ds.queries_100_miss
    } else {
        &ds.queries_80_20
    };
    let mut sink = rust_siphash_find_packed(&map, queries.as_ptr(), queries.len());
    let t0 = Instant::now();
    for _ in 0..iters {
        let q = black_box(queries.as_ptr());
        sink = sink.wrapping_add(black_box(rust_siphash_find_packed(&map, q, queries.len())));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sink);
    let label = if miss_only {
        "Rust 1.91 HashMap SipHash (Aged 2M Miss)"
    } else {
        "Rust 1.91 HashMap SipHash (Aged 2M 80/20)"
    };
    print_lookup_row(label, ms, iters * queries.len(), sink);
}

fn run_aged_fx(iters: usize, miss_only: bool) {
    let ds = build_dataset(NUM_AGED_ENTRIES, NUM_QUERIES);
    let churn = apply_heap_churn(300_000);
    let mut map: HashMap<u64, u64, FxBuildHasher> =
        HashMap::with_capacity_and_hasher(ds.keys.len(), FxBuildHasher::default());
    for i in 0..ds.keys.len() {
        map.insert(ds.keys[i], ds.packed_vals[i]);
    }
    drop(churn);
    let queries = if miss_only {
        &ds.queries_100_miss
    } else {
        &ds.queries_80_20
    };
    let mut sink = rust_fxhash_find_packed(&map, queries.as_ptr(), queries.len());
    let t0 = Instant::now();
    for _ in 0..iters {
        let q = black_box(queries.as_ptr());
        sink = sink.wrapping_add(black_box(rust_fxhash_find_packed(&map, q, queries.len())));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sink);
    let label = if miss_only {
        "Rust 1.91 HashMap FxHash (Aged 2M Miss)"
    } else {
        "Rust 1.91 HashMap FxHash (Aged 2M 80/20)"
    };
    print_lookup_row(label, ms, iters * queries.len(), sink);
}

fn run_aged_session_sip(iters: usize) {
    let ds = build_dataset(NUM_AGED_ENTRIES, NUM_QUERIES);
    let churn = apply_heap_churn(300_000);
    let mut map = HashMap::with_capacity(ds.keys.len());
    for i in 0..ds.keys.len() {
        map.insert(ds.keys[i], ds.session_vals[i]);
    }
    drop(churn);
    let mut sink = find_session_sip(&map, &ds.queries_80_20);
    let t0 = Instant::now();
    for _ in 0..iters {
        let q = black_box(ds.queries_80_20.as_slice());
        sink = sink.wrapping_add(black_box(find_session_sip(&map, q)));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sink);
    print_lookup_row(
        "Rust 1.91 HashMap SipHash (Aged 24B)",
        ms,
        iters * ds.queries_80_20.len(),
        sink,
    );
}

fn run_aged_session_fx(iters: usize) {
    let ds = build_dataset(NUM_AGED_ENTRIES, NUM_QUERIES);
    let churn = apply_heap_churn(300_000);
    let mut map: HashMap<u64, SessionState, FxBuildHasher> =
        HashMap::with_capacity_and_hasher(ds.keys.len(), FxBuildHasher::default());
    for i in 0..ds.keys.len() {
        map.insert(ds.keys[i], ds.session_vals[i]);
    }
    drop(churn);
    let mut sink = find_session_fx(&map, &ds.queries_80_20);
    let t0 = Instant::now();
    for _ in 0..iters {
        let q = black_box(ds.queries_80_20.as_slice());
        sink = sink.wrapping_add(black_box(find_session_fx(&map, q)));
    }
    let ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sink);
    print_lookup_row(
        "Rust 1.91 HashMap FxHash (Aged 24B)",
        ms,
        iters * ds.queries_80_20.len(),
        sink,
    );
}

fn run_battle2_rss() {
    let ds = build_dataset(NUM_AGED_ENTRIES, 1024);
    println!("\n=== Rust 1.91 Battle 2A: Resident Memory Footprint (2,000,000 Entries) ===");

    let rss_before_p = read_vm_rss_bytes();
    let mut map_p = HashMap::with_capacity(ds.keys.len());
    for i in 0..ds.keys.len() {
        map_p.insert(ds.keys[i], ds.packed_vals[i]);
    }
    black_box(map_p.len());
    let rss_after_p = read_vm_rss_bytes();
    let delta_p = rss_after_p.saturating_sub(rss_before_p);
    println!(
        "{:<42} | RSS={:>6.1} MiB | {:>5.1} B/entry",
        "Rust 1.91 HashMap (16B Packed)",
        (delta_p as f64) / (1024.0 * 1024.0),
        (delta_p as f64) / (ds.keys.len() as f64)
    );
    drop(map_p);

    let rss_before_s = read_vm_rss_bytes();
    let mut map_s = HashMap::with_capacity(ds.keys.len());
    for i in 0..ds.keys.len() {
        map_s.insert(ds.keys[i], ds.session_vals[i]);
    }
    black_box(map_s.len());
    let rss_after_s = read_vm_rss_bytes();
    let delta_s = rss_after_s.saturating_sub(rss_before_s);
    println!(
        "{:<42} | RSS={:>6.1} MiB | {:>5.1} B/entry",
        "Rust 1.91 HashMap (32B Session)",
        (delta_s as f64) / (1024.0 * 1024.0),
        (delta_s as f64) / (ds.keys.len() as f64)
    );
}

fn run_tail_insert(label: &str, ds: &GatewayDataset, reserve: bool) {
    let mut samples = vec![0u64; ds.keys.len()];
    let mut map = if reserve {
        HashMap::with_capacity(ds.keys.len())
    } else {
        HashMap::new()
    };
    let start_all = Instant::now();
    for i in 0..ds.keys.len() {
        let t0 = Instant::now();
        map.insert(ds.keys[i], ds.packed_vals[i]);
        samples[i] = t0.elapsed().as_nanos() as u64;
    }
    let total_ms = start_all.elapsed().as_secs_f64() * 1000.0;
    black_box(map.len());
    samples.sort_unstable();
    let pct = |p: f64| -> u64 {
        let idx = ((p * ((samples.len() - 1) as f64)) as usize).min(samples.len() - 1);
        samples[idx]
    };
    println!(
        "{:<42} | total={:>7.1} ms | p50={:>4} ns | p99={:>5} ns | p99.9={:>6} ns | p99.99={:>7} ns | max={:>10} ns",
        label,
        total_ms,
        pct(0.50),
        pct(0.99),
        pct(0.999),
        pct(0.9999),
        samples[samples.len() - 1]
    );
}

fn run_battle2_tail() {
    let ds = build_dataset(NUM_AGED_ENTRIES, 1024);
    println!("\n=== Rust 1.91 Battle 2B: Streaming Insert Tail Latency (0 -> 2,000,000) ===");
    run_tail_insert("Rust 1.91 HashMap SipHash (Unreserved)", &ds, false);
    run_tail_insert("Rust 1.91 HashMap SipHash (Reserved)", &ds, true);
}

fn run_battle3_hashdos(prime: u64) {
    let normal_ds = build_dataset(NUM_HASHDOS_KEYS, 1024);
    let fx_dos = build_fxhash_collision_keys(NUM_HASHDOS_KEYS);
    let prime_dos: Vec<u64> = (1..=NUM_HASHDOS_KEYS).map(|i| (i as u64) * prime).collect();

    println!(
        "\n=== Rust 1.91 Battle 3: Adversarial HashDoS ({} Keys) ===",
        NUM_HASHDOS_KEYS
    );

    let mut sip_norm = HashMap::with_capacity(NUM_HASHDOS_KEYS);
    let t0 = Instant::now();
    for (i, &k) in normal_ds.keys.iter().enumerate() {
        sip_norm.insert(k, i as u64 + 1);
    }
    let sip_norm_ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sip_norm.len());
    println!(
        "{:<42} | {:>8.2} ms",
        "Rust 1.91 SipHash (Normal Keys)", sip_norm_ms
    );

    let mut sip_prime = HashMap::with_capacity(NUM_HASHDOS_KEYS);
    let t0 = Instant::now();
    for (i, &k) in prime_dos.iter().enumerate() {
        sip_prime.insert(k, i as u64 + 1);
    }
    let sip_prime_ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sip_prime.len());
    println!(
        "{:<42} | {:>8.2} ms",
        "Rust 1.91 SipHash (libstdc++ Prime Keys)", sip_prime_ms
    );

    let mut sip_fxdos = HashMap::with_capacity(NUM_HASHDOS_KEYS);
    let t0 = Instant::now();
    for (i, &k) in fx_dos.iter().enumerate() {
        sip_fxdos.insert(k, i as u64 + 1);
    }
    let sip_fxdos_ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(sip_fxdos.len());
    println!(
        "{:<42} | {:>8.2} ms",
        "Rust 1.91 SipHash (FxHash-Targeted Keys)", sip_fxdos_ms
    );

    let mut fx_map: HashMap<u64, u64, FxBuildHasher> =
        HashMap::with_capacity_and_hasher(NUM_HASHDOS_KEYS, FxBuildHasher::default());
    let t0 = Instant::now();
    for (i, &k) in fx_dos.iter().enumerate() {
        fx_map.insert(k, i as u64 + 1);
    }
    let fx_dos_ms = t0.elapsed().as_secs_f64() * 1000.0;
    black_box(fx_map.len());
    println!(
        "{:<42} | {:>8.2} ms ({:.1}x slower vs SipHash!)",
        "Rust 1.91 FxHash (FxHash-Targeted Keys)",
        fx_dos_ms,
        fx_dos_ms / sip_fxdos_ms
    );
}

fn run_self_tests() {
    let ds = build_dataset(16_384, 32_768);
    let mut sip_map = HashMap::with_capacity(ds.keys.len());
    let mut fx_map: HashMap<u64, u64, FxBuildHasher> =
        HashMap::with_capacity_and_hasher(ds.keys.len(), FxBuildHasher::default());
    for i in 0..ds.keys.len() {
        sip_map.insert(ds.keys[i], ds.packed_vals[i]);
        fx_map.insert(ds.keys[i], ds.packed_vals[i]);
    }
    let c_sip =
        rust_siphash_find_packed(&sip_map, ds.queries_80_20.as_ptr(), ds.queries_80_20.len());
    let c_fx = rust_fxhash_find_packed(&fx_map, ds.queries_80_20.as_ptr(), ds.queries_80_20.len());
    assert_eq!(c_sip, c_fx);
    assert_ne!(c_sip, 0);

    let c_miss = rust_siphash_find_packed(
        &sip_map,
        ds.queries_100_miss.as_ptr(),
        ds.queries_100_miss.len(),
    );
    assert_eq!(c_miss, 0);

    let inv = mod_inverse_u64(FX_SEED);
    assert_eq!(FX_SEED.wrapping_mul(inv), 1u64);
    let fx_dos = build_fxhash_collision_keys(1024);
    for k in fx_dos {
        let mut h = FxHasher::default();
        h.write_u64(k);
        let digest = h.finish();
        assert_eq!(digest & 0xFFFF, 0);
        assert_eq!(digest >> 57, 0);
    }
    println!(
        "Rust 1.91 self-test PASSED: 16K 80/20 checksum=0x{:x}, FxHash H1+H2 collision verified.",
        c_sip
    );
}

fn main() {
    let mut mode = String::from("all");
    let mut iters: usize = 0;
    let mut prime: u64 = 43_067;

    for arg in env::args().skip(1) {
        if let Some(m) = arg.strip_prefix("--mode=") {
            mode = m.to_string();
        } else if let Some(it) = arg.strip_prefix("--iters=") {
            iters = it.parse().unwrap_or(0);
        } else if let Some(p) = arg.strip_prefix("--prime=") {
            prime = p.parse().unwrap_or(43_067);
        }
    }

    match mode.as_str() {
        "test" => run_self_tests(),
        "all" => {
            println!("=== Rust 1.91 Battle 1: L1 (1,024) vs. Aged Post-Churn (2,000,000) ===");
            run_l1_sip(if iters > 0 { iters } else { 100 });
            run_l1_fx(if iters > 0 { iters } else { 100 });
            run_aged_sip(if iters > 0 { iters } else { 10 }, false);
            run_aged_fx(if iters > 0 { iters } else { 10 }, false);
            run_aged_sip(if iters > 0 { iters } else { 10 }, true);
            run_aged_fx(if iters > 0 { iters } else { 10 }, true);
            run_aged_session_sip(if iters > 0 { iters } else { 10 });
            run_aged_session_fx(if iters > 0 { iters } else { 10 });
            run_battle2_rss();
            run_battle2_tail();
            run_battle3_hashdos(prime);
        }
        "b1_l1_sip" => run_l1_sip(if iters > 0 { iters } else { 500 }),
        "b1_l1_fx" => run_l1_fx(if iters > 0 { iters } else { 500 }),
        "b1_aged_sip" => run_aged_sip(if iters > 0 { iters } else { 40 }, false),
        "b1_aged_fx" => run_aged_fx(if iters > 0 { iters } else { 40 }, false),
        "b1_miss_sip" => run_aged_sip(if iters > 0 { iters } else { 40 }, true),
        "b1_miss_fx" => run_aged_fx(if iters > 0 { iters } else { 40 }, true),
        "b1_aged_session_sip" => run_aged_session_sip(if iters > 0 { iters } else { 40 }),
        "b1_aged_session_fx" => run_aged_session_fx(if iters > 0 { iters } else { 40 }),
        "b2_rss" => run_battle2_rss(),
        "b2_tail" => run_battle2_tail(),
        "b3_hashdos" => run_battle3_hashdos(prime),
        other => {
            eprintln!("Unknown --mode={}", other);
            std::process::exit(1);
        }
    }
}
