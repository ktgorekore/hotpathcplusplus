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
import java.util.HashMap;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Level;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OperationsPerInvocation;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

/** OpenJDK JMH microbenchmark suite for Episode 15 ("Hash Maps Across Languages"). */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 2, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 3, time = 1, timeUnit = TimeUnit.SECONDS)
@Fork(
    value = 1,
    jvmArgsAppend = {"-Xms2g", "-Xmx2g", "-XX:+AlwaysPreTouch", "-XX:+UseG1GC"})
public class JavaHashMapBenchmark {

  private static final int L1_JMH_QUERIES = 100_000;
  private static final int AGED_JMH_QUERIES = HashMapForensicEngine.NUM_QUERIES;

  private HashMapForensicEngine.GatewayDataset l1Dataset;
  private HashMapForensicEngine.GatewayDataset agedDataset;
  private HashMap<Long, Long> l1JdkPacked;
  private Long2LongOpenHashMap l1FastutilPacked;
  private HashMap<Long, Long> agedJdkPacked;
  private Long2LongOpenHashMap agedFastutilPacked;
  private HashMap<Long, SessionState> agedJdkSession;
  private Long2ObjectOpenHashMap<SessionState> agedFastutilSession;
  private HashMap<Long, Long> sparseJdkPacked;
  private Long2LongOpenHashMap sparseFastutilPacked;

  private long[] normalDosKeys;
  private long[] bucketZeroDosKeys;

  /** Initializes L1, aged DRAM, sparse, and HashDoS datasets once per JMH trial. */
  @Setup(Level.Trial)
  public void setUp() {
    l1Dataset =
        HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_L1_ENTRIES, L1_JMH_QUERIES);
    agedDataset =
        HashMapForensicEngine.buildDataset(
            HashMapForensicEngine.NUM_AGED_ENTRIES, AGED_JMH_QUERIES);

    l1JdkPacked = HashMapForensicEngine.buildAgedJdkPacked(l1Dataset);
    l1FastutilPacked = HashMapForensicEngine.buildAgedFastutilPacked(l1Dataset);
    agedJdkPacked = HashMapForensicEngine.buildAgedJdkPacked(agedDataset);
    agedFastutilPacked = HashMapForensicEngine.buildAgedFastutilPacked(agedDataset);
    agedJdkSession = HashMapForensicEngine.buildAgedJdkSession(agedDataset);
    agedFastutilSession = HashMapForensicEngine.buildAgedFastutilSession(agedDataset);
    sparseJdkPacked = HashMapForensicEngine.buildAgedJdkPackedSparse(agedDataset);
    sparseFastutilPacked = HashMapForensicEngine.buildAgedFastutilPackedSparse(agedDataset);

