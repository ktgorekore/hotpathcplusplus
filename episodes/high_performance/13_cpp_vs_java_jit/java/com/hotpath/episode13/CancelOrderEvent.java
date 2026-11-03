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
 * Concrete order cancellation event (`tag = 1`), representing the secondary subtype in bimodal
 * (50/50) dispatch benchmarks.
 *
 * <p><b>Thread Safety Guarantee:</b> Immutable and unconditionally thread-safe.
 *
 * <p><b>Memory Consistency:</b> Safe publication via {@code final} fields (JLS 17.5).
 */
public final class CancelOrderEvent implements OrderEvent {
  private final long price;
  private final long qty;

  /**
   * Constructs a {@code CancelOrderEvent}.
   *
   * @param price Original order price in ticks.
   * @param qty Cancelled quantity in lots.
   */
  public CancelOrderEvent(long price, long qty) {
    this.price = price;
    this.qty = qty;
  }

  @Override
  public OrderResult processHeapAlloc() {
    return new OrderResult(-(price * qty), 0L);
  }

  @Override
  public long processNoAlloc() {
    return -(price * qty);
  }
}
