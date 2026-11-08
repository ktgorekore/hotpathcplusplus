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

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

/** JUnit 5 verification suite for Episode 14 Java 21 Spatial and Temporal Memory Safety kernels. */
final class JavaMemorySafetyTest {

  @Test
  void primitiveAndObjectSpatialScansProduceIdenticalNotional() {
    JavaMemorySafetyEngine.SpatialDataset ds = new JavaMemorySafetyEngine.SpatialDataset(1024);
    int mask = 1023;

    long seqPrim = JavaMemorySafetyEngine.scanSequentialPrimitive(ds.prices, ds.quantities);
    long seqObj = JavaMemorySafetyEngine.scanSequentialObjectArray(ds.orderObjs);
    assertTrue(seqPrim > 0L);
    assertEquals(seqPrim, seqObj);

    long gatherChecked =
        JavaMemorySafetyEngine.gatherBatchPrimitiveChecked(ds.prices, ds.quantities, ds.randomIdx);
    long gatherBitmask =
        JavaMemorySafetyEngine.gatherBatchPrimitiveBitmask(
            ds.prices, ds.quantities, ds.randomIdx, mask);
    long gatherObj = JavaMemorySafetyEngine.gatherBatchObjectArray(ds.orderObjs, ds.randomIdx);
    assertTrue(gatherChecked > 0L);
    assertEquals(gatherChecked, gatherBitmask);
    assertEquals(gatherChecked, gatherObj);

    int[] oobIdx = new int[] {5000};
    assertThrows(
        ArrayIndexOutOfBoundsException.class,
        () -> JavaMemorySafetyEngine.gatherBatchPrimitiveChecked(ds.prices, ds.quantities, oobIdx));
  }

  @Test
  void primitiveGenerationalSlabAndManagedHeapMatchExactSurvivingNotional() {
    JavaMemorySafetyEngine.TemporalDataset td = new JavaMemorySafetyEngine.TemporalDataset(1024);

    long slabSum =
        JavaMemorySafetyEngine.runTemporalPrimitiveSlabLookup(
            td.slab, td.originalSlabHandles, td.lookupOrder);
    long heapSum =
        JavaMemorySafetyEngine.runTemporalManagedHeapLookupWithChurn(
            td.originalHeapRefs, td.liveHeapBook, td.lookupOrder, 0);

    assertTrue(slabSum > 0L);
    assertEquals(slabSum, heapSum);

    // Releasing an already-released handle in the slab must return false.
    assertFalse(td.slab.release(td.originalSlabHandles[0]));
  }
}
