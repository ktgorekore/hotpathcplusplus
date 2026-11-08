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
 * OpenJDK JMH microbenchmark suite for Episode 14 ("The Physics of Memory Safety: C++20 vs. Rust
 * vs. Java 21").
 *
 * <p>Measures Battle 1 (Spatial Bounds Checking across Sequential Scan, Random Batch Gather, and
 * Dependent Probe Chain) and Battle 3 (Temporal Safety under 25% Order Cancellation & Churn across
 * Flat Primitive Generational Slab vs. Managed Heap {@code OrderObj[]}).
 */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 2, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 3, time = 1, timeUnit = TimeUnit.SECONDS)
@Fork(
    value = 1,
    jvmArgsAppend = {"-Xms2g", "-Xmx2g", "-XX:+AlwaysPreTouch", "-XX:+UseG1GC"})
public class JavaMemorySafetyBenchmark {

  private JavaMemorySafetyEngine.SpatialDataset spatialDataset;
  private JavaMemorySafetyEngine.TemporalDataset temporalDataset;
  private int passCounter;

  /** Initializes the 65,536-order spatial and temporal datasets prior to JMH trial execution. */
  @Setup(Level.Trial)
  public void setUp() {
    spatialDataset =
        new JavaMemorySafetyEngine.SpatialDataset(JavaMemorySafetyEngine.DEFAULT_BOOK_CAPACITY);
    temporalDataset =
        new JavaMemorySafetyEngine.TemporalDataset(JavaMemorySafetyEngine.DEFAULT_BOOK_CAPACITY);
    passCounter = 0;
  }

  /**
   * Battle 1A.1: Sequential scan over primitive {@code long[]} and {@code int[]} arrays (C2 RCE).
   *
   * @return Total notional value across 65,536 orders.
   */
  @Benchmark
  public long BM_Battle1A_Java21_Sequential_PrimitiveRCE() {
    return JavaMemorySafetyEngine.scanSequentialPrimitive(
        spatialDataset.prices, spatialDataset.quantities);
  }

  /**
   * Battle 1A.2: Sequential scan over managed {@code OrderObj[]} reference array.
   *
   * @return Total notional value across 65,536 orders.
   */
  @Benchmark
  public long BM_Battle1A_Java21_Sequential_OrderObjArray() {
    return JavaMemorySafetyEngine.scanSequentialObjectArray(spatialDataset.orderObjs);
  }

  /**
   * Battle 1B.1: Independent random batch gather over primitive arrays (mandatory JVM bounds
   * check).
   *
   * @return Total notional value across 65,536 random lookups.
   */
  @Benchmark
  public long BM_Battle1B_Java21_BatchGather_PrimitiveChecked() {
    return JavaMemorySafetyEngine.gatherBatchPrimitiveChecked(
        spatialDataset.prices, spatialDataset.quantities, spatialDataset.randomIdx);
  }

  /**
   * Battle 1B.2: Independent random batch gather with power-of-two bitmask {@code idx & 0xFFFF}.
   *
   * @return Total notional value across 65,536 random lookups.
   */
  @Benchmark
  public long BM_Battle1B_Java21_BatchGather_PrimitiveBitmask() {
    return JavaMemorySafetyEngine.gatherBatchPrimitiveBitmask(
        spatialDataset.prices,
        spatialDataset.quantities,
        spatialDataset.randomIdx,
        JavaMemorySafetyEngine.DEFAULT_BOOK_MASK);
  }

  /**
   * Battle 1B.3: Independent random batch gather over managed {@code OrderObj[]} reference array.
   *
   * @return Total notional value across 65,536 random lookups.
   */
  @Benchmark
  public long BM_Battle1B_Java21_BatchGather_OrderObjArray() {
    return JavaMemorySafetyEngine.gatherBatchObjectArray(
        spatialDataset.orderObjs, spatialDataset.randomIdx);
  }

  /**
   * Battle 1C: Latency-bound dependent order-book probe chain over primitive arrays.
   *
   * @return Accumulated dependent probe notional.
   */
  @Benchmark
  public long BM_Battle1C_Java21_DependentProbe_Primitive() {
    return JavaMemorySafetyEngine.probeDependentPrimitive(
        spatialDataset.prices,
        spatialDataset.quantities,
        spatialDataset.randomIdx,
        JavaMemorySafetyEngine.DEFAULT_BOOK_MASK);
  }

  /**
   * Battle 3.1: Zero-GC Flat Primitive Generational Slab lookup (`(generation << 32) | index`).
   *
   * @return Exact uncorrupted notional across 75% surviving orders.
   */
  @Benchmark
  public long BM_Battle3_Java21_Temporal_PrimitiveGenerationalSlab() {
    return JavaMemorySafetyEngine.runTemporalPrimitiveSlabLookup(
        temporalDataset.slab, temporalDataset.originalSlabHandles, temporalDataset.lookupOrder);
  }

  /**
   * Battle 3.2: Managed Heap {@code OrderObj[]} lookup with active allocation churn.
   *
   * @return Exact uncorrupted notional across 75% surviving orders.
   */
  @Benchmark
  public long BM_Battle3_Java21_Temporal_ManagedHeapWithChurn() {
    return JavaMemorySafetyEngine.runTemporalManagedHeapLookupWithChurn(
        temporalDataset.originalHeapRefs,
        temporalDataset.liveHeapBook,
        temporalDataset.lookupOrder,
        ++passCounter);
  }
}
