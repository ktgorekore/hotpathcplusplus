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

package com.hotpath.episode15;

import it.unimi.dsi.fastutil.longs.Long2LongOpenHashMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import java.lang.ref.Reference;
import java.util.HashMap;

/**
 * Standalone Java 21 forensic runner and Linux {@code perf stat} target for Episode 15.
 */
public final class JavaForensicChallenge {

  public static volatile long globalSink;
  public static volatile long[] volatileQueries;

  private JavaForensicChallenge() {}

  private static void printRow(String label, double totalMs, long totalOps, long checksum) {
    double nsPerOp = (totalMs * 1_000_000.0) / totalOps;
    double mops = totalOps / (totalMs * 1_000.0);
    System.out.printf(
        "%-42s | %8.2f ns/op | %7.2f M ops/s | checksum=0x%s%n",
        label, nsPerOp, mops, Long.toHexString(checksum));
  }

  private static void runL1Jdk(int iters) {
    var ds =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_L1_ENTRIES, HashMapForensicEngine.NUM_QUERIES);
    var map = HashMapForensicEngine.buildAgedJdkPacked(ds);
    volatileQueries = ds.queries8020;
    long sink = 0L;
    for (int w = 0; w < 10; w++) {
      sink += HashMapForensicEngine.lookupJdkPacked(map, volatileQueries);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.lookupJdkPacked(map, volatileQueries);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    printRow("Java 21 HashMap<Long,Long> (L1 1K 80/20)", ms, (long) iters * ds.queries8020.length, sink);
  }

  private static void runL1Fastutil(int iters) {
    var ds =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_L1_ENTRIES, HashMapForensicEngine.NUM_QUERIES);
    var map = HashMapForensicEngine.buildAgedFastutilPacked(ds);
    volatileQueries = ds.queries8020;
    long sink = 0L;
    for (int w = 0; w < 10; w++) {
      sink += HashMapForensicEngine.lookupFastutilPacked(map, volatileQueries);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.lookupFastutilPacked(map, volatileQueries);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    printRow("Java 21 fastutil Long2Long (L1 1K 80/20)", ms, (long) iters * ds.queries8020.length, sink);
  }

  private static void runAgedJdk(int iters, boolean missOnly) {
    var ds =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_AGED_ENTRIES, HashMapForensicEngine.NUM_QUERIES);
    var map = HashMapForensicEngine.buildAgedJdkPacked(ds);
    volatileQueries = missOnly ? ds.queries100Miss : ds.queries8020;
    long sink = 0L;
    for (int w = 0; w < 3; w++) {
      sink += HashMapForensicEngine.lookupJdkPacked(map, volatileQueries);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.lookupJdkPacked(map, volatileQueries);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    String label =
        missOnly
            ? "Java 21 HashMap<Long,Long> (Aged 2M Miss)"
            : "Java 21 HashMap<Long,Long> (Aged 2M 80/20)";
    printRow(label, ms, (long) iters * volatileQueries.length, sink);
  }

  private static void runAgedFastutil(int iters, boolean missOnly) {
    var ds =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_AGED_ENTRIES, HashMapForensicEngine.NUM_QUERIES);
    var map = HashMapForensicEngine.buildAgedFastutilPacked(ds);
    volatileQueries = missOnly ? ds.queries100Miss : ds.queries8020;
    long sink = 0L;
    for (int w = 0; w < 3; w++) {
      sink += HashMapForensicEngine.lookupFastutilPacked(map, volatileQueries);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.lookupFastutilPacked(map, volatileQueries);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    String label =
        missOnly
            ? "Java 21 fastutil Long2Long (Aged 2M Miss)"
            : "Java 21 fastutil Long2Long (Aged 2M 80/20)";
    printRow(label, ms, (long) iters * volatileQueries.length, sink);
  }

  private static void runAgedSessionJdk(int iters) {
    var ds =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_AGED_ENTRIES, HashMapForensicEngine.NUM_QUERIES);
    var map = HashMapForensicEngine.buildAgedJdkSession(ds);
    volatileQueries = ds.queries8020;
    long sink = 0L;
    for (int w = 0; w < 3; w++) {
      sink += HashMapForensicEngine.lookupJdkSession(map, volatileQueries);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.lookupJdkSession(map, volatileQueries);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    printRow("Java 21 HashMap<Long,Session> (Aged 24B)", ms, (long) iters * ds.queries8020.length, sink);
  }

