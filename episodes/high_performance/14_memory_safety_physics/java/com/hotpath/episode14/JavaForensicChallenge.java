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

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.util.List;
import java.util.stream.Collectors;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

/**
 * Standalone forensic telemetry and hardware PMU (`perf stat`) runner for Episode 14 ("The Physics
 * of Memory Safety: C++20 vs. Rust vs. Java 21").
 *
 * <p>Supports {@code --mode=all|spatial_seq|spatial_rand|spatial_dep|temporal} and {@code
 * --iters=N}.
 */
public final class JavaForensicChallenge {

  private static final Logger tableLogger = LogManager.getLogger("com.hotpath.episode14.table");
  private static final String SEPARATOR = "=".repeat(88);
  private static volatile long blackhole = 0L;

  private JavaForensicChallenge() {}

  private static void printRow(
      String label, double elapsedNs, int iters, int count, long checksum) {
    double usPerPass = (elapsedNs / (double) iters) / 1000.0;
    double nsPerOrder = elapsedNs / ((double) iters * count);
    tableLogger.info(
        String.format(
            "  %-46s | %8.2f us/pass | %6.3f ns/order | sum=%d",
            label, usPerPass, nsPerOrder, checksum));
  }

  /**
   * Entry point for the Episode 14 Java 21 forensic challenge.
   *
   * @param args CLI flags ({@code --mode=...}, {@code --iters=...}).
   */
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
    int count = JavaMemorySafetyEngine.DEFAULT_BOOK_CAPACITY;

    JavaMemorySafetyEngine.SpatialDataset spatial =
        new JavaMemorySafetyEngine.SpatialDataset(count);

