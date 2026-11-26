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

//! Episode 17: Rust 1.91 full 3-story parity benchmark for `RealtimeGatewayService`.
//!
//! Compile with `cargo --release`-equivalent flags (`-C opt-level=3 -C target-cpu=native`):
//!   - Story 1A: Aged-heap `Vec<SubscriptionRule>` vs. `LinkedList<SubscriptionRule>`
//!     full-table predicate scans (`500` sweeps) + mid-table unsubscribes (`8,192` rules,
//!     `512` unsubs: ordered `remove`, unindexed `swap_remove`, Sparse-Set `HashMap +
//!     swap_remove`, and order-preserving indexed tombstone + `retain`).
//!   - Story 1B: Safe Rust Slab-Index Dual Doubly-Linked List (`u32` `prev`/`next`
//!     hooks inside `Vec<ActiveConnection>`, `0` `unsafe`, `0` node allocs) vs.
//!     `O(1)` indexed `Vec<u32>` `copy_within` shift (zero linear scan!) and
//!     linear `Vec<u32>` locate + `rotate` across `16,384` connections & `200,000` events.
//!   - Story 2A & 2B: `100,000` empty `VecDeque` footprint (`0` per-queue allocs) +
//!     `8,000,000` FIFO messages (single channel & `1,024`-queue gateway mux).
//!   - Story 3: `150,000` slot-indexed `ClientSession` reconnect wave (`Vec<ClientSession>`
//!     unreserved bitwise `memcpy` moves, `Vec<Box<ClientSession>>` heap-pointer control,
//!     and `Vec::with_capacity`), verifying every struct field.

use std::collections::{HashMap, LinkedList, VecDeque};
use std::hint::black_box;
use std::time::Instant;

const K_SEED_RULES: u64 = 0x5441534B17;
const K_SEED_NOISE: u64 = 0x99990017;
const NONE_IDX: u32 = u32::MAX;

