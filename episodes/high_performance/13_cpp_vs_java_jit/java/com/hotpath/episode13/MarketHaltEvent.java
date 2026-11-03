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
 * Rare regulatory/volatility market-halt control event (`tag = 4`).
 *
 * <p>In Battle 3, when the HotSpot C2 compiler has speculatively compiled the dispatch loop
 * assuming 100% {@link LimitOrderEvent} traffic, injecting a single {@code MarketHaltEvent} fails
 * the compressed class-word guard (`cmp [rsi+8], LimitOrderEvent_Klass`) and triggers an Uncommon
 * Trap (`reason=class_check action=maybe_recompile`), deoptimizing back to the Tier 0 interpreter.
 *
 * <p><b>Thread Safety Guarantee:</b> Immutable and unconditionally thread-safe.
 *
 * <p><b>Memory Consistency:</b> Safe publication via {@code final} fields (JLS 17.5).
 */
public final class MarketHaltEvent implements OrderEvent {
  private final long price;
  private final long qty;
  private final int symbolId;

  /**
   * Constructs a {@code MarketHaltEvent}.
   *
   * @param price Reference halt price in ticks.
   * @param qty Reference halt quantity in lots.
   * @param symbolId Numeric symbol identifier.
   */
  public MarketHaltEvent(long price, long qty, int symbolId) {
    this.price = price;
    this.qty = qty;
    this.symbolId = symbolId;
  }

  @Override
  public OrderResult processHeapAlloc() {
    return new OrderResult((price * qty * 3L) ^ symbolId, 0L);
  }

  @Override
  public long processNoAlloc() {
    return (price * qty * 3L) ^ symbolId;
  }
}
