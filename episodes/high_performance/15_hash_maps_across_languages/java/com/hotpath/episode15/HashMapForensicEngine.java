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

import it.unimi.dsi.fastutil.longs.Long2LongMap;
import it.unimi.dsi.fastutil.longs.Long2LongOpenHashMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import it.unimi.dsi.fastutil.objects.ObjectIterator;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HashMap;

/**
 * Deterministic Edge Gateway hash map workloads for Java 21 LTS comparing standard {@link
 * java.util.HashMap} (boxed entries + JEP 180 RB-tree buckets) against {@code fastutil} unboxed
 * primitive open-addressing maps.
 */
public final class HashMapForensicEngine {

  public static final long SEED_KEYS = 0xC0FFEE15L;
  public static final long SEED_QUERIES = 0xFACE0015L;
  public static final long SEED_MISSES = 0xBAD00015L;
  public static final long SEED_CHURN = 0x99990015L;

  public static final int NUM_L1_ENTRIES = 1024;
  public static final int NUM_AGED_ENTRIES = 2_000_000;
  public static final int NUM_QUERIES = 2_000_000;
  public static final int NUM_HASHDOS_KEYS = 40_000;
  public static final long MISSING_SENTINEL = -1L;

  private HashMapForensicEngine() {}

  /** Container holding identical SplitMix64 keys, values, and query arrays. */
  public static final class GatewayDataset {
    public final long[] keys;
    public final long[] packedVals;
    public final SessionState[] sessionVals;
    public final long[] queries8020;
    public final long[] queries100Miss;

    GatewayDataset(
        long[] keys,
        long[] packedVals,
        SessionState[] sessionVals,
        long[] queries8020,
        long[] queries100Miss) {
      this.keys = keys;
      this.packedVals = packedVals;
      this.sessionVals = sessionVals;
      this.queries8020 = queries8020;
      this.queries100Miss = queries100Miss;
    }
  }

  /** Summary percentiles for streaming insert latency telemetry. */
  public record TailResult(
      double totalMs,
      double p50Ns,
      double p99Ns,
      double p999Ns,
      double p9999Ns,
      double maxNs,
      long checksum) {}

  /** Advances SplitMix64 state and returns the mixed 64-bit output. */
  public static long splitMix64Next(long[] stateRef) {
    stateRef[0] += 0x9e3779b97f4a7c15L;
    long z = stateRef[0];
    z = (z ^ (z >>> 30)) * 0xbf58476d1ce4e5b9L;
    z = (z ^ (z >>> 27)) * 0x94d049bb133111ebL;
    return z ^ (z >>> 31);
  }

  public static long packTokenBucket(long raw) {
    long lastSeenMs = (raw >>> 24) & 0x7FFFFFFFFFL;
    long quota = (raw & 0xFFFFL) + 1L;
    return (lastSeenMs << 24) | quota;
  }

  public static long finalizeChecksum(long sum, long hits) {
    return sum ^ (hits * 0x9e3779b97f4a7c15L);
  }

  /** Generates a deterministic dataset bit-for-bit identical to C++/Rust/Go/Python. */
  public static GatewayDataset buildDataset(int numEntries, int numQueries) {
    long[] keys = new long[numEntries];
    long[] packedVals = new long[numEntries];
    SessionState[] sessionVals = new SessionState[numEntries];

    long[] keyRng = {SEED_KEYS};
    for (int i = 0; i < numEntries; i++) {
      long k = (splitMix64Next(keyRng) & 0x3FFFFFFFFFFFFFFFL) | 1L;
      long raw = splitMix64Next(keyRng);
      long packed = packTokenBucket(raw);
      long quota = packed & 0xFFFFFFL;
      long lastSeenMs = (packed >>> 24) & 0x7FFFFFFFFFL;
      keys[i] = k;
      packedVals[i] = packed;
      sessionVals[i] =
          new SessionState(
              quota,
              lastSeenMs * 1_000_000L,
              (int) (k & 0xFFFFL),
              (int) ((k >>> 16) & 0xFFL));
    }

    long[] queries8020 = new long[numQueries];
    long[] qRng = {SEED_QUERIES};
    for (int i = 0; i < numQueries; i++) {
      long r = splitMix64Next(qRng);
      if (Long.remainderUnsigned(r, 5L) != 0L) {
        int idx = (int) Long.remainderUnsigned(r >>> 8, Integer.toUnsignedLong(numEntries));
        queries8020[i] = keys[idx];
      } else {
        queries8020[i] = (r & 0x3FFFFFFFFFFFFFFFL) | (1L << 62) | 1L;
      }
    }

    long[] queries100Miss = new long[numQueries];
    long[] mRng = {SEED_MISSES};
    for (int i = 0; i < numQueries; i++) {
      queries100Miss[i] = (splitMix64Next(mRng) & 0x3FFFFFFFFFFFFFFFL) | (1L << 62) | 1L;
    }

    return new GatewayDataset(keys, packedVals, sessionVals, queries8020, queries100Miss);
  }