#[inline]
fn splitmix64(state: &mut u64) -> u64 {
    *state = state.wrapping_add(0x9e3779b97f4a7c15);
    let mut z = *state;
    z = (z ^ (z >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94d049bb133111eb);
    z ^ (z >> 31)
}

#[inline]
fn mix_order_hash(acc: u64, val: u64) -> u64 {
    acc.rotate_left(7) ^ val.wrapping_add(0x9e3779b97f4a7c15)
}

// =============================================================================
// Story 1A: 64-Byte `SubscriptionRule` Table Sweep & Mid-Table Unsubscribes
// =============================================================================

/// 64-byte cache-line-aligned alert-routing rule matching C++ `SubscriptionRule`.
#[repr(C, align(64))]
#[derive(Clone, Copy)]
struct SubscriptionRule {
    rule_id: u64,
    tenant_id: u64,
    severity_weight: i64,
    expires_at_ms: u64,
    threshold_limit: u32,
    current_value: u32,
    trigger_count: u32,
    active_flag: u32,
    topic_prefix: [u8; 16],
}

#[inline]
fn make_subscription_rule(id: u64, rng: &mut u64) -> SubscriptionRule {
    SubscriptionRule {
        rule_id: id,
        tenant_id: 1000 + (splitmix64(rng) % 4096),
        severity_weight: 100 + (splitmix64(rng) % 5000) as i64,
        expires_at_ms: 1764090000000u64.wrapping_add(id * 10),
        threshold_limit: 100 + (splitmix64(rng) % 900) as u32,
        current_value: (splitmix64(rng) % 50) as u32,
        trigger_count: (id & 3) as u32,
        active_flag: 1,
        topic_prefix: *b"alerts-prod\0\0\0\0\0",
    }
}

#[inline]
fn evaluate_rule_score(r: &SubscriptionRule) -> u64 {
    if r.active_flag == 0 {
        return 0;
    }
    (r.severity_weight as u64).wrapping_mul((r.threshold_limit - r.current_value) as u64)
}

struct RuleTableWorkload {
    initial_rules: Vec<SubscriptionRule>,
    unsubscribe_ids: Vec<u64>,
    replacement_rules: Vec<SubscriptionRule>,
    scan_every: usize,
}

fn build_rule_table_workload(
    rule_count: usize,
    num_unsubs: usize,
    scan_every: usize,
) -> RuleTableWorkload {
    let mut initial_rules = Vec::with_capacity(rule_count);
    let mut unsubscribe_ids = Vec::with_capacity(num_unsubs);
    let mut replacement_rules = Vec::with_capacity(num_unsubs);
    let mut rng = K_SEED_RULES;

    for i in 0..rule_count {
        initial_rules.push(make_subscription_rule((i + 1) as u64, &mut rng));
    }
    let mut live_ids: Vec<u64> = initial_rules.iter().map(|r| r.rule_id).collect();
    let mut next_id = (rule_count + 1) as u64;
    for _ in 0..num_unsubs {
        let lo = rule_count / 5;
        let span = ((rule_count * 3) / 5).max(1);
        let idx = lo + (splitmix64(&mut rng) as usize % span);
        unsubscribe_ids.push(live_ids[idx]);
        let rep = make_subscription_rule(next_id, &mut rng);
        next_id += 1;
        replacement_rules.push(rep);
        live_ids.remove(idx);
        live_ids.push(rep.rule_id);
    }
    RuleTableWorkload {
        initial_rules,
        unsubscribe_ids,
        replacement_rules,
        scan_every: scan_every.max(1),
    }
}

// =============================================================================
// Story 1B: Safe Rust 256B `ActiveConnection` With Slab-Index Dual Hooks
// =============================================================================

/// 256-byte pinned connection record carrying embedded `u32` slab-index dual hooks.
#[repr(C, align(64))]
struct ActiveConnection {
    conn_id: u64,
    last_seen_ms: u64,
    bytes_rx: u64,
    bytes_tx: u64,
    fd: u32,
    tls_state: u32,
    socket_metrics: [u64; 12],
    peer_host: [u8; 104],
    lru_prev: u32,
    lru_next: u32,
    to_prev: u32,
    to_next: u32,
}

struct SlabDualLists {
    lru_head: u32,
    lru_tail: u32,
    to_head: u32,
    to_tail: u32,
}

impl SlabDualLists {
    fn new() -> Self {
        Self {
            lru_head: NONE_IDX,
            lru_tail: NONE_IDX,
            to_head: NONE_IDX,
            to_tail: NONE_IDX,
        }
    }

    #[inline]
    fn push_back_lru(&mut self, slab: &mut [ActiveConnection], idx: u32) {
        let prev = self.lru_tail;
        let c = &mut slab[idx as usize];
        c.lru_prev = prev;
        c.lru_next = NONE_IDX;
        if prev != NONE_IDX {
            slab[prev as usize].lru_next = idx;
        } else {
            self.lru_head = idx;
        }
        self.lru_tail = idx;
    }

    #[inline]
    fn push_front_lru(&mut self, slab: &mut [ActiveConnection], idx: u32) {
        let next = self.lru_head;
        let c = &mut slab[idx as usize];
        c.lru_prev = NONE_IDX;
        c.lru_next = next;
        if next != NONE_IDX {
            slab[next as usize].lru_prev = idx;
        } else {
            self.lru_tail = idx;
        }
        self.lru_head = idx;
    }

    #[inline]
    fn unlink_lru(&mut self, slab: &mut [ActiveConnection], idx: u32) {
        let (prev, next) = {
            let c = &slab[idx as usize];
            (c.lru_prev, c.lru_next)
        };
        if prev != NONE_IDX {
            slab[prev as usize].lru_next = next;
        } else {
            self.lru_head = next;
        }
        if next != NONE_IDX {
            slab[next as usize].lru_prev = prev;
        } else {
            self.lru_tail = prev;
        }
    }

    #[inline]
    fn push_back_timeout(&mut self, slab: &mut [ActiveConnection], idx: u32) {
        let prev = self.to_tail;
        let c = &mut slab[idx as usize];
        c.to_prev = prev;
        c.to_next = NONE_IDX;
        if prev != NONE_IDX {
            slab[prev as usize].to_next = idx;
        } else {
            self.to_head = idx;
        }
        self.to_tail = idx;
    }

    #[inline]
    fn unlink_timeout(&mut self, slab: &mut [ActiveConnection], idx: u32) {
        let (prev, next) = {
            let c = &slab[idx as usize];
            (c.to_prev, c.to_next)
        };
        if prev != NONE_IDX {
            slab[prev as usize].to_next = next;
        } else {
            self.to_head = next;
        }
        if next != NONE_IDX {
            slab[next as usize].to_prev = prev;
        } else {
            self.to_tail = prev;
        }
    }
}

fn build_dual_queue_events(num_connections: usize, num_events: usize) -> Vec<u32> {
    let mut events = Vec::with_capacity(num_events);
    let hot_band = (num_connections / 5).max(1) as u64;
    let mut rng = K_SEED_RULES ^ 0xBEEF;
    for _ in 0..num_events {
        let r = splitmix64(&mut rng);
        let pick = splitmix64(&mut rng);
        let idx = if (r % 100) < 80 {
            pick % hot_band
        } else {
            pick % (num_connections as u64)
        };
        events.push(idx as u32);
    }
    events
}

fn make_connection_slab(num_connections: usize) -> Vec<ActiveConnection> {
    (0..num_connections)
        .map(|i| ActiveConnection {
            conn_id: (i + 1) as u64,
            last_seen_ms: 0,
            bytes_rx: 0,
            bytes_tx: 0,
            fd: (i + 10) as u32,
            tls_state: 0,
            socket_metrics: [0; 12],
            peer_host: [0; 104],
            lru_prev: NONE_IDX,
            lru_next: NONE_IDX,
            to_prev: NONE_IDX,
            to_next: NONE_IDX,
        })
        .collect()
}

fn run_dual_queue_slab_intrusive(num_connections: usize, events: &[u32]) -> u64 {
    let mut slab = make_connection_slab(num_connections);
    let mut lists = SlabDualLists::new();
    for i in 0..num_connections as u32 {
        lists.push_back_lru(&mut slab, i);
        lists.push_back_timeout(&mut slab, i);
    }

    let mut ts = 1u64;
    for &idx in events {
        {
            let conn = &mut slab[idx as usize];
            conn.last_seen_ms = ts;
            conn.bytes_rx += 64;
        }
        ts += 1;
        lists.unlink_lru(&mut slab, idx);
        lists.push_front_lru(&mut slab, idx);
        lists.unlink_timeout(&mut slab, idx);
        lists.push_back_timeout(&mut slab, idx);
    }

    slab[lists.lru_head as usize].conn_id
        ^ (slab[lists.lru_tail as usize].conn_id << 16)
        ^ (slab[lists.to_head as usize].conn_id << 32)
        ^ slab[lists.to_tail as usize].last_seen_ms
}

fn run_dual_queue_vec_indexed_shift(num_connections: usize, events: &[u32]) -> u64 {
    let mut slab = make_connection_slab(num_connections);
    let mut lru: Vec<u32> = (0..num_connections as u32).collect();
    let mut timeouts: Vec<u32> = (0..num_connections as u32).collect();
    let mut lru_pos: Vec<u32> = (0..num_connections as u32).collect();
    let mut to_pos: Vec<u32> = (0..num_connections as u32).collect();
    let last = num_connections - 1;

    let mut ts = 1u64;
    for &idx in events {
        slab[idx as usize].last_seen_ms = ts;
        slab[idx as usize].bytes_rx += 64;
        ts += 1;

        let lp = lru_pos[idx as usize] as usize;
        if lp > 0 {
            lru.copy_within(0..lp, 1);
            lru[0] = idx;
            for k in 1..=lp {
                lru_pos[lru[k] as usize] = k as u32;
            }
            lru_pos[idx as usize] = 0;
        }

        let tp = to_pos[idx as usize] as usize;
        if tp < last {
            timeouts.copy_within((tp + 1)..=last, tp);
            timeouts[last] = idx;
            for k in tp..last {
                to_pos[timeouts[k] as usize] = k as u32;
            }
            to_pos[idx as usize] = last as u32;
        }
    }

    slab[lru[0] as usize].conn_id
        ^ (slab[lru[last] as usize].conn_id << 16)
        ^ (slab[timeouts[0] as usize].conn_id << 32)
        ^ slab[timeouts[last] as usize].last_seen_ms
}

fn run_dual_queue_vec_indices(num_connections: usize, events: &[u32]) -> u64 {
    let mut slab = make_connection_slab(num_connections);
    let mut lru: Vec<u32> = (0..num_connections as u32).collect();
    let mut timeouts: Vec<u32> = (0..num_connections as u32).collect();

    let mut ts = 1u64;
    for &idx in events {
        slab[idx as usize].last_seen_ms = ts;
        slab[idx as usize].bytes_rx += 64;
        ts += 1;

        let lru_pos = lru.iter().position(|&x| x == idx).unwrap();
        lru[..=lru_pos].rotate_right(1);

        let to_pos = timeouts.iter().position(|&x| x == idx).unwrap();
        timeouts[to_pos..].rotate_left(1);
    }

    slab[*lru.first().unwrap() as usize].conn_id
        ^ (slab[*lru.last().unwrap() as usize].conn_id << 16)
        ^ (slab[*timeouts.first().unwrap() as usize].conn_id << 32)
        ^ slab[*timeouts.last().unwrap() as usize].last_seen_ms
}

// =============================================================================
// Story 2: 64B `OutboundMessage` & `VecDeque` Ring Buffer
// =============================================================================

/// 64-byte outbound RPC message matching C++ `OutboundMessage`.
#[repr(C, align(64))]
#[derive(Clone, Copy)]
struct OutboundMessage {
    message_id: u64,
    enqueued_ms: u64,
    channel_id: u32,
    payload_bytes: u32,
    priority: u32,
    flags: u32,
    routing_tag: [u8; 32],
}

#[inline]
fn make_outbound_message(seq: u64) -> OutboundMessage {
    let mut tag = [0u8; 32];
    tag[0] = (seq & 0xFF) as u8;
    OutboundMessage {
        message_id: seq,
        enqueued_ms: 1764095000000u64.wrapping_add(seq * 2),
        channel_id: ((seq % 256) * 2 + 1) as u32,
        payload_bytes: (64 + (seq & 0x3FF)) as u32,
        priority: (seq & 7) as u32,
        flags: 0x4,
        routing_tag: tag,
    }
}

#[inline]
fn digest_message(m: &OutboundMessage) -> u64 {
    m.message_id
        ^ m.enqueued_ms
        ^ ((m.channel_id as u64) << 32)
        ^ ((m.payload_bytes + m.priority) as u64)
        ^ (m.routing_tag[0] as u64)
}

// =============================================================================
// Story 3: Slot-Indexed `ClientSession` (`150,000` Reconnect Wave)
// =============================================================================

/// Slot-indexed client session matching C++ `SessionNoexceptRingQueue`.
struct ClientSession {
    session_id: u64,
    account_id: u32,
    flags: u32,
    endpoint_url: String,
    read_buffer: Vec<u8>,
    pending_message_ids: VecDeque<u64>,
}

#[inline]
fn make_session(id: u64) -> ClientSession {
    let mut msgs = VecDeque::with_capacity(8);
    for msg in 0..4u64 {
        msgs.push_back(id * 16 + msg);
    }
    let mut url = String::with_capacity(51);
    url.push_str("https://gateway.service.internal/v2/sessions/slot-");
    url.push((b'0' + (id % 10) as u8) as char);
    ClientSession {
        session_id: id,
        account_id: ((id * 31) & 0xFFFF) as u32,
        flags: 1,
        endpoint_url: url,
        read_buffer: vec![(id & 0xFF) as u8; 256],
        pending_message_ids: msgs,
    }
}

#[inline]
fn digest_session(s: &ClientSession) -> u64 {
    let url_len = s.endpoint_url.len() as u64;
    let url_tail = *s.endpoint_url.as_bytes().last().unwrap() as u64;
    let first_msg = *s.pending_message_ids.front().unwrap();
    let buf_byte = s.read_buffer[0] as u64;
    s.session_id
        ^ ((s.account_id as u64) << 16)
        ^ ((s.flags as u64) << 40)
        ^ (url_len << 32)
        ^ (url_tail << 8)
        ^ (buf_byte << 24)
        ^ first_msg
        ^ ((s.read_buffer.len() + s.pending_message_ids.len()) as u64)
}

fn measure_median_ms<F: FnMut() -> u64>(iters: usize, mut f: F) -> (f64, u64) {
    let mut last = black_box(f());
    let mut samples = Vec::with_capacity(iters);
    for _ in 0..iters {
        let t0 = Instant::now();
        last = black_box(f());
        samples.push(t0.elapsed().as_secs_f64() * 1000.0);
    }
    samples.sort_by(|a, b| a.total_cmp(b));
    (samples[samples.len() / 2], last)
}

fn measure_median_with_state<T, S: FnMut(&mut T), F: FnMut(&mut T) -> u64>(
    iters: usize,
    state: &mut T,
    mut setup: S,
    mut f: F,
) -> (f64, u64) {
    setup(state);
    let mut last = black_box(f(state));
    let mut samples = Vec::with_capacity(iters);
    for _ in 0..iters {
        setup(state);
        let t0 = Instant::now();
        last = black_box(f(state));
        samples.push(t0.elapsed().as_secs_f64() * 1000.0);
    }
    samples.sort_by(|a, b| a.total_cmp(b));
    (samples[samples.len() / 2], last)
}

fn main() {
    assert_eq!(std::mem::size_of::<SubscriptionRule>(), 64);
    assert_eq!(std::mem::size_of::<ActiveConnection>(), 256);
    assert_eq!(std::mem::size_of::<OutboundMessage>(), 64);
    let story = std::env::args()
        .find_map(|a| a.strip_prefix("--story=").map(str::to_owned))
        .unwrap_or_else(|| "all".to_owned());
    let iters = 5;

    if story == "all" || story == "1a" {
        println!(
            "=== RUST 1.91 STORY 1A: Aged-Heap Subscription Rule Table Scan & Mid-Table Unsub ==="
        );
        for &n in &[1_024usize, 4_096, 16_384] {
            let mut rng_noise = K_SEED_NOISE ^ (n as u64);
            let mut churn: Vec<Option<Vec<u8>>> = Vec::with_capacity(24_000);
            for i in 0..24_000usize {
                let sz = 32 + (splitmix64(&mut rng_noise) % 512) as usize;
                churn.push(Some(vec![(i & 0xFF) as u8; sz]));
            }
            for i in (0..churn.len()).step_by(2) {
                churn[i] = None;
            }

            let mut vec_rules = Vec::with_capacity(n);
            let mut list_rules = LinkedList::new();
            let mut noise = Vec::with_capacity(n);
            let mut rng = K_SEED_RULES;
            for i in 0..n {
                let r = make_subscription_rule((i + 1) as u64, &mut rng);
                vec_rules.push(r);
                list_rules.push_back(r);
                let nsz = 48 + (splitmix64(&mut rng_noise) % 256) as usize;
                noise.push(vec![1u8; nsz]);
            }
            drop(noise);

            let reps = 500;
            let (vec_ms, vec_cs) = measure_median_ms(iters, || {
                let mut sum = 0u64;
                for _ in 0..reps {
                    for r in &vec_rules {
                        sum = sum.wrapping_add(evaluate_rule_score(r));
                    }
                }
                sum
            });
            let (list_ms, list_cs) = measure_median_ms(iters, || {
                let mut sum = 0u64;
                for _ in 0..reps {
                    for r in &list_rules {
                        sum = sum.wrapping_add(evaluate_rule_score(r));
                    }
                }
                sum
            });
            let total_evals = (reps * n) as f64;
            println!(
                "  [Aged Scan x{}, N={:5}] Vec={:7.3} ms ({:6.1} M/s) | LinkedList={:7.3} ms ({:5.1} M/s, {:.2}x slower) | parity={}",
                reps,
                n,
                vec_ms,
                (total_evals / (vec_ms / 1000.0)) / 1e6,
                list_ms,
                (total_evals / (list_ms / 1000.0)) / 1e6,
                list_ms / vec_ms,
                if vec_cs == list_cs { "OK" } else { "MISMATCH" }
            );
            black_box(&churn);
        }

        let wl = build_rule_table_workload(8_192, 512, 8);
        let mut work_vec = Vec::with_capacity(8_704);
        let (vo_ms, vo_cs) = measure_median_with_state(
            iters,
            &mut work_vec,
            |v| {
                v.clear();
                v.extend_from_slice(&wl.initial_rules);
            },
            |v| {
                let mut total = 0u64;
                for (c, &target) in wl.unsubscribe_ids.iter().enumerate() {
                    if c % wl.scan_every == 0 {
                        for r in v.iter() {
                            total = total.wrapping_add(evaluate_rule_score(r));
                        }
                    }
                    if let Some(pos) = v.iter().position(|r| r.rule_id == target) {
                        v.remove(pos);
                    }
                    v.push(wl.replacement_rules[c]);
                }
                for r in v.iter() {
                    total = total.wrapping_add(evaluate_rule_score(r));
                }
                total
            },
        );
        let (vs_ms, vs_cs) = measure_median_with_state(
            iters,
            &mut work_vec,
            |v| {
                v.clear();
                v.extend_from_slice(&wl.initial_rules);
            },
            |v| {
                let mut total = 0u64;
                for (c, &target) in wl.unsubscribe_ids.iter().enumerate() {
                    if c % wl.scan_every == 0 {
                        for r in v.iter() {
                            total = total.wrapping_add(evaluate_rule_score(r));
                        }
                    }
                    if let Some(pos) = v.iter().position(|r| r.rule_id == target) {
                        v.swap_remove(pos);
                    }
                    v.push(wl.replacement_rules[c]);
                }
                for r in v.iter() {
                    total = total.wrapping_add(evaluate_rule_score(r));
                }
                total
            },
        );
        let mut sparse_state: (Vec<SubscriptionRule>, HashMap<u64, usize>) =
            (Vec::with_capacity(8_704), HashMap::with_capacity(16_384));
        let (vsp_ms, vsp_cs) = measure_median_with_state(
            iters,
            &mut sparse_state,
            |(v, pos_map)| {
                v.clear();
                v.extend_from_slice(&wl.initial_rules);
                pos_map.clear();
                for (i, r) in v.iter().enumerate() {
                    pos_map.insert(r.rule_id, i);
                }
            },
            |(v, pos_map)| {
                let mut total = 0u64;
                for (c, &target) in wl.unsubscribe_ids.iter().enumerate() {
                    if c % wl.scan_every == 0 {
                        for r in v.iter() {
                            total = total.wrapping_add(evaluate_rule_score(r));
                        }
                    }
                    if let Some(slot) = pos_map.remove(&target) {
                        let last = v.len() - 1;
                        v.swap_remove(slot);
                        if slot != last {
                            pos_map.insert(v[slot].rule_id, slot);
                        }
                    }
                    pos_map.insert(wl.replacement_rules[c].rule_id, v.len());
                    v.push(wl.replacement_rules[c]);
                }
                for r in v.iter() {
                    total = total.wrapping_add(evaluate_rule_score(r));
                }
                total
            },
        );
        let mut tomb_state: (Vec<SubscriptionRule>, HashMap<u64, usize>) =
            (Vec::with_capacity(8_704), HashMap::with_capacity(16_384));
        let (vt_ms, vt_cs) = measure_median_with_state(
            iters,
            &mut tomb_state,
            |(v, pos_map)| {
                v.clear();
                v.extend_from_slice(&wl.initial_rules);
                pos_map.clear();
                for (i, r) in v.iter().enumerate() {
                    pos_map.insert(r.rule_id, i);
                }
            },
            |(v, pos_map)| {
                let mut total = 0u64;
                let mut dead = 0usize;
                for (c, &target) in wl.unsubscribe_ids.iter().enumerate() {
                    if c % wl.scan_every == 0 {
                        for r in v.iter() {
                            total = total.wrapping_add(evaluate_rule_score(r));
                        }
                    }
                    if let Some(idx) = pos_map.remove(&target) {
                        v[idx].active_flag = 0;
                        dead += 1;
                    }
                    pos_map.insert(wl.replacement_rules[c].rule_id, v.len());
                    v.push(wl.replacement_rules[c]);
                    if dead >= 128 {
                        v.retain(|r| r.active_flag != 0);
                        pos_map.clear();
                        for (i, r) in v.iter().enumerate() {
                            pos_map.insert(r.rule_id, i);
                        }
                        dead = 0;
                    }
                }
                v.retain(|r| r.active_flag != 0);
                for r in v.iter() {
                    total = total.wrapping_add(evaluate_rule_score(r));
                }
                total
            },
        );
        println!(
            "  [N=8192, 512 Unsubs + 65 Sweeps] remove={:7.3} ms | swap_rm={:7.3} ms | sparse_set={:7.3} ms | tomb+retain={:7.3} ms | cs={:016x} (parity={})",
            vo_ms,
            vs_ms,
            vsp_ms,
            vt_ms,
            vsp_cs,
            if vo_cs == vs_cs && vs_cs == vsp_cs && vsp_cs == vt_cs {
                "OK"
            } else {
                "MISMATCH"
            }
        );
    }

    if story == "all" || story == "1b" {
        println!(
            "=== RUST 1.91 STORY 1B: Safe Slab-Index Dual Lists vs Vec Shift (16,384 Conns, 200K Events) ==="
        );
        let events = build_dual_queue_events(16_384, 200_000);
        let (slab_ms, slab_cs) =
            measure_median_ms(iters, || run_dual_queue_slab_intrusive(16_384, &events));
        let (vidx_ms, vidx_cs) =
            measure_median_ms(3, || run_dual_queue_vec_indexed_shift(16_384, &events));
        let (vrot_ms, vrot_cs) =
            measure_median_ms(3, || run_dual_queue_vec_indices(16_384, &events));
        println!(
            "  Safe Slab-Index Dual List (0 unsafe)    | median={:7.3} ms | {:6.2} M ev/s | cs={:016x}",
            slab_ms,
            (200_000.0 / (slab_ms / 1000.0)) / 1e6,
            slab_cs
        );
        println!(
            "  Vec<u32> O(1) Idx + copy_within Shift   | median={:7.3} ms | {:6.2} M ev/s ({:.2}x slower) | cs={:016x}",
            vidx_ms,
            (200_000.0 / (vidx_ms / 1000.0)) / 1e6,
            vidx_ms / slab_ms,
            vidx_cs
        );
        println!(
            "  Vec<u32> Linear Position + Rotate       | median={:7.3} ms | {:6.2} M ev/s ({:.2}x slower) | cs={:016x}",
            vrot_ms,
            (200_000.0 / (vrot_ms / 1000.0)) / 1e6,
            vrot_ms / slab_ms,
            vrot_cs
        );
        assert_eq!(slab_cs, vidx_cs);
        assert_eq!(slab_cs, vrot_cs);
    }

    if story == "all" || story == "2" || story == "2a" || story == "2b" {
        println!(
            "=== RUST 1.91 STORY 2A & 2B: Empty VecDeque Footprint & 8,000,000 FIFO Stream ==="
        );
        let empty_queues: Vec<VecDeque<OutboundMessage>> =
            (0..100_000).map(|_| VecDeque::new()).collect();
        let total_cap: usize = empty_queues.iter().map(|q| q.capacity()).sum();
        println!(
            "  100,000 Empty VecDeque<OutboundMessage> | sizeof={}B | sum(capacity)={} (0 per-queue allocs!)",
            std::mem::size_of::<VecDeque<OutboundMessage>>(),
            total_cap
        );
        drop(empty_queues);

        let cycles = 8_000_000u64;
        let (vd_ms, vd_cs) = measure_median_ms(iters, || {
            let mut q = VecDeque::with_capacity(1_024);
            for i in 0..1_024u64 {
                q.push_back(make_outbound_message(i));
            }
            let mut cs = 0x17F1F0u64;
            for i in 0..cycles {
                let m = q.pop_front().unwrap();
                cs = mix_order_hash(cs, digest_message(&m));
                q.push_back(make_outbound_message(1_024 + i));
            }
            for m in &q {
                cs = mix_order_hash(cs, digest_message(m));
            }
            cs
        });
        let (mux_ms, mux_cs) = measure_median_ms(iters, || {
            let mut queues: Vec<VecDeque<OutboundMessage>> =
                (0..1_024).map(|_| VecDeque::with_capacity(16)).collect();
            let mut seq = 0u64;
            for q in &mut queues {
                for _ in 0..16 {
                    q.push_back(make_outbound_message(seq));
                    seq += 1;
                }
            }
            let mut cs = 0x17F1F0u64;
            for i in 0..cycles as usize {
                let q = &mut queues[(i * 73) & 1023];
                let m = q.pop_front().unwrap();
                cs = mix_order_hash(cs, digest_message(&m));
                q.push_back(make_outbound_message(seq));
                seq += 1;
            }
            for q in &queues {
                for m in q {
                    cs = mix_order_hash(cs, digest_message(m));
                }
            }
            cs
        });
        println!(
            "  Single Queue: VecDeque (8M msgs)        | median={:7.3} ms | {:6.2} M msgs/s | cs={:016x}",
            vd_ms,
            (cycles as f64 / (vd_ms / 1000.0)) / 1e6,
            vd_cs
        );
        println!(
            "  1,024 Queues: VecDeque (8M msgs)        | median={:7.3} ms | {:6.2} M msgs/s | cs={:016x}",
            mux_ms,
            (cycles as f64 / (mux_ms / 1000.0)) / 1e6,
            mux_cs
        );
    }

    if story == "all" || story == "3" {
        println!(
            "=== RUST 1.91 STORY 3: Gateway Reconnect Wave — 150,000 Slot-Indexed ClientSessions ==="
        );
        let num_sessions = 150_000u64;
        let mut unres_cap = 0usize;
        let (unres_ms, unres_cs) = measure_median_ms(iters, || {
            let mut sessions: Vec<ClientSession> = Vec::new();
            for i in 0..num_sessions {
                sessions.push(make_session(i + 1));
            }
            unres_cap = sessions.capacity();
            let mut cs = 0x5E5510u64;
            for s in &sessions {
                cs = mix_order_hash(cs, digest_session(s));
            }
            cs
        });
        let mut box_cap = 0usize;
        let (box_ms, box_cs) = measure_median_ms(iters, || {
            let mut sessions: Vec<Box<ClientSession>> = Vec::new();
            for i in 0..num_sessions {
                sessions.push(Box::new(make_session(i + 1)));
            }
            box_cap = sessions.capacity();
            let mut cs = 0x5E5510u64;
            for s in &sessions {
                cs = mix_order_hash(cs, digest_session(s));
            }
            cs
        });
        let mut res_cap = 0usize;
        let (res_ms, res_cs) = measure_median_ms(iters, || {
            let mut sessions: Vec<ClientSession> = Vec::with_capacity(num_sessions as usize);
            for i in 0..num_sessions {
                sessions.push(make_session(i + 1));
            }
            res_cap = sessions.capacity();
            let mut cs = 0x5E5510u64;
            for s in &sessions {
                cs = mix_order_hash(cs, digest_session(s));
            }
            cs
        });
        println!(
            "  Vec<ClientSession> (unreserved 2.0x)    | median={:7.3} ms | {:5.2} M/s | cap={:6} | cs={:016x}",
            unres_ms,
            (num_sessions as f64 / (unres_ms / 1000.0)) / 1e6,
            unres_cap,
            unres_cs
        );
        println!(
            "  Vec<Box<ClientSession>> (unreserved)    | median={:7.3} ms | {:5.2} M/s | cap={:6} | cs={:016x}",
            box_ms,
            (num_sessions as f64 / (box_ms / 1000.0)) / 1e6,
            box_cap,
            box_cs
        );
        println!(
            "  Vec<ClientSession> (with_capacity)      | median={:7.3} ms | {:5.2} M/s | cap={:6} | cs={:016x}",
            res_ms,
            (num_sessions as f64 / (res_ms / 1000.0)) / 1e6,
            res_cap,
            res_cs
        );
        assert_eq!(unres_cs, box_cs);
        assert_eq!(unres_cs, res_cs);
    }
}
