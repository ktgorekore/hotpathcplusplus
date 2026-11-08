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
 * Core Java 21 Spatial and Temporal Memory Safety benchmark kernels for Episode 14 ("The Physics of
 * Memory Safety: C++20 vs. Rust vs. Java 21").
 *
 * <p>Implements identical deterministic datasets (`65,536` orders, `0xCAFEBABE1337BEEFL` xorshift64
 * permutation) as C++20 (`memory_safety.h`) and Rust 1.91 (`memory_safety_bench.rs`):
 *
 * <ul>
 *   <li><b>Battle 1A (Sequential Scan):</b> C2 JIT range check elimination (RCE) on primitive
 *       {@code long[]} arrays vs. {@code OrderObj[]} reference arrays.
 *   <li><b>Battle 1B (Independent Random Batch Gather):</b> Mandatory JVM array bounds check on
 *       {@code prices[indices[i]]}, bitmask-bounded {@code prices[indices[i] & 0xFFFF]}, and
 *       pointer-chasing {@code OrderObj[indices[i]]}.
 *   <li><b>Battle 1C (Dependent Probe Chain):</b> Serial data-dependent order-book probe isolating
 *       C2 range-check cost.
 *   <li><b>Battle 3 (Temporal Safety Under 25% Order Cancellation & Churn):</b> Managed heap {@code
 *       OrderObj[]} allocation/GC churn vs. Zero-GC Flat Primitive Generational Slab (`long` packed
 *       handle {@code (generation << 32) | index}).
 * </ul>
 */
public final class JavaMemorySafetyEngine {

  public static final int DEFAULT_BOOK_CAPACITY = 65_536;
  public static final int DEFAULT_BOOK_MASK = DEFAULT_BOOK_CAPACITY - 1;
  public static final int ORDER_FLAG_ACTIVE = 1;
  public static final int ORDER_FLAG_CANCELED = 0xDEAD_0000;

  private JavaMemorySafetyEngine() {}

  /** Pre-initialized spatial dataset matching C++20 {@code SpatialDataset}. */
  public static final class SpatialDataset {
    public final long[] prices;
    public final int[] quantities;
    public final OrderObj[] orderObjs;
    public final int[] randomIdx;

    /**
     * Constructs a deterministic spatial dataset of {@code count} orders.
     *
     * @param count Number of orders (power of two).
     */
    public SpatialDataset(int count) {
      this.prices = new long[count];
      this.quantities = new int[count];
      this.orderObjs = new OrderObj[count];
      this.randomIdx = new int[count];

      long state = 0xCAFE_BABE_1337_BEEFL;
      for (int i = 0; i < count; i++) {
        long price = 10_000L + (i & 1023);
        int qty = 1 + (i & 63);
        this.prices[i] = price;
        this.quantities[i] = qty;
        this.orderObjs[i] = new OrderObj(i + 1L, price, qty, i & 15, ORDER_FLAG_ACTIVE);

        state ^= (state << 13);
        state ^= (state >>> 7);
        state ^= (state << 17);
        this.randomIdx[i] =
            (int)
                (Long.divideUnsigned(state, count) >= 0 ? Long.remainderUnsigned(state, count) : 0);
      }
    }
  }

  /**
   * Zero-GC flat primitive generational slab allocator in Java 21.
   *
   * <p>Packs {@code (generation << 32) | index} into a primitive 64-bit {@code long} handle,
   * eliminating heap object headers, pointer chasing, and GC young-gen evacuation pauses while
   * preserving 100% spatial and temporal safety.
   */
  public static final class PrimitiveGenerationalSlab {
    public final long[] orderIds;
    public final long[] prices;
    public final int[] quantities;
    public final int[] slotState; // (generation << 1) | occupiedBit
    public final int[] nextFree;
    private int freeHead;

    /**
     * Pre-allocates {@code capacity} primitive slab slots.
     *
     * @param capacity Maximum number of slots.
     */
    public PrimitiveGenerationalSlab(int capacity) {
      this.orderIds = new long[capacity];
      this.prices = new long[capacity];
      this.quantities = new int[capacity];
      this.slotState = new int[capacity];
      this.nextFree = new int[capacity];
      this.freeHead = 0;
      for (int i = 0; i < capacity; i++) {
        this.slotState[i] = (1 << 1); // generation = 1, occupied = 0
        this.nextFree[i] = (i + 1 < capacity) ? (i + 1) : -1;
      }
    }

    /**
     * Allocates a slot and returns a packed 64-bit {@code (expectedState << 32) | index} handle.
     *
     * @param orderId Order identifier.
     * @param priceTicks Limit price in ticks.
     * @param quantity Order quantity in lots.
     * @return Packed 64-bit handle, or {@code -1L} if full.
     */
    public long allocate(long orderId, long priceTicks, int quantity) {
      int idx = this.freeHead;
      if (idx < 0) {
        return -1L;
      }
      this.freeHead = this.nextFree[idx];
      this.nextFree[idx] = -1;
      int activeState = this.slotState[idx] | 1;
      this.slotState[idx] = activeState;
      this.orderIds[idx] = orderId;
      this.prices[idx] = priceTicks;
      this.quantities[idx] = quantity;
      return (Integer.toUnsignedLong(activeState) << 32) | Integer.toUnsignedLong(idx);
    }

    /**
     * Releases a slot and increments its generation counter.
     *
     * @param handle Packed 64-bit generational handle.
     * @return {@code true} if valid and released; {@code false} if stale.
     */
    public boolean release(long handle) {
      int idx = (int) handle;
      int expectedState = (int) (handle >>> 32);
      if (idx < 0 || idx >= this.slotState.length) {
        return false;
      }
      if (this.slotState[idx] != expectedState) {
        return false;
      }
      int nextGen = (expectedState >>> 1) + 1;
      if (nextGen == 0) {
        nextGen = 1;
      }
      this.slotState[idx] = (nextGen << 1); // occupied = 0
      this.nextFree[idx] = this.freeHead;
      this.freeHead = idx;
      return true;
    }

    /**
     * Evaluates {@code priceTicks * quantity} for a packed handle if and only if its generation and
     * occupied state match.
     *
     * @param handle Packed 64-bit generational handle.
     * @return Order notional, or {@code 0L} if the handle is stale/canceled.
     */
    public long notionalIfValid(long handle) {
      int idx = (int) handle;
      int expectedState = (int) (handle >>> 32);
      if (this.slotState[idx] == expectedState) {
        return this.prices[idx] * (long) this.quantities[idx];
      }
      return 0L;
    }
  }

  /** Pre-constructed temporal churn scenario matching C++20 {@code TemporalChurnScenario}. */
  public static final class TemporalDataset {
    public final PrimitiveGenerationalSlab slab;
    public final long[] originalSlabHandles;
    public final OrderObj[] originalHeapRefs;
    public final OrderObj[] liveHeapBook;
    public final int[] lookupOrder;

    /**
     * Builds the 25% cancellation + ABA slot replacement scenario across {@code count} orders.
     *
     * @param count Number of orders.
     */
    public TemporalDataset(int count) {
      this.slab = new PrimitiveGenerationalSlab(count);
      this.originalSlabHandles = new long[count];
      this.originalHeapRefs = new OrderObj[count];
      this.liveHeapBook = new OrderObj[count];
      this.lookupOrder = new int[count];

      for (int i = 0; i < count; i++) {
        long price = 10_000L + (i & 1023);
        int qty = 1 + (i & 63);
        this.originalSlabHandles[i] = this.slab.allocate(i + 1L, price, qty);
        OrderObj obj = new OrderObj(i + 1L, price, qty, i & 15, ORDER_FLAG_ACTIVE);
        this.originalHeapRefs[i] = obj;
        this.liveHeapBook[i] = obj;
      }

      for (int i = 0; i < count; i += 4) {
        this.slab.release(this.originalSlabHandles[i]);
        this.slab.allocate(1_000_000L + i, 999_999L, 100);

        this.originalHeapRefs[i].flags = ORDER_FLAG_CANCELED;
        this.liveHeapBook[i] =
            new OrderObj(1_000_000L + i, 999_999L, 100, i & 15, ORDER_FLAG_ACTIVE);
      }

      long state = 0x9E37_79B9_7F4A_7C15L;
      for (int i = 0; i < count; i++) {
        state ^= (state << 13);
        state ^= (state >>> 7);
        state ^= (state << 17);
        this.lookupOrder[i] = (int) Long.remainderUnsigned(state, count);
      }
    }
  }

  /**
   * Battle 1A.1: Sequential scan over primitive {@code long[]} and {@code int[]} arrays (C2 JIT
   * hoists range checks via Range Check Elimination).
   *
   * @param prices Primitive price array.
   * @param quantities Primitive quantity array.
   * @return Total notional value.
   */
  public static long scanSequentialPrimitive(long[] prices, int[] quantities) {
    long total = 0L;
    int n = prices.length;
    for (int i = 0; i < n; i++) {
      total += prices[i] * (long) quantities[i];
    }
    return total;
  }

  /**
   * Battle 1A.2: Sequential scan over {@code OrderObj[]} reference array.
   *
   * @param orders Managed order object array.
   * @return Total notional value.
   */
  public static long scanSequentialObjectArray(OrderObj[] orders) {
    long total = 0L;
    int n = orders.length;
    for (int i = 0; i < n; i++) {
      OrderObj o = orders[i];
      total += o.priceTicks * (long) o.quantity;
    }
    return total;
  }

  /**
   * Battle 1B.1: Independent random batch gather over primitive arrays (mandatory JVM bounds check
   * on each random index {@code idx}).
   *
   * @param prices Primitive price array.
   * @param quantities Primitive quantity array.
   * @param indices Permuted lookup index array.
   * @return Total notional value.
   */
  public static long gatherBatchPrimitiveChecked(long[] prices, int[] quantities, int[] indices) {
    long total = 0L;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      int idx = indices[i];
      total += prices[idx] * (long) quantities[idx];
    }
    return total;
  }

  /**
   * Battle 1B.2: Independent random batch gather with power-of-two bitmask {@code indices[i] &
   * 0xFFFF}.
   *
   * @param prices Primitive price array.
   * @param quantities Primitive quantity array.
   * @param indices Permuted lookup index array.
   * @param mask Power-of-two bitmask ({@code capacity - 1}).
   * @return Total notional value.
   */
  public static long gatherBatchPrimitiveBitmask(
      long[] prices, int[] quantities, int[] indices, int mask) {
    long total = 0L;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      int idx = indices[i] & mask;
      total += prices[idx] * (long) quantities[idx];
    }
    return total;
  }

  /**
   * Battle 1B.3: Independent random batch gather over {@code OrderObj[]} reference array.
   *
   * @param orders Managed order object array.
   * @param indices Permuted lookup index array.
   * @return Total notional value.
   */
  public static long gatherBatchObjectArray(OrderObj[] orders, int[] indices) {
    long total = 0L;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      OrderObj o = orders[indices[i]];
      total += o.priceTicks * (long) o.quantity;
    }
    return total;
  }

  /**
   * Battle 1C: Latency-bound dependent order-book probe chain over primitive arrays.
   *
   * @param prices Primitive price array.
   * @param quantities Primitive quantity array.
   * @param indices Base probe index array.
   * @param mask Power-of-two bitmask.
   * @return Accumulated dependent probe notional.
   */
  public static long probeDependentPrimitive(
      long[] prices, int[] quantities, int[] indices, int mask) {
    long total = 0L;
    int prevMix = 0;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      int idx = (indices[i] ^ prevMix) & mask;
      long notional = prices[idx] * (long) quantities[idx];
      total += notional;
      prevMix = ((int) notional) & 0xFF;
    }
    return total;
  }

  /**
   * Battle 3.1: Zero-GC Flat Primitive Generational Slab lookup (`(generation << 32) | index`).
   *
   * @param slab Primitive generational slab pool.
   * @param handles Original 64-bit packed handles.
   * @param lookupOrder Permuted lookup order.
   * @return Exact uncorrupted notional across the 75% surviving orders.
   */
  public static long runTemporalPrimitiveSlabLookup(
      PrimitiveGenerationalSlab slab, long[] handles, int[] lookupOrder) {
    long total = 0L;
    int n = lookupOrder.length;
    for (int i = 0; i < n; i++) {
      long h = handles[lookupOrder[i]];
      total += slab.notionalIfValid(h);
    }
    return total;
  }

  /**
   * Battle 3.2: Managed Heap {@code OrderObj[]} lookup with active allocation churn (allocates a
   * replacement {@code OrderObj} every 64th lookup to model continuous order-book churn under G1GC
   * / Generational ZGC).
   *
   * @param originalRefs Original {@code OrderObj} references held by the gateway.
   * @param liveBook Live order book array receiving replacement allocations.
   * @param lookupOrder Permuted lookup order.
   * @param passIndex Current pass counter for churn rotation.
   * @return Exact uncorrupted notional across the 75% surviving orders.
   */
  public static long runTemporalManagedHeapLookupWithChurn(
      OrderObj[] originalRefs, OrderObj[] liveBook, int[] lookupOrder, int passIndex) {
    long total = 0L;
    int n = lookupOrder.length;
    for (int i = 0; i < n; i++) {
      int pos = lookupOrder[i];
      OrderObj o = originalRefs[pos];
      if (o.flags == ORDER_FLAG_ACTIVE) {
        total += o.priceTicks * (long) o.quantity;
      }
      if ((i & 7) == 0) {
        int churnSlot = (pos + passIndex) & DEFAULT_BOOK_MASK;
        liveBook[churnSlot] =
            new OrderObj(2_000_000L + i, 10_500L, 10, churnSlot & 15, ORDER_FLAG_ACTIVE);
      }
    }
    return total;
  }
}
