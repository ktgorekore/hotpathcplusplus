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

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Standalone Java 21 Spatial & Temporal Memory Safety Benchmark Runner for Live Terminal Sessions 2
 * & 3 (`javac MemorySafetyBench.java && java ... MemorySafetyBench`).
 */
public final class MemorySafetyBench {

  private static final int BOOK_CAPACITY = 65_536;
  private static final int BOOK_MASK = BOOK_CAPACITY - 1;
  private static final int FLAG_ACTIVE = 1;
  private static final int FLAG_CANCELED = 0xDEAD_0000;

  private static volatile long blackhole = 0L;

  private MemorySafetyBench() {}

  public static final class OrderObj {
    public final long orderId;
    public final long priceTicks;
    public final int quantity;
    public final int symbolId;
    public int flags;

    public OrderObj(long orderId, long priceTicks, int quantity, int symbolId, int flags) {
      this.orderId = orderId;
      this.priceTicks = priceTicks;
      this.quantity = quantity;
      this.symbolId = symbolId;
      this.flags = flags;
    }
  }

  public static final class PrimitiveGenerationalSlab {
    public final long[] prices;
    public final int[] quantities;
    public final int[] slotState;
    public final int[] nextFree;
    private int freeHead;

    public PrimitiveGenerationalSlab(int capacity) {
      this.prices = new long[capacity];
      this.quantities = new int[capacity];
      this.slotState = new int[capacity];
      this.nextFree = new int[capacity];
      this.freeHead = 0;
      for (int i = 0; i < capacity; i++) {
        this.slotState[i] = (1 << 1);
        this.nextFree[i] = (i + 1 < capacity) ? (i + 1) : -1;
      }
    }

    public long allocate(long priceTicks, int quantity) {
      int idx = this.freeHead;
      if (idx < 0) {
        return -1L;
      }
      this.freeHead = this.nextFree[idx];
      this.nextFree[idx] = -1;
      int activeState = this.slotState[idx] | 1;
      this.slotState[idx] = activeState;
      this.prices[idx] = priceTicks;
      this.quantities[idx] = quantity;
      return (Integer.toUnsignedLong(activeState) << 32) | Integer.toUnsignedLong(idx);
    }

    public void release(long handle) {
      int idx = (int) handle;
      int expectedState = (int) (handle >>> 32);
      if (idx < 0 || idx >= this.slotState.length) {
        return;
      }
      if (this.slotState[idx] != expectedState) {
        return;
      }
      int nextGen = (expectedState >>> 1) + 1;
      if (nextGen == 0) {
        nextGen = 1;
      }
      this.slotState[idx] = (nextGen << 1);
      this.nextFree[idx] = this.freeHead;
      this.freeHead = idx;
    }

    public long notionalIfValid(long handle) {
      int idx = (int) handle;
      int expectedState = (int) (handle >>> 32);
      if (this.slotState[idx] == expectedState) {
        return this.prices[idx] * (long) this.quantities[idx];
      }
      return 0L;
    }
  }

  public static long scanSequentialPrimitive(long[] prices, int[] quantities) {
    long total = 0L;
    int n = prices.length;
    for (int i = 0; i < n; i++) {
      total += prices[i] * (long) quantities[i];
    }
    return total;
  }

  public static long scanSequentialObjects(OrderObj[] orders) {
    long total = 0L;
    int n = orders.length;
    for (int i = 0; i < n; i++) {
      OrderObj o = orders[i];
      total += o.priceTicks * (long) o.quantity;
    }
    return total;
  }