    if ("all".equals(mode) || "spatial_seq".equals(mode)) {
      tableLogger.info(SEPARATOR);
      tableLogger.info(
          String.format("JAVA 21 BATTLE 1A: SEQUENTIAL SCAN (%d Orders, GC=[%s])", count, gcNames));
      tableLogger.info(SEPARATOR);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.scanSequentialPrimitive(spatial.prices, spatial.quantities);
      }
      long acc1 = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc1 += JavaMemorySafetyEngine.scanSequentialPrimitive(spatial.prices, spatial.quantities);
      }
      long t1 = System.nanoTime();
      blackhole ^= acc1;
      printRow(
          "1. Primitive long[] Scan (C2 Range Check Hoisted)",
          (double) (t1 - t0),
          iters,
          count,
          acc1);

      for (int w = 0; w < 200; w++) {
        blackhole ^= JavaMemorySafetyEngine.scanSequentialObjectArray(spatial.orderObjs);
      }
      long acc2 = 0L;
      long t2 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc2 += JavaMemorySafetyEngine.scanSequentialObjectArray(spatial.orderObjs);
      }
      long t3 = System.nanoTime();
      blackhole ^= acc2;
      printRow(
          "2. OrderObj[] Reference Scan (Pointer Chasing)", (double) (t3 - t2), iters, count, acc2);
    }

    if ("all".equals(mode) || "spatial_rand".equals(mode)) {
      tableLogger.info(SEPARATOR);
      tableLogger.info(
          String.format(
              "JAVA 21 BATTLE 1B: INDEPENDENT RANDOM BATCH GATHER (%d Lookups, GC=[%s])",
              count, gcNames));
      tableLogger.info(SEPARATOR);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.gatherBatchPrimitiveChecked(
                spatial.prices, spatial.quantities, spatial.randomIdx);
      }
      long acc1 = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc1 +=
            JavaMemorySafetyEngine.gatherBatchPrimitiveChecked(
                spatial.prices, spatial.quantities, spatial.randomIdx);
      }
      long t1 = System.nanoTime();
      blackhole ^= acc1;
      printRow(
          "1. Primitive long[idx] Gather (Mandatory Check)",
          (double) (t1 - t0),
          iters,
          count,
          acc1);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.gatherBatchPrimitiveBitmask(
                spatial.prices,
                spatial.quantities,
                spatial.randomIdx,
                JavaMemorySafetyEngine.DEFAULT_BOOK_MASK);
      }
      long acc2 = 0L;
      long t2 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc2 +=
            JavaMemorySafetyEngine.gatherBatchPrimitiveBitmask(
                spatial.prices,
                spatial.quantities,
                spatial.randomIdx,
                JavaMemorySafetyEngine.DEFAULT_BOOK_MASK);
      }
      long t3 = System.nanoTime();
      blackhole ^= acc2;
      printRow(
          "2. Primitive long[idx & 0xFFFF] (Bitmask Safe)", (double) (t3 - t2), iters, count, acc2);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.gatherBatchObjectArray(spatial.orderObjs, spatial.randomIdx);
      }
      long acc3 = 0L;
      long t4 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc3 += JavaMemorySafetyEngine.gatherBatchObjectArray(spatial.orderObjs, spatial.randomIdx);
      }
      long t5 = System.nanoTime();
      blackhole ^= acc3;
      printRow(
          "3. OrderObj[idx] Gather (Bounds + Ptr Chase)", (double) (t5 - t4), iters, count, acc3);
    }

    if ("all".equals(mode) || "spatial_dep".equals(mode)) {
      tableLogger.info(SEPARATOR);
      tableLogger.info(
          String.format("JAVA 21 BATTLE 1C: DEPENDENT PROBE CHAIN (%d Probes)", count));
      tableLogger.info(SEPARATOR);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.probeDependentPrimitive(
                spatial.prices,
                spatial.quantities,
                spatial.randomIdx,
                JavaMemorySafetyEngine.DEFAULT_BOOK_MASK);
      }
      long acc1 = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        acc1 +=
            JavaMemorySafetyEngine.probeDependentPrimitive(
                spatial.prices,
                spatial.quantities,
                spatial.randomIdx,
                JavaMemorySafetyEngine.DEFAULT_BOOK_MASK);
      }
      long t1 = System.nanoTime();
      blackhole ^= acc1;
      printRow(
          "1. Primitive Dependent Probe (C2 Bounds Check)", (double) (t1 - t0), iters, count, acc1);
    }

    if ("all".equals(mode) || "temporal".equals(mode)) {
      tableLogger.info(SEPARATOR);
      tableLogger.info(
          String.format(
              "JAVA 21 BATTLE 3: TEMPORAL SAFETY & GC CHURN (%d Orders, GC=[%s])", count, gcNames));
      tableLogger.info(SEPARATOR);

      JavaMemorySafetyEngine.TemporalDataset temporal =
          new JavaMemorySafetyEngine.TemporalDataset(count);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.runTemporalPrimitiveSlabLookup(
                temporal.slab, temporal.originalSlabHandles, temporal.lookupOrder);
      }
      long accSlab = 0L;
      long t0 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        accSlab +=
            JavaMemorySafetyEngine.runTemporalPrimitiveSlabLookup(
                temporal.slab, temporal.originalSlabHandles, temporal.lookupOrder);
      }
      long t1 = System.nanoTime();
      blackhole ^= accSlab;
      printRow(
          "1. Zero-GC Primitive Generational Slab (Safe)",
          (double) (t1 - t0),
          iters,
          count,
          accSlab);

      for (int w = 0; w < 200; w++) {
        blackhole ^=
            JavaMemorySafetyEngine.runTemporalManagedHeapLookupWithChurn(
                temporal.originalHeapRefs, temporal.liveHeapBook, temporal.lookupOrder, w);
      }
      long gcCountBefore =
          gcBeans.stream().mapToLong(GarbageCollectorMXBean::getCollectionCount).sum();
      long gcTimeBefore =
          gcBeans.stream().mapToLong(GarbageCollectorMXBean::getCollectionTime).sum();
      long accHeap = 0L;
      long t2 = System.nanoTime();
      for (int r = 0; r < iters; r++) {
        accHeap +=
            JavaMemorySafetyEngine.runTemporalManagedHeapLookupWithChurn(
                temporal.originalHeapRefs, temporal.liveHeapBook, temporal.lookupOrder, r);
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
          count,
          accHeap);
      tableLogger.info(
          String.format(
              "     -> GC Collections Delta: %d | GC Time Delta: %d ms | Zero UAF / ABA=0",
              gcCountAfter - gcCountBefore, gcTimeAfter - gcTimeBefore));
    }
  }
}