  private static void runAgedSessionFastutil(int iters) {
    var ds =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_AGED_ENTRIES, HashMapForensicEngine.NUM_QUERIES);
    var map = HashMapForensicEngine.buildAgedFastutilSession(ds);
    volatileQueries = ds.queries8020;
    long sink = 0L;
    for (int w = 0; w < 3; w++) {
      sink += HashMapForensicEngine.lookupFastutilSession(map, volatileQueries);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.lookupFastutilSession(map, volatileQueries);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    printRow(
        "Java 21 fastutil Long2Object (Aged 24B)", ms, (long) iters * ds.queries8020.length, sink);
  }

  private static void printSweepRow(
      String label, double totalMs, int sweeps, int liveEntries, long checksum) {
    double msPerSweep = totalMs / sweeps;
    long totalLive = (long) sweeps * liveEntries;
    double nsPerLive = (totalMs * 1_000_000.0) / totalLive;
    double mops = totalLive / (totalMs * 1_000.0);
    System.out.printf(
        "%-42s | %6.2f ms/swp | %6.2f ns/live | %7.2f M/s | checksum=0x%s%n",
        label, msPerSweep, nsPerLive, mops, Long.toHexString(checksum));
  }

  private static void runIterJdk(int iters, boolean sparse) {
    var ds = HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_AGED_ENTRIES, 1024);
    var map =
        sparse
            ? HashMapForensicEngine.buildAgedJdkPackedSparse(ds)
            : HashMapForensicEngine.buildAgedJdkPacked(ds);
    int live = map.size();
    long sink = 0L;
    for (int w = 0; w < 3; w++) {
      sink += HashMapForensicEngine.iterJdkPacked(map);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.iterJdkPacked(map);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    printSweepRow(
        sparse ? "Java 21 HashMap<Long,Long> (Sparse 200K)" : "Java 21 HashMap<Long,Long> (Iter 2M Full)",
        ms,
        iters,
        live,
        sink);
  }

