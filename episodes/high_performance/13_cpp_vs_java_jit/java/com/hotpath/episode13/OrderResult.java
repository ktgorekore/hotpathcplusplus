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
 * Immutable result object returned from order event processing in Battle 1.
 *
 * <p>When a factory or dispatch call is inlined by the HotSpot C2 compiler and this instance does
 * not escape the calling compilation unit (`NoEscape`), C2 performs Escape Analysis and Scalar
 * Replacement—unpacking {@code notional} and {@code feeTicks} directly into 64-bit hardware general
 * purpose registers ({@code rax}, {@code rdx}) with zero heap allocation or TLAB pointer movement.
 *
 * <p><b>Thread Safety Guarantee:</b> This class is unconditionally thread-safe because all fields
 * are {@code final} primitives initialized during construction.
 *
 * <p><b>Memory Consistency:</b> Safe publication is guaranteed by the Java Memory Model (JLS 17.5)
 * for {@code final} fields upon completion of the constructor.
 */
public final class OrderResult {
  private final long notional;
  private final long feeTicks;

  /**
   * Constructs an immutable {@code OrderResult}.
   *
   * @param notional The gross signed notional value of the order event in price ticks.
   * @param feeTicks The exchange fee in price ticks.
   */
  public OrderResult(long notional, long feeTicks) {
    this.notional = notional;
    this.feeTicks = feeTicks;
  }

  /**
   * Returns the gross signed notional value in price ticks.
   *
   * @return The 64-bit gross signed notional value.
   */
  public long notional() {
    return notional;
  }

  /**
   * Returns the exchange fee in price ticks.
   *
   * @return The 64-bit exchange fee in price ticks.
   */
  public long feeTicks() {
    return feeTicks;
  }

  /**
   * Returns the net notional exposure ({@code notional - feeTicks}).
   *
   * @return The net notional value after subtracting exchange fees.
   */
  public long netNotional() {
    return notional - feeTicks;
  }
}
