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
 * Concrete modify/replace order event (`tag = 2`), participating in megamorphic (5-subtype)
 * dispatch benchmarks.
 *
 * <p><b>Thread Safety Guarantee:</b> Immutable and unconditionally thread-safe.
 *
 * <p><b>Memory Consistency:</b> Safe publication via {@code final} fields (JLS 17.5).
 */
public final class ModifyOrderEvent implements OrderEvent {
  private final long price;
  private final long qty;

  /**
   * Constructs a {@code ModifyOrderEvent}.
   *
   * @param price Modified order price in ticks.
   * @param qty Modified order quantity in lots.
   */
  public ModifyOrderEvent(long price, long qty) {
    this.price = price;
    this.qty = qty;
  }

  @Override
  public OrderResult processHeapAlloc() {
    long gross = price * (qty + 1L);
    return new OrderResult(gross, gross >> 9);
  }

  @Override
  public long processNoAlloc() {
    return (price * (qty + 1L)) - ((price * qty) >> 9);
  }
}
