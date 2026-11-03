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

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

/**
 * Battle 2 memory-layout containers and scan kernels comparing idiomatic Java {@code
 * ArrayList<OrderRecord>} (both freshly TLAB-allocated and post-churn fragmented heap) against flat
 * primitive Structure-of-Arrays ({@code long[]}) layout.
 *
 * <p><b>Thread Safety Guarantee:</b> {@link PrimitiveSoABook} and {@code ArrayList<OrderRecord>}
 * instances are not synchronized for concurrent mutation; read-only scans via {@link
 * #scanArrayListOrders(List)} and {@link #scanPrimitiveSoA(long[], long[], long[], int)} are
 * thread-safe once populated.
 *
 * <p><b>Memory Consistency:</b> Callers must safely publish populated containers before concurrent
 * read scans.
 */
public final class OrderBookLayouts {

  private OrderBookLayouts() {}

  /**
   * Flat primitive Structure-of-Arrays (SoA) order book storing parallel {@code long[]} arrays for
   * prices, quantities, and branchless active-order bitmasks.
   *
   * <p><b>Thread Safety Guarantee:</b> Safe for concurrent read scans once initialized.
   */
  public static final class PrimitiveSoABook {
    private final long[] prices;
    private final long[] qtys;
    private final long[] masks;
    private final int size;

    /**
     * Constructs and populates a flat primitive SoA order book of {@code count} orders.
     *
     * @param count Number of orders to allocate in contiguous primitive arrays.
     * @throws IllegalArgumentException If {@code count < 0}.
     */
    public PrimitiveSoABook(int count) {
      if (count < 0) {
        throw new IllegalArgumentException("count must be non-negative: " + count);
      }
      this.size = count;
      this.prices = new long[count];
      this.qtys = new long[count];
      this.masks = new long[count];
      for (int i = 0; i < count; i++) {
        this.prices[i] = 1000L + (i & 1023);
        this.qtys[i] = 1L + (i & 31);
        this.masks[i] = -1L;
      }
    }

    /**
     * Returns the number of orders in the book.
     *
     * @return The order count.
     */
    public int size() {
      return size;
    }

    /**
     * Returns the backing primitive price array.
     *
     * @return The {@code long[]} price array.
     */
    public long[] prices() {
      return prices;
    }

    /**
     * Returns the backing primitive quantity array.
     *
     * @return The {@code long[]} quantity array.
     */
    public long[] qtys() {
      return qtys;
    }

    /**
     * Returns the backing primitive active-mask array.
     *
     * @return The {@code long[]} active-mask array (`-1L` = active, `0L` = inactive).
     */
    public long[] masks() {
      return masks;
    }
  }

  /**
   * Creates an {@link ArrayList} of {@link OrderRecord} objects allocated sequentially inside TLAB
   * buffers.
   *
   * <p>Because sequential {@code new OrderRecord(...)} allocations bump the TLAB pointer
   * contiguously, the L2 streamer hardware prefetcher can initially predict the ~48-byte stride—an
   * artificial microbenchmark advantage that disappears once orders are cancelled and replaced.
   *
   * @param orderCount Number of orders to allocate.
   * @return A sequentially allocated {@link ArrayList} of {@link OrderRecord} instances.
   */
  public static List<OrderRecord> createFreshArrayListBook(int orderCount) {
    ArrayList<OrderRecord> freshOrders = new ArrayList<>(orderCount);
    for (int i = 0; i < orderCount; i++) {
      freshOrders.add(new OrderRecord(i, 1000L + (i & 1023), 1L + (i & 31), i & 255, 1));
    }
    return freshOrders;
  }

  /**
   * Creates an {@link ArrayList} of {@link OrderRecord} objects and shuffles reference order with a
   * deterministic seed ({@code 0xC0FFEE13L}) to simulate realistic production order-book churn and
   * GC promotion across physical pages.
   *
   * @param orderCount Number of orders to allocate and shuffle.
   * @return A heap-churned {@link ArrayList} of {@link OrderRecord} instances.
   */
  public static List<OrderRecord> createChurnedArrayListBook(int orderCount) {
    ArrayList<OrderRecord> churnedOrders = new ArrayList<>(orderCount);
    for (int i = 0; i < orderCount; i++) {
      churnedOrders.add(new OrderRecord(i, 1000L + (i & 1023), 1L + (i & 31), i & 255, 1));
    }
    Collections.shuffle(churnedOrders, new Random(0xC0FFEE13L));
    return churnedOrders;
  }

  /**
   * Scans an {@link ArrayList} of boxed {@link OrderRecord} references and accumulates total active
   * notional exposure ({@code price * qty} where {@code flags == 1}).
   *
   * @param orders The list of {@link OrderRecord} references to scan.
   * @return Total accumulated notional exposure across all active orders.
   */
  public static long scanArrayListOrders(List<OrderRecord> orders) {
    long totalExposure = 0L;
    final int size = orders.size();
    for (int i = 0; i < size; i++) {
      OrderRecord o = orders.get(i);
      if (o.flags() == 1) {
        totalExposure += o.price() * o.qty();
      }
    }
    return totalExposure;
  }

  /**
   * Scans a flat primitive Structure-of-Arrays (SoA) order book and accumulates total active
   * notional exposure using branchless bitmasking.
   *
   * @param prices Contiguous primitive array of order prices.
   * @param qtys Contiguous primitive array of order quantities.
   * @param masks Contiguous primitive array of active masks (`-1L` or `0L`).
   * @param size Number of elements to scan.
   * @return Total accumulated notional exposure across all active orders.
   */
  public static long scanPrimitiveSoA(long[] prices, long[] qtys, long[] masks, int size) {
    long totalExposure = 0L;
    for (int i = 0; i < size; i++) {
      totalExposure += (prices[i] * qtys[i]) & masks[i];
    }
    return totalExposure;
  }
}
