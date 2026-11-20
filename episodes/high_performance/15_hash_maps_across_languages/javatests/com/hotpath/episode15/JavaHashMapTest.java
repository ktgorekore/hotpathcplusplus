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

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;

import java.util.HashMap;
import org.junit.jupiter.api.Test;

/** JUnit 5 verification suite for Episode 15 Java 21 benchmarks. */
final class JavaHashMapTest {

  @Test
  void jdkAndFastutilProduceIdenticalChecksums() {
    var ds = HashMapForensicEngine.buildDataset(16_384, 32_768);
    var jdkPacked = HashMapForensicEngine.buildAgedJdkPacked(ds);
    var fuPacked = HashMapForensicEngine.buildAgedFastutilPacked(ds);

    long cJdk = HashMapForensicEngine.lookupJdkPacked(jdkPacked, ds.queries8020);
    long cFu = HashMapForensicEngine.lookupFastutilPacked(fuPacked, ds.queries8020);
    assertNotEquals(0L, cJdk);
    assertEquals(cJdk, cFu);

    assertEquals(0L, HashMapForensicEngine.lookupJdkPacked(jdkPacked, ds.queries100Miss));
    assertEquals(0L, HashMapForensicEngine.lookupFastutilPacked(fuPacked, ds.queries100Miss));

    var jdkSess = HashMapForensicEngine.buildAgedJdkSession(ds);
    var fuSess = HashMapForensicEngine.buildAgedFastutilSession(ds);
    assertEquals(
        HashMapForensicEngine.lookupJdkSession(jdkSess, ds.queries8020),
        HashMapForensicEngine.lookupFastutilSession(fuSess, ds.queries8020));
  }

  @Test
  void allBucketZeroHashDosKeysCollideInLowSixteenBits() {
    long[] keys =
        HashMapForensicEngine.buildJavaBucketZeroHashDosKeys(
            HashMapForensicEngine.NUM_HASHDOS_KEYS);
    HashMap<Long, Long> map = new HashMap<>(65_536);
    for (int i = 0; i < keys.length; i++) {
      int mixed = HashMapForensicEngine.jdkHashMix(keys[i]);
      assertEquals(0, mixed & 0xFFFF);
      map.put(keys[i], (long) i);
    }
    assertEquals(HashMapForensicEngine.NUM_HASHDOS_KEYS, map.size());
  }
}
