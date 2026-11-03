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
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

/**
 * Standalone forensic telemetry and hardware PMU (`perf stat`) target for Episode 13 ("Is C++
 * Actually Faster Than Java?").
 *
 * <p>Supports isolated CLI modes via {@code --mode=battle1|battle2_fresh|battle2_churned|
 * battle2_soa|battle3|all} so Linux {@code perf stat} can attach to individual Battle 2 memory
 * layouts and Battle 3 GC configurations without cross-contamination.
 *
 * <p><b>Thread Safety Guarantee:</b> Single-threaded deterministic forensic runner.
 *
 * <p><b>Memory Consistency:</b> Uses a {@code volatile} blackhole accumulator to prevent C2
 * dead-code elimination across benchmark loops.
 */
public final class JavaForensicChallenge {

  private static final Logger tableLogger = LogManager.getLogger("com.hotpath.episode13.table");
  private static volatile long blackhole = 0L;

  private JavaForensicChallenge() {}

  /**
   * Executes Battle 1: TLAB + Escape Analysis & C2 Devirtualization across Monomorphic, Bimodal,
   * Megamorphic, and Fresh-Allocation loops.
   */
  public static void runBattle1Suite() {
    tableLogger.info("====================================================================");
    tableLogger.info("JAVA 21 BATTLE 1: TLAB + ESCAPE ANALYSIS & C2 DEVIRTUALIZATION");
    tableLogger.info("====================================================================");

    final int batchSize = 4096;
    OrderEvent[] monoRing = new OrderEvent[batchSize];
    OrderEvent[] bimodalRing = new OrderEvent[batchSize];
    OrderEvent[] megaRing = new OrderEvent[batchSize];

    for (int i = 0; i < batchSize; i++) {
      monoRing[i] = new LimitOrderEvent(100L + (i & 63), 10L + (i & 15));
      bimodalRing[i] = OrderEvent.makeEvent(i & 1, 100L + (i & 63), 10L + (i & 15));
      megaRing[i] = OrderEvent.makeEvent(i % 5, 100L + (i & 63), 10L + (i & 15));
    }

    benchBattle1Variant("1. Java 21 C2 Monomorphic (100% LimitOrder + new OrderResult)", monoRing);
    benchBattle1Variant(
        "2. Java 21 C2 Bimodal (50/50 2 Subclasses + new OrderResult)", bimodalRing);
    benchBattle1Variant("3. Java 21 C2 Megamorphic (20%x5 Subclasses + new OrderResult)", megaRing);
    benchBattle1FreshAlloc();
  }

  private static void benchBattle1Variant(String label, OrderEvent[] ring) {
    final int warmupOps = 20_000_000;
    final int measureOps = 100_000_000;
    long acc = 0L;
    for (int i = 0; i < warmupOps; i++) {
      acc += OrderEvent.runBattle1Step(ring[i & 4095]);
    }
    blackhole ^= acc;

    long t0 = System.nanoTime();
    for (int i = 0; i < measureOps; i++) {
      acc += OrderEvent.runBattle1Step(ring[i & 4095]);
    }
    long t1 = System.nanoTime();
    blackhole ^= acc;

    double nsPerOp = (double) (t1 - t0) / measureOps;
    double mOpsPerSec = 1000.0 / nsPerOp;
    tableLogger.info(
        String.format("  %-58s : %6.2f ns/op (%6.1f M ops/s)", label, nsPerOp, mOpsPerSec));
  }

  private static void benchBattle1FreshAlloc() {
    final int warmupOps = 20_000_000;
    final int measureOps = 100_000_000;
    long acc = 0L;
    for (int i = 0; i < warmupOps; i++) {
      OrderEvent ev = OrderEvent.makeEvent(0, 100L + (i & 63), 10L + (i & 15));
      acc += OrderEvent.runBattle1Step(ev);
    }
    blackhole ^= acc;

    long t0 = System.nanoTime();
    for (int i = 0; i < measureOps; i++) {
      OrderEvent ev = OrderEvent.makeEvent(0, 100L + (i & 63), 10L + (i & 15));
      acc += OrderEvent.runBattle1Step(ev);
    }
    long t1 = System.nanoTime();
    blackhole ^= acc;

    double nsPerOp = (double) (t1 - t0) / measureOps;
    double mOpsPerSec = 1000.0 / nsPerOp;
    tableLogger.info(
        String.format(
            "  %-58s : %6.2f ns/op (%6.1f M ops/s)",
            "4. Java 21 Fresh Event + Result Alloc (Monomorphic)", nsPerOp, mOpsPerSec));
  }