  /** Builds an aged HashMap after mixed-size heap churn so nodes disperse across pages. */
  public static HashMap<Long, Long> buildAgedJdkPacked(GatewayDataset ds) {
    byte[][] noise = createHeapChurn(300_000);
    HashMap<Long, Long> map = new HashMap<>((int) (ds.keys.length / 0.75) + 1);
    byte[][] scratch = new byte[128][];
    for (int i = 0; i < ds.keys.length; i++) {
      map.put(ds.keys[i], ds.packedVals[i]);
      if ((i & 7) == 0) {
        scratch[(i >>> 3) & 127] = new byte[64];
      }
    }
    if (noise.length == 0 && scratch.length == 0) {
      throw new AssertionError();
    }
    return map;
  }

  public static HashMap<Long, SessionState> buildAgedJdkSession(GatewayDataset ds) {
    byte[][] noise = createHeapChurn(300_000);
    HashMap<Long, SessionState> map = new HashMap<>((int) (ds.keys.length / 0.75) + 1);
    for (int i = 0; i < ds.keys.length; i++) {
      map.put(ds.keys[i], ds.sessionVals[i]);
    }
    if (noise.length == 0) {
      throw new AssertionError();
    }
    return map;
  }

  public static Long2LongOpenHashMap buildAgedFastutilPacked(GatewayDataset ds) {
    Long2LongOpenHashMap map = new Long2LongOpenHashMap(ds.keys.length);
    map.defaultReturnValue(MISSING_SENTINEL);
    for (int i = 0; i < ds.keys.length; i++) {
      map.put(ds.keys[i], ds.packedVals[i]);
    }
    return map;
  }

  public static Long2ObjectOpenHashMap<SessionState> buildAgedFastutilSession(GatewayDataset ds) {
    byte[][] noise = createHeapChurn(300_000);
    Long2ObjectOpenHashMap<SessionState> map = new Long2ObjectOpenHashMap<>(ds.keys.length);
    for (int i = 0; i < ds.keys.length; i++) {
      map.put(ds.keys[i], ds.sessionVals[i]);
    }
    if (noise.length == 0) {
      throw new AssertionError();
    }
    return map;
  }

  private static byte[][] createHeapChurn(int chunkCount) {
    long[] rng = {SEED_CHURN};
    byte[][] chunks = new byte[chunkCount][];
    for (int i = 0; i < chunkCount; i++) {
      int sz = 32 + (int) ((splitMix64Next(rng) & 7L) * 32L);
      chunks[i] = new byte[sz];
    }
    for (int i = 0; i < chunkCount; i += 2) {
      chunks[i] = null;
    }
    return chunks;
  }

  public static long lookupJdkPacked(HashMap<Long, Long> map, long[] queries) {
    long sum = 0L;
    long hits = 0L;
    for (long q : queries) {
      Long val = map.get(q);
      if (val != null) {
        sum += val;
        hits++;
      }
    }
    return finalizeChecksum(sum, hits);
  }

  public static long lookupJdkSession(HashMap<Long, SessionState> map, long[] queries) {
    long sum = 0L;
    long hits = 0L;
    for (long q : queries) {
      SessionState s = map.get(q);
      if (s != null) {
        sum += s.digest();
        hits++;
      }
    }
    return finalizeChecksum(sum, hits);
  }

  public static long lookupFastutilPacked(Long2LongOpenHashMap map, long[] queries) {
    long sum = 0L;
    long hits = 0L;
    for (long q : queries) {
      long val = map.get(q);
      if (val != MISSING_SENTINEL) {
        sum += val;
        hits++;
      }
    }
    return finalizeChecksum(sum, hits);
  }

  public static long lookupFastutilSession(
      Long2ObjectOpenHashMap<SessionState> map, long[] queries) {
    long sum = 0L;
    long hits = 0L;
    for (long q : queries) {
      SessionState s = map.get(q);
      if (s != null) {
        sum += s.digest();
        hits++;
      }
    }
    return finalizeChecksum(sum, hits);
  }

  public static HashMap<Long, Long> buildAgedJdkPackedSparse(GatewayDataset ds) {
    HashMap<Long, Long> map = buildAgedJdkPacked(ds);
    for (int i = 0; i < ds.keys.length; i++) {
      if ((i % 10) != 0) {
        map.remove(ds.keys[i]);
      }
    }
    return map;
  }

