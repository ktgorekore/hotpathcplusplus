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

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.util.Arrays;
import java.util.List;

/**
 * Battle 3 streaming state-mutation and C2 Uncommon Trap deoptimization harness.
 *
 * <p>Maintains a live working set of {@code 2,097,152} {@link OrderRecord} references while
 * mutating slots in batches of 256 messages (`50,000` batches = `12.8M` heap mutations). This
 * measures:
 *
 * <ul>
 *   <li><b>Part A (C2 Uncommon Trap Spike):</b> After training {@link
 *       OrderEvent#runBattle1Step(OrderEvent)} for 200,000 iterations exclusively on {@link
 *       LimitOrderEvent}, injecting a single rare {@link MarketHaltEvent} triggers a C2 class-check
 *       Uncommon Trap (`Deoptimization::uncommon_trap`), tearing down the compiled frame to the
 *       interpreter.
 *   <li><b>Part B (GC Tail Latency):</b> Measures per-batch latency percentiles (`p50`, `p99`,
 *       `p99.9`, `p99.99`, `Max`) under OpenJDK 21 G1GC (Stop-The-World evacuation pauses) vs.
 *       Generational ZGC (`-XX:+UseZGC -XX:+ZGenerational`, concurrent colored-pointer load
 *       barriers and allocation stalls under tight heap headroom).
 * </ul>
 *
 * <p><b>Thread Safety Guarantee:</b> {@link TelemetryReport} is immutable and thread-safe. The
 * benchmark execution method creates thread-local state per invocation.
 *
 * <p><b>Memory Consistency:</b> Safe publication of {@link TelemetryReport} via {@code final}
 * fields (JLS 17.5).
 */
public final class StreamingStateEngine {

  /** Default live order book capacity (2,097,152 orders, power of two). */
  public static final int DEFAULT_CAPACITY = 1 << 21;

  /** Default number of batches in the full Battle 3 run (50,000 batches). */
  public static final int DEFAULT_BATCHES = 50_000;

  /** Default number of order mutations per batch (256 messages). */
  public static final int DEFAULT_BATCH_MSGS = 256;

  private static volatile long blackhole = 0L;

  private StreamingStateEngine() {}

  /**
   * Immutable telemetry summary produced by a Battle 3 run.
   *
   * <p><b>Thread Safety Guarantee:</b> Unconditionally thread-safe (`final` primitive/String
   * fields).
   */
  public static final class TelemetryReport {
    private final String gcName;
    private final double p50Us;
    private final double p99Us;
    private final double p999Us;
    private final double p9999Us;
    private final double maxUs;
    private final long firstRarePacketNs;
    private final long gcCount;
    private final long gcTimeMs;
    private final long checksum;

    /**
     * Constructs a {@code TelemetryReport}.
     *
     * @param gcName Active JVM Garbage Collector names reported by {@link GarbageCollectorMXBean}.
     * @param p50Us 50th percentile batch latency in microseconds.
     * @param p99Us 99th percentile batch latency in microseconds.
     * @param p999Us 99.9th percentile batch latency in microseconds.
     * @param p9999Us 99.99th percentile batch latency in microseconds.
     * @param maxUs Maximum (worst-case) batch latency in microseconds.
     * @param firstRarePacketNs Latency in nanoseconds of the first rare {@link MarketHaltEvent}
     *     that triggers the C2 Uncommon Trap.
     * @param gcCount Total GC collection count observed during the run.
     * @param gcTimeMs Total GC pause/collection time in milliseconds reported by JMX.
     * @param checksum Accumulated notional checksum preventing dead-code elimination.
     */
    public TelemetryReport(
        String gcName,
        double p50Us,
        double p99Us,
        double p999Us,
        double p9999Us,
        double maxUs,
        long firstRarePacketNs,
        long gcCount,
        long gcTimeMs,
        long checksum) {
      this.gcName = gcName;
      this.p50Us = p50Us;
      this.p99Us = p99Us;
      this.p999Us = p999Us;
      this.p9999Us = p9999Us;
      this.maxUs = maxUs;
      this.firstRarePacketNs = firstRarePacketNs;
      this.gcCount = gcCount;
      this.gcTimeMs = gcTimeMs;
      this.checksum = checksum;
    }

    /** Returns the active GC collector names. */
    public String gcName() {
      return gcName;
    }

    /** Returns p50 batch latency in microseconds. */
    public double p50Us() {
      return p50Us;
    }

    /** Returns p99 batch latency in microseconds. */
    public double p99Us() {
      return p99Us;
    }

    /** Returns p99.9 batch latency in microseconds. */
    public double p999Us() {
      return p999Us;
    }

    /** Returns p99.99 batch latency in microseconds. */
    public double p9999Us() {
      return p9999Us;
    }

    /** Returns maximum batch latency in microseconds. */
    public double maxUs() {
      return maxUs;
    }

    /** Returns the first rare {@link MarketHaltEvent} Uncommon Trap latency in nanoseconds. */
    public long firstRarePacketNs() {
      return firstRarePacketNs;
    }

    /** Returns total GC collection cycles observed during the benchmark. */
    public long gcCount() {
      return gcCount;
    }

    /** Returns total GC time in milliseconds reported by {@link GarbageCollectorMXBean}. */
    public long gcTimeMs() {
      return gcTimeMs;
    }