  private static void runIterFastutil(int iters, boolean sparse) {
    var ds = HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_AGED_ENTRIES, 1024);
    var map =
        sparse
            ? HashMapForensicEngine.buildAgedFastutilPackedSparse(ds)
            : HashMapForensicEngine.buildAgedFastutilPacked(ds);
    int live = map.size();
    long sink = 0L;
    for (int w = 0; w < 3; w++) {
      sink += HashMapForensicEngine.iterFastutilPacked(map);
    }
    long t0 = System.nanoTime();
    for (int i = 0; i < iters; i++) {
      sink += HashMapForensicEngine.iterFastutilPacked(map);
    }
    double ms = (System.nanoTime() - t0) / 1e6;
    globalSink = sink;
    printSweepRow(
        sparse
            ? "Java 21 fastutil Long2Long (Sparse 200K)"
            : "Java 21 fastutil Long2Long (Iter 2M Full)",
        ms,
        iters,
        live,
        sink);
  }

  private static void runBattle2Rss() {
    var ds = HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_AGED_ENTRIES, 1024);
    System.out.println("\n=== Java 21 Battle 2A: Live Heap Footprint (2,000,000 Entries) ===");

    long base = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    HashMap<Long, Long> jdkPacked = new HashMap<>((int) (ds.keys.length / 0.75) + 1);
    for (int i = 0; i < ds.keys.length; i++) {
      jdkPacked.put(ds.keys[i], ds.packedVals[i]);
    }
    long afterJdkP = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    Reference.reachabilityFence(jdkPacked);
    long deltaJdkP = Math.max(0L, afterJdkP - base);
    System.out.printf(
        "%-42s | Heap=%6.1f MiB (%5.1f B/entry)%n",
        "Java 21 HashMap<Long, Long> (2M)",
        deltaJdkP / (1024.0 * 1024.0),
        (double) deltaJdkP / ds.keys.length);
    jdkPacked = null;

    base = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    Long2LongOpenHashMap fuPacked = new Long2LongOpenHashMap(ds.keys.length);
    for (int i = 0; i < ds.keys.length; i++) {
      fuPacked.put(ds.keys[i], ds.packedVals[i]);
    }
    long afterFuP = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    Reference.reachabilityFence(fuPacked);
    long deltaFuP = Math.max(0L, afterFuP - base);
    System.out.printf(
        "%-42s | Heap=%6.1f MiB (%5.1f B/entry)%n",
        "Java 21 fastutil Long2LongOpenHashMap (2M)",
        deltaFuP / (1024.0 * 1024.0),
        (double) deltaFuP / ds.keys.length);
    fuPacked = null;

    base = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    HashMap<Long, SessionState> jdkSess = new HashMap<>((int) (ds.keys.length / 0.75) + 1);
    for (int i = 0; i < ds.keys.length; i++) {
      SessionState s = ds.sessionVals[i];
      jdkSess.put(
          ds.keys[i],
          new SessionState(s.quotaRemaining, s.lastSeenNs, s.tierFlags, s.rateWindowHits));
    }
    long afterJdkS = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    Reference.reachabilityFence(jdkSess);
    long deltaJdkS = Math.max(0L, afterJdkS - base);
    System.out.printf(
        "%-42s | Heap=%6.1f MiB (%5.1f B/entry)%n",
        "Java 21 HashMap<Long, SessionState> (2M)",
        deltaJdkS / (1024.0 * 1024.0),
        (double) deltaJdkS / ds.keys.length);
    jdkSess = null;

    base = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    Long2ObjectOpenHashMap<SessionState> fuSess = new Long2ObjectOpenHashMap<>(ds.keys.length);
    for (int i = 0; i < ds.keys.length; i++) {
      SessionState s = ds.sessionVals[i];
      fuSess.put(
          ds.keys[i],
          new SessionState(s.quotaRemaining, s.lastSeenNs, s.tierFlags, s.rateWindowHits));
    }
    long afterFuS = HashMapForensicEngine.usedJavaHeapBytesAfterGc();
    Reference.reachabilityFence(fuSess);
    long deltaFuS = Math.max(0L, afterFuS - base);
    System.out.printf(
        "%-42s | Heap=%6.1f MiB (%5.1f B/entry)%n",
        "Java 21 fastutil Long2Object<Session> (2M)",
        deltaFuS / (1024.0 * 1024.0),
        (double) deltaFuS / ds.keys.length);
  }

  private static void runBattle2Tail() {
    var ds = HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_AGED_ENTRIES, 1024);
    System.out.println(
        "\n=== Java 21 Battle 2B: Streaming Insert Tail Latency (0 -> 2,000,000) ===");
    // Warm up JIT once on a smaller run.
    HashMapForensicEngine.measureInsertTailJdk(
        HashMapForensicEngine.buildDataset(100_000, 64), false);
    HashMapForensicEngine.measureInsertTailFastutil(
        HashMapForensicEngine.buildDataset(100_000, 64), false);

    printTail("Java 21 HashMap<Long,Long> (Unreserved)",
        HashMapForensicEngine.measureInsertTailJdk(ds, false));
    printTail("Java 21 HashMap<Long,Long> (Reserved)",
        HashMapForensicEngine.measureInsertTailJdk(ds, true));
    printTail("Java 21 fastutil Long2Long (Unreserved)",
        HashMapForensicEngine.measureInsertTailFastutil(ds, false));
    printTail("Java 21 fastutil Long2Long (Reserved)",
        HashMapForensicEngine.measureInsertTailFastutil(ds, true));
  }

  private static void printTail(String label, HashMapForensicEngine.TailResult r) {
    System.out.printf(
        "%-42s | total=%7.1f ms | p50=%4.0f ns | p99=%5.0f ns | p99.9=%6.0f ns | p99.99=%7.0f ns | max=%10.0f ns%n",
        label, r.totalMs(), r.p50Ns(), r.p99Ns(), r.p999Ns(), r.p9999Ns(), r.maxNs());
  }

  private static void runBattle3HashDos() {
    long[] norm =
        HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_HASHDOS_KEYS, 64).keys;
    long[] dos =
        HashMapForensicEngine.buildJavaBucketZeroHashDosKeys(
            HashMapForensicEngine.NUM_HASHDOS_KEYS);

    // Warm up C2 before timing.
    for (int w = 0; w < 5; w++) {
      HashMap<Long, Long> warm = new HashMap<>(65_536);
      for (int i = 0; i < norm.length; i++) {
        warm.put(norm[i], (long) i);
      }
      globalSink ^= warm.size();
    }

    System.out.printf(
        "%n=== Java 21 Battle 3: OpenJDK JEP 180 Treeification (%d Bucket-0 Keys) ===%n",
        HashMapForensicEngine.NUM_HASHDOS_KEYS);

    long t0 = System.nanoTime();
    HashMap<Long, Long> mNorm = new HashMap<>(65_536);
    for (int i = 0; i < norm.length; i++) {
      mNorm.put(norm[i], (long) (i + 1));
    }
    double normMs = (System.nanoTime() - t0) / 1e6;
    globalSink ^= mNorm.size();
    System.out.printf("%-42s | %8.2f ms%n", "Java 21 HashMap (Normal Keys)", normMs);

    t0 = System.nanoTime();
    HashMap<Long, Long> mDos = new HashMap<>(65_536);
    for (int i = 0; i < dos.length; i++) {
      mDos.put(dos[i], (long) (i + 1));
    }
    double dosMs = (System.nanoTime() - t0) / 1e6;
    globalSink ^= mDos.size();
    System.out.printf(
        "%-42s | %8.2f ms (JEP 180 RB-tree O(N log N): %.1fx)%n",
        "Java 21 HashMap (Bucket-0 Collisions)", dosMs, dosMs / normMs);

    t0 = System.nanoTime();
    Long2LongOpenHashMap fuDos = new Long2LongOpenHashMap(dos.length);
    for (int i = 0; i < dos.length; i++) {
      fuDos.put(dos[i], (long) (i + 1));
    }
    double fuMs = (System.nanoTime() - t0) / 1e6;
    globalSink ^= fuDos.size();
    System.out.printf("%-42s | %8.2f ms%n", "Java 21 fastutil (Bucket-0 Keys)", fuMs);
  }

  public static void main(String[] args) {
    String mode = "all";
    int iters = 0;
    for (String arg : args) {
      if (arg.startsWith("--mode=")) {
        mode = arg.substring(7);
      } else if (arg.startsWith("--iters=")) {
        iters = Integer.parseInt(arg.substring(8));
      }
    }

    switch (mode) {
      case "all" -> {
        System.out.println("=== Java 21 Battle 1: L1 (1,024) vs. Aged Post-Churn (2,000,000) ===");
        runL1Jdk(iters > 0 ? iters : 100);
        runL1Fastutil(iters > 0 ? iters : 100);
        runAgedJdk(iters > 0 ? iters : 10, false);
        runAgedFastutil(iters > 0 ? iters : 10, false);
        runAgedJdk(iters > 0 ? iters : 10, true);
        runAgedFastutil(iters > 0 ? iters : 10, true);
        runAgedSessionJdk(iters > 0 ? iters : 10);
        runAgedSessionFastutil(iters > 0 ? iters : 10);
        runIterJdk(iters > 0 ? iters : 20, false);
        runIterFastutil(iters > 0 ? iters : 20, false);
        runIterJdk(iters > 0 ? iters : 60, true);
        runIterFastutil(iters > 0 ? iters : 60, true);
        runBattle2Rss();
        runBattle2Tail();
        runBattle3HashDos();
      }
      case "b1_l1_jdk" -> runL1Jdk(iters > 0 ? iters : 500);
      case "b1_l1_fastutil" -> runL1Fastutil(iters > 0 ? iters : 500);
      case "b1_aged_jdk" -> runAgedJdk(iters > 0 ? iters : 40, false);
      case "b1_aged_fastutil" -> runAgedFastutil(iters > 0 ? iters : 40, false);
      case "b1_miss_jdk" -> runAgedJdk(iters > 0 ? iters : 40, true);
      case "b1_miss_fastutil" -> runAgedFastutil(iters > 0 ? iters : 40, true);
      case "b1_aged_session_jdk" -> runAgedSessionJdk(iters > 0 ? iters : 40);
      case "b1_aged_session_fastutil" -> runAgedSessionFastutil(iters > 0 ? iters : 40);
      case "b1_iter_full_jdk" -> runIterJdk(iters > 0 ? iters : 40, false);
      case "b1_iter_full_fastutil" -> runIterFastutil(iters > 0 ? iters : 40, false);
      case "b1_iter_sparse_jdk" -> runIterJdk(iters > 0 ? iters : 200, true);
      case "b1_iter_sparse_fastutil" -> runIterFastutil(iters > 0 ? iters : 200, true);
      case "b2_rss" -> runBattle2Rss();
      case "b2_tail" -> runBattle2Tail();
      case "b3_hashdos" -> runBattle3HashDos();
      default -> {
        System.err.println("Unknown --mode=" + mode);
        System.exit(1);
      }
    }
  }
}
