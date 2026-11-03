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

package com.hotpath.episode13;

/**
 * Polymorphic order event contract evaluated in Battle 1 (Allocation & Devirtualization) and Battle
 * 3 (C2 Uncommon Trap Deoptimization).
 *
 * <p>In HotSpot JVM, interface dispatch (`invokeinterface`) uses Class Hierarchy Analysis (CHA) and
 * Receiver Type Profiling at Tier 3 to guide C2 Tier 4 speculative devirtualization:
 *
 * <ul>
 *   <li><b>Monomorphic (100% single subtype):</b> C2 emits a single 32-bit compressed class-word
 *       guard ({@code cmp DWORD PTR [rsi+0x8], imm32; jne uncommon_trap}) and inlines the target
 *       method body directly.
 *   <li><b>Bimodal (2 subtypes):</b> C2 emits a two-way Polymorphic Inline Cache (PIC) and inlines
 *       both branches.
 *   <li><b>Megamorphic (3+ subtypes):</b> Inline cache capacity is exceeded (`TypeProfileWidth=2`),
 *       forcing a full virtual dispatch table lookup and defeating Escape Analysis of returned
 *       {@link OrderResult} objects.
 * </ul>
 *
 * <p><b>Thread Safety Guarantee:</b> All concrete implementations must be immutable and
 * thread-safe.
 */
public interface OrderEvent {

  /**
   * Evaluates the event and returns a freshly constructed {@link OrderResult} on the heap (or
   * scalar-replaced into CPU registers when C2 Escape Analysis succeeds).
   *
   * @return A non-null {@link OrderResult} containing gross notional and fee ticks.
   */
  OrderResult processHeapAlloc();

  /**
   * Evaluates the event and returns the net notional value directly as a primitive {@code long}
   * without allocating an {@link OrderResult} wrapper.
   *
   * @return The 64-bit signed net notional value.
   */
  long processNoAlloc();

  /**
   * Factory helper matching the C++ {@code MakeCrossTuEvent(tag, price, qty)} dispatcher across all
   * 5 polymorphic subtypes.
   *
   * @param tag Polymorphic subtype selector in {@code [0, 4]}.
   * @param price Order price in ticks.
   * @param qty Order quantity in lots.
   * @return A newly instantiated {@link OrderEvent} implementation.
   */
  static OrderEvent makeEvent(int tag, long price, long qty) {
    return switch (tag) {
      case 0 -> new LimitOrderEvent(price, qty);
      case 1 -> new CancelOrderEvent(price, qty);
      case 2 -> new ModifyOrderEvent(price, qty);
      case 3 -> new PeggedOrderEvent(price, qty);
      default -> new MarketHaltEvent(price, qty, 42);
    };
  }

  /**
   * Executes one Battle 1 dispatch step, invoking {@link #processHeapAlloc()} and reducing the
   * returned {@link OrderResult} to its net notional primitive value.
   *
   * <p>Can be inspected under HotSpot C2 via {@code
   * -XX:CompileCommand=print,*OrderEvent::runBattle1Step}.
   *
   * @param event The polymorphic {@link OrderEvent} to process.
   * @return The 64-bit net notional exposure ({@code res.notional() - res.feeTicks()}).
   */
  static long runBattle1Step(OrderEvent event) {
    OrderResult res = event.processHeapAlloc();
    return res.notional() - res.feeTicks();
  }
}
