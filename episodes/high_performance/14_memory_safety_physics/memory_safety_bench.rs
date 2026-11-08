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

//! Episode 14 Rust 1.91 Spatial & Temporal Memory Safety Benchmark Suite.
//!
//! Compile & Run:
//!   rustc --edition=2021 -C opt-level=3 -C target-cpu=native \
//!     memory_safety_bench.rs -o /tmp/ep14_build/bench_rust

use std::hint::black_box;
use std::rc::{Rc, Weak};
use std::time::Instant;

pub const DEFAULT_BOOK_CAPACITY: usize = 65_536;
pub const DEFAULT_BOOK_MASK: u32 = (DEFAULT_BOOK_CAPACITY - 1) as u32;
pub const ORDER_FLAG_ACTIVE: u32 = 0x0000_0001;
pub const ORDER_FLAG_CANCELED: u32 = 0xDEAD_0000;

#[repr(C, align(32))]
#[derive(Clone, Copy, Debug)]
pub struct Order {
    pub order_id: u64,
    pub price_ticks: i64,
    pub quantity: u32,
    pub symbol_id: u32,
    pub flags: u32,
    pub reserved_pad: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OrderHandle {
    pub index: u32,
    pub generation: u32,
}

#[repr(C, align(64))]
#[derive(Clone, Copy)]
pub struct OrderSlabSlot {
    pub order: Order,
    pub generation: u32,
    pub next_free: u32,
    pub occupied: u8,
    pub pad: [u8; 23],
}

pub struct OrderSlabPool {
    slots: Vec<OrderSlabSlot>,
    free_head: u32,
}

impl OrderSlabPool {
    pub const INVALID_INDEX: u32 = u32::MAX;

    pub fn new(capacity: usize) -> Self {
        let mut slots = Vec::with_capacity(capacity);
        for i in 0..capacity {
            slots.push(OrderSlabSlot {
                order: Order {
                    order_id: 0,
                    price_ticks: 0,
                    quantity: 0,
                    symbol_id: 0,
                    flags: 0,
                    reserved_pad: 0,
                },
                generation: 1,
                next_free: if i + 1 < capacity {
                    (i + 1) as u32
                } else {
                    Self::INVALID_INDEX
                },
                occupied: 0,
                pad: [0u8; 23],
            });
        }
        Self {
            slots,
            free_head: 0,
        }
    }

    #[inline]
    pub fn allocate(&mut self, initial: Order) -> Option<OrderHandle> {
        if self.free_head == Self::INVALID_INDEX {
            return None;
        }
        let idx = self.free_head;
        let slot = &mut self.slots[idx as usize];
        self.free_head = slot.next_free;
        slot.next_free = Self::INVALID_INDEX;
        slot.occupied = 1;
        slot.order = initial;
        Some(OrderHandle {
            index: idx,
            generation: slot.generation,
        })
    }

    #[inline]
    pub fn release(&mut self, handle: OrderHandle) -> bool {
        let Some(slot) = self.slots.get_mut(handle.index as usize) else {
            return false;
        };
        if slot.occupied == 0 || slot.generation != handle.generation {
            return false;
        }
        slot.occupied = 0;
        slot.order.flags = ORDER_FLAG_CANCELED;
        slot.generation = if slot.generation.wrapping_add(1) == 0 {
            1
        } else {
            slot.generation.wrapping_add(1)
        };
        slot.next_free = self.free_head;
        self.free_head = handle.index;
        true
    }

    #[inline]
    pub fn get(&self, handle: OrderHandle) -> Option<&Order> {
        let slot = self.slots.get(handle.index as usize)?;
        if slot.occupied == 0 || slot.generation != handle.generation {
            return None;
        }
        Some(&slot.order)
    }

