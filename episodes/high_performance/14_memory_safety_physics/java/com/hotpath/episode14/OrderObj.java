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

package com.hotpath.episode14;

/**
 * Managed Java 21 heap order object (`12B` HotSpot object header + `8B` {@code orderId} + `8B`
 * {@code priceTicks} + `4B` {@code quantity} = `32B` heap footprint, plus `4B` compressed oops
 * reference slot in {@code OrderObj[]}).
 *
 * <p>Because the JVM garbage collector retains every reachable {@code OrderObj}, temporal
 * Use-After-Free and ABA type confusion are impossible in safe Java; however, pointer chasing and
 * allocation churn incur cache-line indirection and GC barrier overhead compared to flat primitive
 * slabs.
 */
public final class OrderObj {
  public final long orderId;
  public final long priceTicks;
  public final int quantity;
  public final int symbolId;
  public int flags;

  /**
   * Constructs a managed heap {@code OrderObj}.
   *
   * @param orderId Monotonic order identifier.
   * @param priceTicks Limit price in ticks.
   * @param quantity Order quantity in lots.
   * @param symbolId Numeric symbol identifier.
   * @param flags Status bitmask (`1` = active).
   */
  public OrderObj(long orderId, long priceTicks, int quantity, int symbolId, int flags) {
    this.orderId = orderId;
    this.priceTicks = priceTicks;
    this.quantity = quantity;
    this.symbolId = symbolId;
    this.flags = flags;
  }
}