  /**
   * Executes Battle 2: Cache-Line Physics across 4,000,000 orders for the requested layout variant.
   *
   * @param onlyVariant {@code "fresh"}, {@code "churned"}, {@code "soa"}, or {@code "all"}.
   */
  public static void runBattle2Suite(String onlyVariant) {
    final int orderCount = 4_000_000;
    tableLogger.info("");
    tableLogger.info("====================================================================");
    tableLogger.info("JAVA 21 BATTLE 2: CACHE-LINE PHYSICS (4,000,000 Orders)");
    tableLogger.info("====================================================================");

    if (onlyVariant.equals("all") || onlyVariant.equals("fresh")) {
      List<OrderRecord> freshOrders = OrderBookLayouts.createFreshArrayListBook(orderCount);
      benchBattle2ArrayList("1. Java ArrayList<Order> (Fresh Sequential TLAB Layout)", freshOrders);
    }

    if (onlyVariant.equals("all") || onlyVariant.equals("churned")) {
      List<OrderRecord> churnedOrders = OrderBookLayouts.createChurnedArrayListBook(orderCount);
      benchBattle2ArrayList("2. Java ArrayList<Order> (Post-Churn Fragmented Heap)", churnedOrders);
    }

    if (onlyVariant.equals("all") || onlyVariant.equals("soa")) {
      OrderBookLayouts.PrimitiveSoABook soaBook = new OrderBookLayouts.PrimitiveSoABook(orderCount);
      benchBattle2SoA(
          "3. Java Flat Primitive Arrays (long[] SoA Layout)",
          soaBook.prices(),
          soaBook.qtys(),
          soaBook.masks(),
          soaBook.size());
    }
  }

  private static void benchBattle2ArrayList(String label, List<OrderRecord> orders) {
    for (int w = 0; w < 15; w++) {
      blackhole ^= OrderBookLayouts.scanArrayListOrders(orders);
    }
    final int passes = 40;
    long t0 = System.nanoTime();
    for (int p = 0; p < passes; p++) {
      blackhole ^= OrderBookLayouts.scanArrayListOrders(orders);
    }
    long t1 = System.nanoTime();
    double msPerPass = ((double) (t1 - t0) / passes) / 1_000_000.0;
    double nsPerOrder = ((double) (t1 - t0) / passes) / orders.size();
    tableLogger.info(
        String.format("  %-55s : %7.3f ms / scan (%5.2f ns/order)", label, msPerPass, nsPerOrder));
  }

  private static void benchBattle2SoA(
      String label, long[] prices, long[] qtys, long[] masks, int n) {
    for (int w = 0; w < 15; w++) {
      blackhole ^= OrderBookLayouts.scanPrimitiveSoA(prices, qtys, masks, n);
    }
    final int passes = 40;
    long t0 = System.nanoTime();
    for (int p = 0; p < passes; p++) {
      blackhole ^= OrderBookLayouts.scanPrimitiveSoA(prices, qtys, masks, n);
    }
    long t1 = System.nanoTime();
    double msPerPass = ((double) (t1 - t0) / passes) / 1_000_000.0;
    double nsPerOrder = ((double) (t1 - t0) / passes) / n;
    tableLogger.info(
        String.format("  %-55s : %7.3f ms / scan (%5.2f ns/order)", label, msPerPass, nsPerOrder));
  }

  /**
   * Executes Battle 3: Tail Latency under G1GC or Generational ZGC + C2 Uncommon Trap
   * Deoptimization spike.
   */
  public static void runBattle3Suite() {
    StreamingStateEngine.TelemetryReport report =
        StreamingStateEngine.runBattle3(
            StreamingStateEngine.DEFAULT_CAPACITY,
            StreamingStateEngine.DEFAULT_BATCHES,
            StreamingStateEngine.DEFAULT_BATCH_MSGS);
    blackhole ^= report.checksum();

    tableLogger.info("");
    tableLogger.info("====================================================================");
    tableLogger.info("JAVA 21 BATTLE 3: TAIL LATENCY & DEOPT (" + report.gcName() + ")");
    tableLogger.info("====================================================================");
    tableLogger.info(String.format("  p50.00 Latency : %8.3f us", report.p50Us()));
    tableLogger.info(String.format("  p99.00 Latency : %8.3f us", report.p99Us()));
    tableLogger.info(String.format("  p99.90 Latency : %8.3f us", report.p999Us()));
    tableLogger.info(String.format("  p99.99 Latency : %8.3f us", report.p9999Us()));
    tableLogger.info(String.format("  Max    Latency : %8.3f us", report.maxUs()));
    tableLogger.info(
        String.format(
            "  1st Rare MarketHaltEvent Spike (Uncommon Trap) : %d ns",
            report.firstRarePacketNs()));
    tableLogger.info(
        String.format(
            "  GC Collections : %d cycles (%d ms reported GC time)",
            report.gcCount(), report.gcTimeMs()));
    tableLogger.info("====================================================================");
  }

  /**
   * CLI entry point supporting {@code --mode=battle1|battle2_fresh|battle2_churned|battle2_soa|
   * battle3|all}.
   *
   * @param args Command-line arguments.
   */
  public static void main(String[] args) {
    String mode = "all";
    for (String arg : args) {
      if (arg.startsWith("--mode=")) {
        mode = arg.substring("--mode=".length());
      }
    }

    switch (mode) {
      case "battle1" -> runBattle1Suite();
      case "battle2_fresh" -> runBattle2Suite("fresh");
      case "battle2_churned" -> runBattle2Suite("churned");
      case "battle2_soa" -> runBattle2Suite("soa");
      case "battle3" -> runBattle3Suite();
      default -> {
        runBattle1Suite();
        runBattle2Suite("all");
        runBattle3Suite();
      }
    }
  }
}