  public static Long2LongOpenHashMap buildAgedFastutilPackedSparse(GatewayDataset ds) {
    Long2LongOpenHashMap map = buildAgedFastutilPacked(ds);
    for (int i = 0; i < ds.keys.length; i++) {
      if ((i % 10) != 0) {
        map.remove(ds.keys[i]);
      }
    }
    return map;
  }

  public static long iterJdkPacked(HashMap<Long, Long> map) {
    long sum = 0L;
    long count = 0L;
    for (var entry : map.entrySet()) {
      sum += (entry.getKey() ^ entry.getValue());
      count++;
    }
    return finalizeChecksum(sum, count);
  }

  public static long iterFastutilPacked(Long2LongOpenHashMap map) {
    long sum = 0L;
    long count = 0L;
    ObjectIterator<Long2LongMap.Entry> it = map.long2LongEntrySet().fastIterator();
    while (it.hasNext()) {
      Long2LongMap.Entry entry = it.next();
      sum += (entry.getLongKey() ^ entry.getLongValue());
      count++;
    }
    return finalizeChecksum(sum, count);
  }

  /**
   * Generates {@code count} keys that all share {@code (HashMap.hash(k) & 0xFFFF) == 0}, forcing
   * every key into bucket 0 of a 65,536-slot HashMap and triggering OpenJDK JEP 180 treeifyBin().
   */
  public static long[] buildJavaBucketZeroHashDosKeys(int count) {
    long[] out = new long[count];
    for (int i = 0; i < count; i++) {
      long upper = (long) (i + 1);
      out[i] = (upper << 16) | upper;
    }
    return out;
  }

  /** Computes the exact OpenJDK HashMap hash mixing function {@code h ^ (h >>> 16)}. */
  public static int jdkHashMix(long key) {
    int h = Long.hashCode(key);
    return h ^ (h >>> 16);
  }

  public static long usedJavaHeapBytesAfterGc() {
    for (int i = 0; i < 3; i++) {
      System.gc();
      try {
        Thread.sleep(20);
      } catch (InterruptedException ignored) {
        Thread.currentThread().interrupt();
      }
    }
    Runtime rt = Runtime.getRuntime();
    return rt.totalMemory() - rt.freeMemory();
  }

  public static long readVmRssBytes() {
    try {
      for (String line : Files.readAllLines(Path.of("/proc/self/status"))) {
        if (line.startsWith("VmRSS:")) {
          String[] parts = line.trim().split("\\s+");
          if (parts.length >= 2) {
            return Long.parseLong(parts[1]) * 1024L;
          }
        }
      }
    } catch (IOException ignored) {
    }
    return 0L;
  }

  public static TailResult measureInsertTailJdk(GatewayDataset ds, boolean reserve) {
    long[] samples = new long[ds.keys.length];
    HashMap<Long, Long> map =
        reserve ? new HashMap<>((int) (ds.keys.length / 0.75) + 1) : new HashMap<>();
    long tAll0 = System.nanoTime();
    for (int i = 0; i < ds.keys.length; i++) {
      long k = ds.keys[i];
      long v = ds.packedVals[i];
      long t0 = System.nanoTime();
      map.put(k, v);
      samples[i] = System.nanoTime() - t0;
    }
    long tAll1 = System.nanoTime();
    return summarizeTail(samples, (tAll1 - tAll0) / 1e6, map.size());
  }

  public static TailResult measureInsertTailFastutil(GatewayDataset ds, boolean reserve) {
    long[] samples = new long[ds.keys.length];
    Long2LongOpenHashMap map =
        reserve ? new Long2LongOpenHashMap(ds.keys.length) : new Long2LongOpenHashMap();
    long tAll0 = System.nanoTime();
    for (int i = 0; i < ds.keys.length; i++) {
      long k = ds.keys[i];
      long v = ds.packedVals[i];
      long t0 = System.nanoTime();
      map.put(k, v);
      samples[i] = System.nanoTime() - t0;
    }
    long tAll1 = System.nanoTime();
    return summarizeTail(samples, (tAll1 - tAll0) / 1e6, map.size());
  }

  private static TailResult summarizeTail(long[] samples, double totalMs, long checksum) {
    Arrays.sort(samples);
    int n = samples.length;
    return new TailResult(
        totalMs,
        samples[(int) (0.50 * (n - 1))],
        samples[(int) (0.99 * (n - 1))],
        samples[(int) (0.999 * (n - 1))],
        samples[(int) (0.9999 * (n - 1))],
        samples[n - 1],
        checksum);
  }
}