    normalDosKeys =
        HashMapForensicEngine.buildDataset(HashMapForensicEngine.NUM_HASHDOS_KEYS, 64).keys;
    bucketZeroDosKeys =
        HashMapForensicEngine.buildJavaBucketZeroHashDosKeys(
            HashMapForensicEngine.NUM_HASHDOS_KEYS);
  }

  /**
   * Benchmarks 80/20 lookup throughput in an L1-resident boxed {@link HashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(L1_JMH_QUERIES)
  public long BM_Battle1_L1_JavaHashMap_80_20() {
    return HashMapForensicEngine.lookupJdkPacked(l1JdkPacked, l1Dataset.queries8020);
  }

  /**
   * Benchmarks 80/20 lookup throughput in an L1-resident unboxed {@link Long2LongOpenHashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(L1_JMH_QUERIES)
  public long BM_Battle1_L1_FastutilLong2Long_80_20() {
    return HashMapForensicEngine.lookupFastutilPacked(l1FastutilPacked, l1Dataset.queries8020);
  }

  /**
   * Benchmarks 80/20 lookup throughput in an aged 2M-entry boxed {@link HashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(AGED_JMH_QUERIES)
  public long BM_Battle1_Aged2M_JavaHashMap_80_20() {
    return HashMapForensicEngine.lookupJdkPacked(agedJdkPacked, agedDataset.queries8020);
  }

  /**
   * Benchmarks 80/20 lookup throughput in an aged 2M-entry unboxed {@link Long2LongOpenHashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(AGED_JMH_QUERIES)
  public long BM_Battle1_Aged2M_FastutilLong2Long_80_20() {
    return HashMapForensicEngine.lookupFastutilPacked(agedFastutilPacked, agedDataset.queries8020);
  }

  /**
   * Benchmarks 100% miss lookup latency in an aged 2M-entry boxed {@link HashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(AGED_JMH_QUERIES)
  public long BM_Battle1_Aged2M_JavaHashMap_100_Miss() {
    return HashMapForensicEngine.lookupJdkPacked(agedJdkPacked, agedDataset.queries100Miss);
  }

  /**
   * Benchmarks 100% miss lookup latency in an aged 2M-entry {@link Long2LongOpenHashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(AGED_JMH_QUERIES)
  public long BM_Battle1_Aged2M_FastutilLong2Long_100_Miss() {
    return HashMapForensicEngine.lookupFastutilPacked(
        agedFastutilPacked, agedDataset.queries100Miss);
  }

  /**
   * Benchmarks 80/20 lookup latency in an aged 2M-entry {@link HashMap} storing {@link
   * SessionState} records.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(AGED_JMH_QUERIES)
  public long BM_Battle1_Aged2M_JavaHashMap_Session24B() {
    return HashMapForensicEngine.lookupJdkSession(agedJdkSession, agedDataset.queries8020);
  }

  /**
   * Benchmarks 80/20 lookup latency in an aged 2M-entry {@link Long2ObjectOpenHashMap} storing
   * {@link SessionState} records.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(AGED_JMH_QUERIES)
  public long BM_Battle1_Aged2M_FastutilLong2Object_Session24B() {
    return HashMapForensicEngine.lookupFastutilSession(
        agedFastutilSession, agedDataset.queries8020);
  }

  /**
   * Benchmarks full 2M-entry iteration throughput over a boxed {@link HashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(HashMapForensicEngine.NUM_AGED_ENTRIES)
  public long BM_Battle1B_IterFull2M_JavaHashMap() {
    return HashMapForensicEngine.iterJdkPacked(agedJdkPacked);
  }

  /**
   * Benchmarks full 2M-entry iteration throughput over a {@link Long2LongOpenHashMap}.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(HashMapForensicEngine.NUM_AGED_ENTRIES)
  public long BM_Battle1B_IterFull2M_FastutilLong2Long() {
    return HashMapForensicEngine.iterFastutilPacked(agedFastutilPacked);
  }

  /**
   * Benchmarks sparse 10%-occupancy iteration over a boxed {@link HashMap} after 90% deletions.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(200_000)
  public long BM_Battle1B_IterSparse200K_JavaHashMap() {
    return HashMapForensicEngine.iterJdkPacked(sparseJdkPacked);
  }

  /**
   * Benchmarks sparse 10%-occupancy iteration over a {@link Long2LongOpenHashMap} after 90%
   * deletions.
   *
   * @return verification checksum
   */
  @Benchmark
  @OperationsPerInvocation(200_000)
  public long BM_Battle1B_IterSparse200K_FastutilLong2Long() {
    return HashMapForensicEngine.iterFastutilPacked(sparseFastutilPacked);
  }

  /**
   * Benchmarks inserting 40K uniformly distributed keys into a pre-sized {@link HashMap}.
   *
   * @return final map entry count
   */
  @Benchmark
  @OutputTimeUnit(TimeUnit.MILLISECONDS)
  public int BM_Battle3_HashDos40K_JavaHashMap_Normal() {
    HashMap<Long, Long> m = new HashMap<>(65_536);
    for (int i = 0; i < normalDosKeys.length; i++) {
      m.put(normalDosKeys[i], (long) (i + 1));
    }
    return m.size();
  }

  /**
   * Benchmarks inserting 40K bucket-0 colliding keys into {@link HashMap} to exercise OpenJDK JEP
   * 180 red-black tree conversion.
   *
   * @return final map entry count
   */
  @Benchmark
  @OutputTimeUnit(TimeUnit.MILLISECONDS)
  public int BM_Battle3_HashDos40K_JavaHashMap_Bucket0_JEP180() {
    HashMap<Long, Long> m = new HashMap<>(65_536);
    for (int i = 0; i < bucketZeroDosKeys.length; i++) {
      m.put(bucketZeroDosKeys[i], (long) (i + 1));
    }
    return m.size();
  }
}