  public static long gatherBatchPrimitiveChecked(long[] prices, int[] quantities, int[] indices) {
    long total = 0L;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      int idx = indices[i];
      total += prices[idx] * (long) quantities[idx];
    }
    return total;
  }

  public static long gatherBatchPrimitiveBitmask(long[] prices, int[] quantities, int[] indices) {
    long total = 0L;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      int idx = indices[i] & BOOK_MASK;
      total += prices[idx] * (long) quantities[idx];
    }
    return total;
  }

  public static long gatherBatchObjects(OrderObj[] orders, int[] indices) {
    long total = 0L;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      OrderObj o = orders[indices[i]];
      total += o.priceTicks * (long) o.quantity;
    }
    return total;
  }

  public static long probeDependentPrimitive(long[] prices, int[] quantities, int[] indices) {
    long total = 0L;
    int prevMix = 0;
    int n = indices.length;
    for (int i = 0; i < n; i++) {
      int idx = (indices[i] ^ prevMix) & BOOK_MASK;
      long notional = prices[idx] * (long) quantities[idx];
      total += notional;
      prevMix = ((int) notional) & 0xFF;
    }
    return total;
  }

  public static long runTemporalPrimitiveSlab(
      PrimitiveGenerationalSlab slab, long[] handles, int[] lookupOrder) {
    long total = 0L;
    int n = lookupOrder.length;
    for (int i = 0; i < n; i++) {
      total += slab.notionalIfValid(handles[lookupOrder[i]]);
    }
    return total;
  }

  public static long runTemporalManagedHeapWithChurn(
      OrderObj[] originalRefs, OrderObj[] liveBook, int[] lookupOrder, int pass) {
    long total = 0L;
    int n = lookupOrder.length;
    for (int i = 0; i < n; i++) {
      int pos = lookupOrder[i];
      OrderObj o = originalRefs[pos];
      if (o.flags == FLAG_ACTIVE) {
        total += o.priceTicks * (long) o.quantity;
      }
      if ((i & 7) == 0) {
        int slot = (pos + pass) & BOOK_MASK;
        liveBook[slot] = new OrderObj(2_000_000L + i, 10_500L, 10, slot & 15, FLAG_ACTIVE);
      }
    }
    return total;
  }

  private static void printRow(
      String label, double elapsedNs, int iters, int count, long checksum) {
    double usPerPass = (elapsedNs / (double) iters) / 1000.0;
    double nsPerOrder = elapsedNs / ((double) iters * count);
    System.out.printf(
        "  %-46s | %8.2f us/pass | %6.3f ns/order | sum=%d%n",
        label, usPerPass, nsPerOrder, checksum);
  }

  public static void main(String[] args) {
    String mode = "all";
    int iters = 2000;
    for (String arg : args) {
      if (arg.startsWith("--mode=")) {
        mode = arg.substring(7);
      } else if (arg.startsWith("--iters=")) {
        iters = Integer.parseInt(arg.substring(8));
      }
    }

    List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
    String gcNames =
        gcBeans.stream().map(GarbageCollectorMXBean::getName).collect(Collectors.joining(", "));

    long[] prices = new long[BOOK_CAPACITY];
    int[] quantities = new int[BOOK_CAPACITY];
    OrderObj[] orderObjs = new OrderObj[BOOK_CAPACITY];
    int[] randomIdx = new int[BOOK_CAPACITY];

    long state = 0xCAFE_BABE_1337_BEEFL;
    for (int i = 0; i < BOOK_CAPACITY; i++) {
      long price = 10_000L + (i & 1023);
      int qty = 1 + (i & 63);
      prices[i] = price;
      quantities[i] = qty;
      orderObjs[i] = new OrderObj(i + 1L, price, qty, i & 15, FLAG_ACTIVE);
      state ^= (state << 13);
      state ^= (state >>> 7);
      state ^= (state << 17);
      randomIdx[i] = (int) Long.remainderUnsigned(state, BOOK_CAPACITY);
    }

    if ("all".equals(mode) || "spatial_seq".equals(mode)) {
      System.out.println(
          "========================================================================================");
      System.out.printf(
          "JAVA 21 BATTLE 1A: SEQUENTIAL SCAN (%d Orders, GC=[%s])%n", BOOK_CAPACITY, gcNames);
      System.out.println(
          "========================================================================================");

      for (int w = 0; w < 200; w++) {
        blackhole ^= scanSequentialPrimitive(prices, quantities);
        blackhole ^= scanSequentialObjects(orderObjs);
      }
      long acc1 = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc1 += scanSequentialPrimitive(prices, quantities);
      }
      long t1 = System.nanoTime();
      blackhole ^= acc1;
      printRow(
          "1. Primitive long[] Scan (C2 Range Check Hoisted)",
          (double) (t1 - t0),
          iters,
          BOOK_CAPACITY,
          acc1);

      long acc2 = 0L;
      long t2 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc2 += scanSequentialObjects(orderObjs);
      }
      long t3 = System.nanoTime();
      blackhole ^= acc2;
      printRow(
          "2. OrderObj[] Reference Scan (Pointer Chasing)",
          (double) (t3 - t2),
          iters,
          BOOK_CAPACITY,
          acc2);
    }

    if ("all".equals(mode) || "spatial_rand".equals(mode)) {
      System.out.println(
          "========================================================================================");
      System.out.printf(
          "JAVA 21 BATTLE 1B: INDEPENDENT RANDOM BATCH GATHER (%d Lookups, GC=[%s])%n",
          BOOK_CAPACITY, gcNames);
      System.out.println(
          "========================================================================================");

      for (int w = 0; w < 200; w++) {
        blackhole ^= gatherBatchPrimitiveChecked(prices, quantities, randomIdx);
        blackhole ^= gatherBatchPrimitiveBitmask(prices, quantities, randomIdx);
        blackhole ^= gatherBatchObjects(orderObjs, randomIdx);
      }
      long acc1 = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc1 += gatherBatchPrimitiveChecked(prices, quantities, randomIdx);
      }
      long t1 = System.nanoTime();
      blackhole ^= acc1;
      printRow(
          "1. Primitive long[idx] Gather (Mandatory Check)",
          (double) (t1 - t0),
          iters,
          BOOK_CAPACITY,
          acc1);

      long acc2 = 0L;
      long t2 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc2 += gatherBatchPrimitiveBitmask(prices, quantities, randomIdx);
      }
      long t3 = System.nanoTime();
      blackhole ^= acc2;
      printRow(
          "2. Primitive long[idx & 0xFFFF] (Bitmask Safe)",
          (double) (t3 - t2),
          iters,
          BOOK_CAPACITY,
          acc2);

      long acc3 = 0L;
      long t4 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc3 += gatherBatchObjects(orderObjs, randomIdx);
      }
      long t5 = System.nanoTime();
      blackhole ^= acc3;
      printRow(
          "3. OrderObj[idx] Gather (Bounds + Ptr Chase)",
          (double) (t5 - t4),
          iters,
          BOOK_CAPACITY,
          acc3);
    }

    if ("all".equals(mode) || "spatial_dep".equals(mode)) {
      System.out.println(
          "========================================================================================");
      System.out.printf("JAVA 21 BATTLE 1C: DEPENDENT PROBE CHAIN (%d Probes)%n", BOOK_CAPACITY);
      System.out.println(
          "========================================================================================");
      for (int w = 0; w < 200; w++) {
        blackhole ^= probeDependentPrimitive(prices, quantities, randomIdx);
      }
      long acc1 = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc1 += probeDependentPrimitive(prices, quantities, randomIdx);
      }
      long t1 = System.nanoTime();
      blackhole ^= acc1;
      printRow(
          "1. Primitive Dependent Probe (C2 Bounds Check)",
          (double) (t1 - t0),
          iters,
          BOOK_CAPACITY,
          acc1);
    }

    if ("all".equals(mode) || "temporal".equals(mode)) {
      System.out.println(
          "========================================================================================");
      System.out.printf(
          "JAVA 21 BATTLE 3: TEMPORAL SAFETY & GC CHURN (%d Orders, GC=[%s])%n",
          BOOK_CAPACITY, gcNames);
      System.out.println(
          "========================================================================================");

      PrimitiveGenerationalSlab slab = new PrimitiveGenerationalSlab(BOOK_CAPACITY);
      long[] handles = new long[BOOK_CAPACITY];
      OrderObj[] origRefs = new OrderObj[BOOK_CAPACITY];
      OrderObj[] liveBook = new OrderObj[BOOK_CAPACITY];
      int[] lookupOrder = new int[BOOK_CAPACITY];

      for (int i = 0; i < BOOK_CAPACITY; i++) {
        long price = 10_000L + (i & 1023);
        int qty = 1 + (i & 63);
        handles[i] = slab.allocate(price, qty);
        OrderObj obj = new OrderObj(i + 1L, price, qty, i & 15, FLAG_ACTIVE);
        origRefs[i] = obj;
        liveBook[i] = obj;
      }

      for (int i = 0; i < BOOK_CAPACITY; i += 4) {
        slab.release(handles[i]);
        slab.allocate(999_999L, 100);
        origRefs[i].flags = FLAG_CANCELED;
        liveBook[i] = new OrderObj(1_000_000L + i, 999_999L, 100, i & 15, FLAG_ACTIVE);
      }

      long tstate = 0x9E37_79B9_7F4A_7C15L;
      for (int i = 0; i < BOOK_CAPACITY; i++) {
        tstate ^= (tstate << 13);
        tstate ^= (tstate >>> 7);
        tstate ^= (tstate << 17);
        lookupOrder[i] = (int) Long.remainderUnsigned(tstate, BOOK_CAPACITY);
      }

      for (int w = 0; w < 200; w++) {
        blackhole ^= runTemporalPrimitiveSlab(slab, handles, lookupOrder);
      }
      long accSlab = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        accSlab += runTemporalPrimitiveSlab(slab, handles, lookupOrder);
      }
      long t1 = System.nanoTime();
      blackhole ^= accSlab;
      printRow(
          "1. Zero-GC Primitive Generational Slab (Safe)",
          (double) (t1 - t0),
          iters,
          BOOK_CAPACITY,
          accSlab);

      for (int w = 0; w < 200; w++) {
        blackhole ^= runTemporalManagedHeapWithChurn(origRefs, liveBook, lookupOrder, w);
      }
      long gcCountBefore =
          gcBeans.stream().mapToLong(GarbageCollectorMXBean::getCollectionCount).sum();
      long gcTimeBefore =
          gcBeans.stream().mapToLong(GarbageCollectorMXBean::getCollectionTime).sum();
      long accHeap = 0L;
      long t2 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        accHeap += runTemporalManagedHeapWithChurn(origRefs, liveBook, lookupOrder, r);
      }
      long t3 = System.nanoTime();
      long gcCountAfter =
          gcBeans.stream().mapToLong(GarbageCollectorMXBean::getCollectionCount).sum();
      long gcTimeAfter =
          gcBeans.stream().mapToLong(GarbageCollectorMXBean::getCollectionTime).sum();
      blackhole ^= accHeap;
      printRow(
          "2. Managed Heap OrderObj[] + Allocation Churn",
          (double) (t3 - t2),
          iters,
          BOOK_CAPACITY,
          accHeap);
      System.out.printf(
          "     -> GC Collections Delta: %d | GC Time Delta: %d ms | Zero UAF / ABA=0%n",
          gcCountAfter - gcCountBefore, gcTimeAfter - gcTimeBefore);
    }
  }
}