    #[inline]
    pub unsafe fn get_unchecked_slot(&self, index: u32) -> &Order {
        &unsafe { self.slots.get_unchecked(index as usize) }.order
    }
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_scan_sequential_iter(orders: *const Order, len: usize) -> i64 {
    let slice = unsafe { std::slice::from_raw_parts(orders, len) };
    let mut total: i64 = 0;
    for o in slice.iter() {
        total = total.wrapping_add(o.price_ticks.wrapping_mul(o.quantity as i64));
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_scan_sequential_indexed(orders: *const Order, len: usize) -> i64 {
    let slice = unsafe { std::slice::from_raw_parts(orders, len) };
    let mut total: i64 = 0;
    for i in 0..slice.len() {
        let o = &slice[i];
        total = total.wrapping_add(o.price_ticks.wrapping_mul(o.quantity as i64));
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_gather_batch_unchecked(
    orders: *const Order,
    orders_len: usize,
    indices: *const u32,
    idx_len: usize,
) -> i64 {
    let orders_slice = unsafe { std::slice::from_raw_parts(orders, orders_len) };
    let idx_slice = unsafe { std::slice::from_raw_parts(indices, idx_len) };
    let mut total: i64 = 0;
    for &idx in idx_slice {
        let o = unsafe { orders_slice.get_unchecked(idx as usize) };
        total = total.wrapping_add(o.price_ticks.wrapping_mul(o.quantity as i64));
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_gather_batch_checked(
    orders: *const Order,
    orders_len: usize,
    indices: *const u32,
    idx_len: usize,
) -> i64 {
    let orders_slice = unsafe { std::slice::from_raw_parts(orders, orders_len) };
    let idx_slice = unsafe { std::slice::from_raw_parts(indices, idx_len) };
    let mut total: i64 = 0;
    for &idx in idx_slice {
        let o = &orders_slice[idx as usize];
        total = total.wrapping_add(o.price_ticks.wrapping_mul(o.quantity as i64));
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_gather_batch_option_get(
    orders: *const Order,
    orders_len: usize,
    indices: *const u32,
    idx_len: usize,
) -> i64 {
    let orders_slice = unsafe { std::slice::from_raw_parts(orders, orders_len) };
    let idx_slice = unsafe { std::slice::from_raw_parts(indices, idx_len) };
    let mut total: i64 = 0;
    for &idx in idx_slice {
        if let Some(o) = orders_slice.get(idx as usize) {
            total = total.wrapping_add(o.price_ticks.wrapping_mul(o.quantity as i64));
        }
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_gather_batch_bitmask_safe(
    orders: *const Order,
    orders_len: usize,
    indices: *const u32,
    idx_len: usize,
) -> i64 {
    let orders_slice = unsafe { std::slice::from_raw_parts(orders, orders_len) };
    let idx_slice = unsafe { std::slice::from_raw_parts(indices, idx_len) };
    assert!(orders_slice.len() >= DEFAULT_BOOK_CAPACITY);
    let active_prefix = &orders_slice[..DEFAULT_BOOK_CAPACITY];
    let mut total: i64 = 0;
    for &idx in idx_slice {
        let safe_idx = (idx & DEFAULT_BOOK_MASK) as usize;
        let o = &active_prefix[safe_idx];
        total = total.wrapping_add(o.price_ticks.wrapping_mul(o.quantity as i64));
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_probe_dependent_unchecked(
    orders: *const Order,
    orders_len: usize,
    indices: *const u32,
    idx_len: usize,
) -> i64 {
    let orders_slice = unsafe { std::slice::from_raw_parts(orders, orders_len) };
    let idx_slice = unsafe { std::slice::from_raw_parts(indices, idx_len) };
    let mut total: i64 = 0;
    let mut prev_mix: u32 = 0;
    for &base_idx in idx_slice {
        let idx = ((base_idx ^ prev_mix) & DEFAULT_BOOK_MASK) as usize;
        let o = unsafe { orders_slice.get_unchecked(idx) };
        let notional = o.price_ticks.wrapping_mul(o.quantity as i64);
        total = total.wrapping_add(notional);
        prev_mix = (notional as u32) & 0xFF;
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_probe_dependent_checked(
    orders: *const Order,
    orders_len: usize,
    indices: *const u32,
    idx_len: usize,
) -> i64 {
    let orders_slice = unsafe { std::slice::from_raw_parts(orders, orders_len) };
    let idx_slice = unsafe { std::slice::from_raw_parts(indices, idx_len) };
    let mut total: i64 = 0;
    let mut prev_mix: u32 = 0;
    for &base_idx in idx_slice {
        let idx = ((base_idx ^ prev_mix) & DEFAULT_BOOK_MASK) as usize;
        let o = &orders_slice[idx];
        let notional = o.price_ticks.wrapping_mul(o.quantity as i64);
        total = total.wrapping_add(notional);
        prev_mix = (notional as u32) & 0xFF;
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_temporal_unchecked_lookup(
    pool: &OrderSlabPool,
    handles: &[OrderHandle],
    lookup_order: &[u32],
) -> i64 {
    let mut total: i64 = 0;
    for &pos in lookup_order {
        let h = unsafe { *handles.get_unchecked(pos as usize) };
        let order = unsafe { pool.get_unchecked_slot(h.index) };
        if order.flags == ORDER_FLAG_ACTIVE {
            total = total.wrapping_add(order.price_ticks.wrapping_mul(order.quantity as i64));
        }
    }
    total
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn rust_temporal_generational_lookup(
    pool: &OrderSlabPool,
    handles: &[OrderHandle],
    lookup_order: &[u32],
) -> i64 {
    let mut total: i64 = 0;
    for &pos in lookup_order {
        let h = handles[pos as usize];
        if let Some(order) = pool.get(h) {
            total = total.wrapping_add(order.price_ticks.wrapping_mul(order.quantity as i64));
        }
    }
    total
}

#[inline(never)]
pub fn rust_temporal_rc_weak_lookup(
    weak_handles: &[Weak<Order>],
    lookup_order: &[u32],
) -> i64 {
    let mut total: i64 = 0;
    for &pos in lookup_order {
        if let Some(rc_order) = weak_handles[pos as usize].upgrade() {
            total = total.wrapping_add(rc_order.price_ticks.wrapping_mul(rc_order.quantity as i64));
        }
    }
    total
}

fn print_row(label: &str, elapsed_ns: f64, iters: usize, count: usize, checksum: i64) {
    let us_per_pass = (elapsed_ns / (iters as f64)) / 1000.0;
    let ns_per_op = elapsed_ns / ((iters * count) as f64);
    println!(
        "  {:<46} | {:8.2} us/pass | {:6.3} ns/order | sum={}",
        label, us_per_pass, ns_per_op, checksum
    );
}

fn main() {
    let mut mode = String::from("all");
    let mut variant = String::from("all");
    let mut iters: usize = 2000;

    for arg in std::env::args().skip(1) {
        if let Some(val) = arg.strip_prefix("--mode=") {
            mode = val.to_string();
        } else if let Some(val) = arg.strip_prefix("--variant=") {
            variant = val.to_string();
        } else if let Some(val) = arg.strip_prefix("--iters=") {
            iters = val.parse().unwrap_or(2000);
        }
    }

    let count = DEFAULT_BOOK_CAPACITY;
    let mut orders = Vec::with_capacity(count);
    let mut random_idx = Vec::with_capacity(count);
    let mut state: u64 = 0xCAFE_BABE_1337_BEEF;

    for i in 0..count {
        orders.push(Order {
            order_id: (i + 1) as u64,
            price_ticks: (10_000 + (i & 1023)) as i64,
            quantity: (1 + (i & 63)) as u32,
            symbol_id: (i & 15) as u32,
            flags: ORDER_FLAG_ACTIVE,
            reserved_pad: 0,
        });
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        random_idx.push((state % (count as u64)) as u32);
    }

    if mode == "all" || mode == "spatial_seq" {
        println!(
            "========================================================================================"
        );
        println!(
            "RUST 1.91 BATTLE 1A: SEQUENTIAL SCAN ({} Orders = 2.0 MiB, {} Passes)",
            count, iters
        );
        println!(
            "========================================================================================"
        );

        if variant == "all" || variant == "iter" {
            for _ in 0..50 {
                black_box(rust_scan_sequential_iter(orders.as_ptr(), orders.len()));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_scan_sequential_iter(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row("1. Safe Slice Iterator (orders.iter())", elapsed, iters, count, acc);
        }

        if variant == "all" || variant == "indexed" {
            for _ in 0..50 {
                black_box(rust_scan_sequential_indexed(orders.as_ptr(), orders.len()));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_scan_sequential_indexed(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row("2. Safe Indexed Loop (0..orders.len())", elapsed, iters, count, acc);
        }
    }

    if mode == "all" || mode == "spatial_rand" {
        println!(
            "========================================================================================"
        );
        println!(
            "RUST 1.91 BATTLE 1B: INDEPENDENT RANDOM BATCH GATHER ({} Lookups, {} Passes)",
            count, iters
        );
        println!(
            "========================================================================================"
        );

        if variant == "all" || variant == "unchecked" {
            for _ in 0..50 {
                black_box(rust_gather_batch_unchecked(
                    orders.as_ptr(),
                    orders.len(),
                    random_idx.as_ptr(),
                    random_idx.len(),
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_gather_batch_unchecked(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                    black_box(random_idx.as_ptr()),
                    black_box(random_idx.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "1. unsafe get_unchecked(idx) (AVX-512 Gather)",
                elapsed,
                iters,
                count,
                acc,
            );
        }

        if variant == "all" || variant == "checked" {
            for _ in 0..50 {
                black_box(rust_gather_batch_checked(
                    orders.as_ptr(),
                    orders.len(),
                    random_idx.as_ptr(),
                    random_idx.len(),
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_gather_batch_checked(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                    black_box(random_idx.as_ptr()),
                    black_box(random_idx.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "2. Safe Slice Indexing orders[idx] (Panic Check)",
                elapsed,
                iters,
                count,
                acc,
            );
        }

        if variant == "all" || variant == "option_get" {
            for _ in 0..50 {
                black_box(rust_gather_batch_option_get(
                    orders.as_ptr(),
                    orders.len(),
                    random_idx.as_ptr(),
                    random_idx.len(),
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_gather_batch_option_get(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                    black_box(random_idx.as_ptr()),
                    black_box(random_idx.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "3. Safe orders.get(idx) -> Option<&Order>",
                elapsed,
                iters,
                count,
                acc,
            );
        }

        if variant == "all" || variant == "bitmask" {
            for _ in 0..50 {
                black_box(rust_gather_batch_bitmask_safe(
                    orders.as_ptr(),
                    orders.len(),
                    random_idx.as_ptr(),
                    random_idx.len(),
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_gather_batch_bitmask_safe(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                    black_box(random_idx.as_ptr()),
                    black_box(random_idx.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "4. Safe Bitmask orders[idx & 0xFFFF] (AVX-512)",
                elapsed,
                iters,
                count,
                acc,
            );
        }
    }

    if mode == "all" || mode == "spatial_dep" {
        println!(
            "========================================================================================"
        );
        println!(
            "RUST 1.91 BATTLE 1C: DEPENDENT ORDER-BOOK PROBE CHAIN ({} Probes, {} Passes)",
            count, iters
        );
        println!(
            "========================================================================================"
        );

        if variant == "all" || variant == "unchecked" {
            for _ in 0..50 {
                black_box(rust_probe_dependent_unchecked(
                    orders.as_ptr(),
                    orders.len(),
                    random_idx.as_ptr(),
                    random_idx.len(),
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_probe_dependent_unchecked(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                    black_box(random_idx.as_ptr()),
                    black_box(random_idx.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "1. unsafe get_unchecked(idx) Dependent Probe",
                elapsed,
                iters,
                count,
                acc,
            );
        }

        if variant == "all" || variant == "checked" {
            for _ in 0..50 {
                black_box(rust_probe_dependent_checked(
                    orders.as_ptr(),
                    orders.len(),
                    random_idx.as_ptr(),
                    random_idx.len(),
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_probe_dependent_checked(
                    black_box(orders.as_ptr()),
                    black_box(orders.len()),
                    black_box(random_idx.as_ptr()),
                    black_box(random_idx.len()),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "2. Safe orders[idx] Dependent Probe (Panic Check)",
                elapsed,
                iters,
                count,
                acc,
            );
        }
    }

    if mode == "all" || mode == "temporal" {
        println!(
            "========================================================================================"
        );
        println!(
            "RUST 1.91 BATTLE 3: TEMPORAL SAFETY UNDER 25% CANCELLATION & ABA RECYCLING ({} Passes)",
            iters
        );
        println!(
            "========================================================================================"
        );

        let mut pool = OrderSlabPool::new(count);
        let mut original_handles = Vec::with_capacity(count);
        let mut live_rc_book = Vec::with_capacity(count);
        let mut original_weak = Vec::with_capacity(count);

        for i in 0..count {
            let initial = Order {
                order_id: (i + 1) as u64,
                price_ticks: (10_000 + (i & 1023)) as i64,
                quantity: (1 + (i & 63)) as u32,
                symbol_id: (i & 15) as u32,
                flags: ORDER_FLAG_ACTIVE,
                reserved_pad: 0,
            };
            original_handles.push(pool.allocate(initial).unwrap());
            let rc = Rc::new(initial);
            original_weak.push(Rc::downgrade(&rc));
            live_rc_book.push(rc);
        }

        for i in (0..count).step_by(4) {
            pool.release(original_handles[i]);
            let replacement = Order {
                order_id: (1_000_000 + i) as u64,
                price_ticks: 999_999,
                quantity: 100,
                symbol_id: (i & 15) as u32,
                flags: ORDER_FLAG_ACTIVE,
                reserved_pad: 0,
            };
            let _ = pool.allocate(replacement);
            live_rc_book[i] = Rc::new(replacement);
        }

        let mut lookup_order = Vec::with_capacity(count);
        let mut tstate: u64 = 0x9E37_79B9_7F4A_7C15;
        for _ in 0..count {
            tstate ^= tstate << 13;
            tstate ^= tstate >> 7;
            tstate ^= tstate << 17;
            lookup_order.push((tstate % (count as u64)) as u32);
        }

        if variant == "all" || variant == "unchecked" {
            for _ in 0..50 {
                black_box(rust_temporal_unchecked_lookup(
                    &pool,
                    &original_handles,
                    &lookup_order,
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_temporal_unchecked_lookup(
                    black_box(&pool),
                    black_box(&original_handles),
                    black_box(&lookup_order),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "1. unsafe Unchecked Slot (25% Silent ABA Bug!)",
                elapsed,
                iters,
                count,
                acc,
            );
        }

        if variant == "all" || variant == "generational" {
            for _ in 0..50 {
                black_box(rust_temporal_generational_lookup(
                    &pool,
                    &original_handles,
                    &lookup_order,
                ));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_temporal_generational_lookup(
                    black_box(&pool),
                    black_box(&original_handles),
                    black_box(&lookup_order),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "2. Safe 64-Bit Generational Handle (O(1) Safe)",
                elapsed,
                iters,
                count,
                acc,
            );
        }

        if variant == "all" || variant == "rc_weak" {
            for _ in 0..50 {
                black_box(rust_temporal_rc_weak_lookup(&original_weak, &lookup_order));
            }
            let mut acc: i64 = 0;
            let t0 = Instant::now();
            for _ in 0..iters {
                acc = acc.wrapping_add(rust_temporal_rc_weak_lookup(
                    black_box(&original_weak),
                    black_box(&lookup_order),
                ));
            }
            let elapsed = t0.elapsed().as_nanos() as f64;
            print_row(
                "3. Safe Rc<Order> / Weak::upgrade()",
                elapsed,
                iters,
                count,
                acc,
            );
        }
    }
}
