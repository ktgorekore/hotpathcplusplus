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
 * Boxed heap order object used in Battle 2 (`ArrayList<OrderRecord>`) and Battle 3 (`OrderRecord[]`
 * state table under G1GC and Generational ZGC).
 *
 * <p><b>64-bit HotSpot Memory Layout (Compressed OOPs enabled):</b>
 *
 * <ul>
 *   <li>8-byte Mark Word (lock state, identity hash code, GC age/color bits)
 *   <li>4-byte Compressed Klass Pointer
 *   <li>4-byte field: {@code int symbolId}
 *   <li>8-byte field: {@code long orderId}
 *   <li>8-byte field: {@code long price}
 *   <li>8-byte field: {@code long qty}
 *   <li>4-byte field: {@code int flags}
 *   <li>4-byte alignment padding to 8-byte object boundary = <b>48 bytes per instance</b>
 *   <li>Plus 4-byte Compressed OOP reference inside {@code ArrayList.elementData[]} = <b>52 bytes
 *       effective footprint per order</b> (vs. 32 bytes in contiguous C++ {@code
 *       std::vector<Order>} or 24 bytes in hot-field SoA).
 * </ul>
 *
 * <p><b>Thread Safety Guarantee:</b> Immutable and unconditionally thread-safe.
 *
 * <p><b>Memory Consistency:</b> Safe publication via {@code final} fields (JLS 17.5).
 */
public final class OrderRecord {
  private final long orderId;
  private final long price;
  private final long qty;
  private final int symbolId;
  private final int flags;

  /**
   * Constructs a boxed {@code OrderRecord} instance on the Java heap.
   *
   * @param orderId Monotonically increasing 64-bit order identifier.
   * @param price Limit price in integer ticks.
   * @param qty Remaining quantity in lots.
   * @param symbolId Instrument symbol identifier.
   * @param flags Bitmask flags (`1` indicates an active resting order).
   */
  public OrderRecord(long orderId, long price, long qty, int symbolId, int flags) {
    this.orderId = orderId;
    this.price = price;
    this.qty = qty;
    this.symbolId = symbolId;
    this.flags = flags;
  }

  /**
   * Returns the 64-bit order identifier.
   *
   * @return The order ID.
   */
  public long orderId() {
    return orderId;
  }

  /**
   * Returns the order price in integer ticks.
   *
   * @return The price in ticks.
   */
  public long price() {
    return price;
  }

  /**
   * Returns the order quantity in lots.
   *
   * @return The quantity in lots.
   */
  public long qty() {
    return qty;
  }

  /**
   * Returns the numeric instrument symbol identifier.
   *
   * @return The 32-bit symbol identifier.
   */
  public int symbolId() {
    return symbolId;
  }

  /**
   * Returns the order status bitmask flags (`1` = active).
   *
   * @return The 32-bit flags value.
   */
  public int flags() {
    return flags;
  }
}
