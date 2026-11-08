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

//! Episode 14 Live Session 1: Rust Borrow Checker (`E0502`) & Safe 64-Bit
//! Generational Slab Handle Counterpart.
//!
//! 1. Default build (demonstrates compile-time rejection `error[E0502]`):
//!      rustc --edition=2021 live_uaf_repro.rs
//! 2. Safe Generational Slab build:
//!      rustc --edition=2021 -O --cfg safe_slab live_uaf_repro.rs

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
}

#[cfg(not(safe_slab))]
fn trigger_borrow_checker_violation() {
    let mut book: Vec<Order> = Vec::with_capacity(2);
    book.push(Order {
        order_id: 1001,
        price_ticks: 105_250,
        quantity: 50,
        symbol_id: 7,
        flags: 1,
        reserved_pad: 0,
    });
    book.push(Order {
        order_id: 1002,
        price_ticks: 105_200,
        quantity: 25,
        symbol_id: 7,
        flags: 1,
        reserved_pad: 0,
    });

    // Immutable borrow `&Order` into `book[0]`:
    let best_bid: &Order = &book[0];

    // Mutable borrow `book.push(...)` while `best_bid` is still live below ->
    // rejected at compile time with error[E0502]!
    book.push(Order {
        order_id: 1003,
        price_ticks: 105_150,
        quantity: 100,
        symbol_id: 7,
        flags: 1,
        reserved_pad: 0,
    });

    println!(
        "best_bid order_id={} price_ticks={}",
        best_bid.order_id, best_bid.price_ticks
    );
}

#[cfg(safe_slab)]
fn run_safe_generational_slab_demo() {
    println!("======================================================================");
    println!("RUST 1.91 SAFE 64-BIT GENERATIONAL SLAB HANDLE (OrderSlabPool)");
    println!("======================================================================");
    let mut pool = OrderSlabPool::new(1024);

    let h_bid = pool
        .allocate(Order {
            order_id: 1001,
            price_ticks: 105_250,
            quantity: 50,
            symbol_id: 7,
            flags: 1,
            reserved_pad: 0,
        })
        .expect("slab capacity available");

    println!(
        "[1] Allocated OrderHandle      = {{index={}, generation={}}}",
        h_bid.index, h_bid.generation
    );

    // Cancel order 1001 and immediately recycle slot 0 with a new order.
    pool.release(h_bid);
    let h_replacement = pool
        .allocate(Order {
            order_id: 2002,
            price_ticks: 999_999_900,
            quantity: 999_999,
            symbol_id: 7,
            flags: 1,
            reserved_pad: 0,
        })
        .expect("slab capacity available");

    println!(
        "[2] Recycled Slot 0 Handle     = {{index={}, generation={}}}",
        h_replacement.index, h_replacement.generation
    );

    println!(
        "[3] Lookup stale {{idx={}, gen={}}} = {:?} [SAFE REJECT]",
        h_bid.index,
        h_bid.generation,
        pool.get(h_bid)
    );
    if let Some(live) = pool.get(h_replacement) {
        println!(
            "[4] Lookup live  {{idx={}, gen={}}} = order_id={}, price_ticks={}",
            h_replacement.index, h_replacement.generation, live.order_id, live.price_ticks
        );
    }
}

fn main() {
    #[cfg(not(safe_slab))]
    trigger_borrow_checker_violation();

    #[cfg(safe_slab)]
    run_safe_generational_slab_demo();
}