    /** Returns the accumulated notional checksum. */
    public long checksum() {
      return checksum;
    }
  }

  /**
   * Isolated hot method trained exclusively on {@link LimitOrderEvent} in Part A of Battle 3 so C2
   * compiles it with a speculative monomorphic class guard before the first {@link MarketHaltEvent}
   * arrives.
   *
   * @param event The order event to evaluate.
   * @return The net notional value.
   */
  public static long dispatchSpeculativeEvent(OrderEvent event) {
    OrderResult res = event.processHeapAlloc();
    return res.notional() - res.feeTicks();
  }

  /**
   * Executes Battle 3 (Part A: C2 Uncommon Trap Deoptimization spike; Part B: Streaming state table
   * mutation under GC pressure) and returns a {@link TelemetryReport}.
   *
   * @param capacity Power-of-two capacity of the live order table (e.g., {@code 2,097,152}).
   * @param numBatches Number of batches to execute (e.g., {@code 50,000}).
   * @param batchMsgs Number of order mutations per batch (e.g., {@code 256}).
   * @return Populated {@link TelemetryReport}.
   * @throws IllegalArgumentException If {@code capacity} is not a positive power of two or {@code
   *     numBatches <= 0}.
   */
  public static TelemetryReport runBattle3(int capacity, int numBatches, int batchMsgs) {
    if (capacity <= 0 || (capacity & (capacity - 1)) != 0) {
      throw new IllegalArgumentException("capacity must be a positive power of two: " + capacity);
    }
    if (numBatches <= 0 || batchMsgs <= 0) {
      throw new IllegalArgumentException("numBatches and batchMsgs must be positive");
    }

    List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
    String gcNames =
        gcBeans.stream()
            .map(GarbageCollectorMXBean::getName)
            .reduce((a, b) -> a + ", " + b)
            .orElse("Unknown");

    // Part A: C2 JIT Uncommon Trap (Deoptimization) Measurement
    OrderEvent warmLimit = new LimitOrderEvent(105L, 20L);
    long sum = 0L;
    for (int i = 0; i < 200_000; i++) {
      sum += dispatchSpeculativeEvent(warmLimit);
    }
    blackhole ^= sum;

    OrderEvent rareHalt = new MarketHaltEvent(105L, 20L, 42);
    long d0 = System.nanoTime();
    sum += dispatchSpeculativeEvent(rareHalt);
    long d1 = System.nanoTime();
    blackhole ^= sum;
    long firstRarePacketNs = d1 - d0;

    // Record baseline GC counts before Part B state-table mutation
    long baseGcCount = 0L;
    long baseGcTimeMs = 0L;
    for (GarbageCollectorMXBean bean : gcBeans) {
      if (bean.getCollectionCount() > 0) {
        baseGcCount += bean.getCollectionCount();
      }
      if (bean.getCollectionTime() > 0) {
        baseGcTimeMs += bean.getCollectionTime();
      }
    }

    // Part B: Sustained State Table Mutation
    OrderRecord[] liveBook = new OrderRecord[capacity];
    for (int i = 0; i < capacity; i++) {
      liveBook[i] = new OrderRecord(i, 1000L + (i & 63), 10L, i & 255, 1);
    }

    long[] batchLatenciesNs = new long[numBatches];
    long cursor = 0L;
    long checksum = 0L;
    final int mask = capacity - 1;

    for (int b = 0; b < numBatches; b++) {
      long t0 = System.nanoTime();
      for (int m = 0; m < batchMsgs; m++) {
        int slot = (int) ((cursor++) & mask);
        OrderRecord incoming =
            new OrderRecord(cursor, 1000L + (m & 63), 10L + (m & 15), b & 255, 1);
        liveBook[slot] = incoming;

        int matchSlot = (slot + 1024) & mask;
        OrderRecord resting = liveBook[matchSlot];
        checksum += resting.price() * incoming.qty();
      }
      long t1 = System.nanoTime();
      batchLatenciesNs[b] = t1 - t0;
    }
    blackhole ^= checksum;

    Arrays.sort(batchLatenciesNs);
    double p50 = batchLatenciesNs[(int) (numBatches * 0.50)] / 1000.0;
    double p99 = batchLatenciesNs[(int) (numBatches * 0.99)] / 1000.0;
    double p999 = batchLatenciesNs[(int) (numBatches * 0.999)] / 1000.0;
    double p9999 = batchLatenciesNs[(int) (numBatches * 0.9999)] / 1000.0;
    double maxUs = batchLatenciesNs[numBatches - 1] / 1000.0;

    long endGcCount = 0L;
    long endGcTimeMs = 0L;
    for (GarbageCollectorMXBean bean : gcBeans) {
      if (bean.getCollectionCount() > 0) {
        endGcCount += bean.getCollectionCount();
      }
      if (bean.getCollectionTime() > 0) {
        endGcTimeMs += bean.getCollectionTime();
      }
    }

    return new TelemetryReport(
        gcNames,
        p50,
        p99,
        p999,
        p9999,
        maxUs,
        firstRarePacketNs,
        Math.max(0L, endGcCount - baseGcCount),
        Math.max(0L, endGcTimeMs - baseGcTimeMs),
        checksum ^ blackhole);
  }
}
