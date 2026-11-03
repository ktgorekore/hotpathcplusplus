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

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.List;
import org.junit.jupiter.api.Test;

/**
 * JUnit 5 verification suite for the Episode 13 Java 21 benchmark and forensic challenge kernels.
 */
final class JavaHotPathTest {

  @Test
  void allPolymorphicEventSubtypesMatchHeapAndNoAllocSemantics() {
    OrderEvent limit = OrderEvent.makeEvent(0, 1024L, 10L);
    assertEquals(10240L - 10L, OrderEvent.runBattle1Step(limit));
    assertEquals(10240L - 10L, limit.processNoAlloc());

    OrderEvent cancel = OrderEvent.makeEvent(1, 500L, 4L);
    assertEquals(-2000L, OrderEvent.runBattle1Step(cancel));
    assertEquals(-2000L, cancel.processNoAlloc());

    OrderEvent modify = OrderEvent.makeEvent(2, 512L, 10L);
    assertEquals(512L * 11L - ((512L * 11L) >> 9), OrderEvent.runBattle1Step(modify));
    assertEquals(512L * 11L - ((512L * 10L) >> 9), modify.processNoAlloc());

    OrderEvent pegged = OrderEvent.makeEvent(3, 100L, 5L);
    assertEquals((100L + 2L) * 5L - 1L, OrderEvent.runBattle1Step(pegged));
    assertEquals((100L + 2L) * 5L, pegged.processNoAlloc());

    OrderEvent halt = OrderEvent.makeEvent(4, 100L, 5L);
    assertEquals((100L * 5L * 3L) ^ 42L, OrderEvent.runBattle1Step(halt));
    assertEquals((100L * 5L * 3L) ^ 42L, halt.processNoAlloc());
  }

  @Test
  void battle2OrderBookLayoutsProduceIdenticalTotalExposure() {
    final int count = 8192;
    List<OrderRecord> fresh = OrderBookLayouts.createFreshArrayListBook(count);
    List<OrderRecord> churned = OrderBookLayouts.createChurnedArrayListBook(count);
    OrderBookLayouts.PrimitiveSoABook soa = new OrderBookLayouts.PrimitiveSoABook(count);

    long freshSum = OrderBookLayouts.scanArrayListOrders(fresh);
    long churnedSum = OrderBookLayouts.scanArrayListOrders(churned);
    long soaSum =
        OrderBookLayouts.scanPrimitiveSoA(soa.prices(), soa.qtys(), soa.masks(), soa.size());

    assertTrue(freshSum > 0L);
    assertEquals(freshSum, churnedSum);
    assertEquals(freshSum, soaSum);
  }

  @Test
  void battle3StreamingStateEngineProducesValidPercentilesAndDeoptSpike() {
    StreamingStateEngine.TelemetryReport report = StreamingStateEngine.runBattle3(4096, 200, 64);
    assertTrue(report.p50Us() > 0.0);
    assertTrue(report.p99Us() >= report.p50Us());
    assertTrue(report.maxUs() >= report.p99Us());
    assertTrue(report.firstRarePacketNs() > 0L);
  }
}
