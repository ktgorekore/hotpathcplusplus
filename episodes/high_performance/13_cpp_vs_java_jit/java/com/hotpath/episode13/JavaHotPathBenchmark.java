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

import java.util.List;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Level;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

/**
 * OpenJDK JMH microbenchmark suite for Episode 13 ("Is C++ Actually Faster Than Java?").
 *
 * <p>Measures Battle 1 (TLAB & Escape Analysis + Monomorphic/Bimodal/Megamorphic interface
 * dispatch) and Battle 2 (Cache-Line Physics across 4,000,000 orders in {@code
 * ArrayList<OrderRecord>} vs. flat primitive {@code long[]} SoA arrays).
 *
 * <p><b>Thread Safety Guarantee:</b> JMH {@code @State(Scope.Thread)} isolates all benchmark state
 * objects per worker thread.
 *
 * <p><b>Memory Consistency:</b> JMH executes {@code @Setup(Level.Trial)} before measurement
 * iterations on the benchmark thread.
 */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 2, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 3, time = 1, timeUnit = TimeUnit.SECONDS)
@Fork(
    value = 1,
    jvmArgsAppend = {"-Xms4g", "-Xmx4g", "-XX:+AlwaysPreTouch", "-XX:+UseG1GC"})
public class JavaHotPathBenchmark {

  private static final int RING_SIZE = 4096;
  private static final int RING_MASK = RING_SIZE - 1;
  private static final int BOOK_ORDERS = 4_000_000;

  private OrderEvent[] monoRing;
  private OrderEvent[] bimodalRing;
  private OrderEvent[] megaRing;
  private int cursor;

  private List<OrderRecord> freshArrayListBook;
  private List<OrderRecord> churnedArrayListBook;
  private OrderBookLayouts.PrimitiveSoABook primitiveSoABook;

  /**
   * Initializes the Battle 1 polymorphic dispatch rings (`4,096` events) and Battle 2 order books
   * (`4,000,000` orders) prior to JMH trial execution.
   */
  @Setup(Level.Trial)
  public void setUp() {
    monoRing = new OrderEvent[RING_SIZE];
    bimodalRing = new OrderEvent[RING_SIZE];
    megaRing = new OrderEvent[RING_SIZE];

    for (int i = 0; i < RING_SIZE; i++) {
      long price = 100L + (i & 63);
      long qty = 10L + (i & 15);
      monoRing[i] = new LimitOrderEvent(price, qty);
      bimodalRing[i] = OrderEvent.makeEvent(i & 1, price, qty);
      megaRing[i] = OrderEvent.makeEvent(i % 5, price, qty);
    }
    cursor = 0;

    freshArrayListBook = OrderBookLayouts.createFreshArrayListBook(BOOK_ORDERS);
    churnedArrayListBook = OrderBookLayouts.createChurnedArrayListBook(BOOK_ORDERS);
    primitiveSoABook = new OrderBookLayouts.PrimitiveSoABook(BOOK_ORDERS);
  }

  /**
   * Battle 1.1: Monomorphic dispatch (`100% LimitOrderEvent`) with {@code new OrderResult(...)}
   * allocation eliminated by C2 Escape Analysis & Scalar Replacement.
   *
   * @return Net notional exposure for one event.
   */
  @Benchmark
  public long BM_Battle1_Java21_Monomorphic_EscapeAnalysis() {
    int idx = (cursor++) & RING_MASK;
    return OrderEvent.runBattle1Step(monoRing[idx]);
  }

  /**
   * Battle 1.2: Bimodal dispatch (`50% LimitOrderEvent`, `50% CancelOrderEvent`) resolved via C2
   * two-receiver Polymorphic Inline Cache (PIC).
   *
   * @return Net notional exposure for one event.
   */
  @Benchmark
  public long BM_Battle1_Java21_Bimodal_TwoReceiverPIC() {
    int idx = (cursor++) & RING_MASK;
    return OrderEvent.runBattle1Step(bimodalRing[idx]);
  }

  /**
   * Battle 1.3: Megamorphic dispatch (`20% x 5` concrete subclasses), exceeding
   * `TypeProfileWidth=2` and forcing virtual interface dispatch (`vtable`/`itable` lookup).
   *
   * @return Net notional exposure for one event.
   */
  @Benchmark
  public long BM_Battle1_Java21_Megamorphic_VirtualCall() {
    int idx = (cursor++) & RING_MASK;
    return OrderEvent.runBattle1Step(megaRing[idx]);
  }

  /**
   * Battle 1.4: Fresh {@link OrderEvent} + {@link OrderResult} allocation inside the hot loop,
   * testing C2 Escape Analysis and TLAB pointer-bump allocation speed.
   *
   * @return Net notional exposure for the newly allocated event.
   */
  @Benchmark
  public long BM_Battle1_Java21_FreshEventAndResultAlloc() {
    int i = cursor++;
    OrderEvent ev = OrderEvent.makeEvent(0, 100L + (i & 63), 10L + (i & 15));
    return OrderEvent.runBattle1Step(ev);
  }

  /**
   * Battle 2.1: Full scan of 4,000,000 orders in a freshly allocated {@code ArrayList<OrderRecord>}
   * where TLAB allocation order happens to be sequential in virtual memory.
   *
   * @return Accumulated active notional exposure.
   */
  @Benchmark
  public long BM_Battle2_Java21_ArrayList_FreshTLAB_4M() {
    return OrderBookLayouts.scanArrayListOrders(freshArrayListBook);
  }

  /**
   * Battle 2.2: Full scan of 4,000,000 orders in a post-churn {@code ArrayList<OrderRecord>} where
   * object references jump randomly across heap pages, triggering L1D, L2, LLC, and dTLB stalls.
   *
   * @return Accumulated active notional exposure.
   */
  @Benchmark
  public long BM_Battle2_Java21_ArrayList_ChurnedHeap_4M() {
    return OrderBookLayouts.scanArrayListOrders(churnedArrayListBook);
  }

  /**
   * Battle 2.3: Full scan of 4,000,000 orders stored in flat primitive {@code long[]} Structure of
   * Arrays (SoA), allowing C2 to emit AVX2/AVX-512 SIMD vector loops.
   *
   * @return Accumulated active notional exposure.
   */
  @Benchmark
  public long BM_Battle2_Java21_PrimitiveSoA_FlatArrays_4M() {
    return OrderBookLayouts.scanPrimitiveSoA(
        primitiveSoABook.prices(),
        primitiveSoABook.qtys(),
        primitiveSoABook.masks(),
        primitiveSoABook.size());
  }
}
