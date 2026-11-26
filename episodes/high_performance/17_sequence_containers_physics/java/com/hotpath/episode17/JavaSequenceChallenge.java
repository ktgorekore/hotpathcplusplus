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

package com.hotpath.episode17;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;

/**
 * Episode 17: OpenJDK 21 LTS full 3-story parity harness for {@code RealtimeGatewayService}.
 *
 * <ul>
 *   <li><b>Story 1A:</b> Shuffled aged-heap {@code ArrayList<SubscriptionRule>} vs. {@code
 *       LinkedList<SubscriptionRule>} full-table scans (500 sweeps) + mid-table unsubscribes
 *       (ordered remove, Sparse-Set swap-pop, tombstone + {@code removeIf}, and {@code
 *       LinkedList}).
 *   <li><b>Story 1B:</b> Caffeine/Netty-style intrusive dual-link {@code ActiveConnection} (zero
 *       wrapper {@code Node} allocations across 16,384 connections &amp; 200,000 events) vs. {@code
 *       O(1)}-indexed array shift (zero {@code indexOf}) and linear {@code ArrayList.indexOf}.
 *   <li><b>Story 2A &amp; 2B:</b> 100,000 idle {@code ArrayDeque} eager {@code Object[17]}
 *       footprint vs. lazy {@code ArrayList}, plus 8,000,000 FIFO message streams (single channel
 *       and 1,024-connection multiplexed gateway).
 *   <li><b>Story 3:</b> 150,000 {@code ClientSession} reconnect wave into {@code
 *       ArrayList<ClientSession>} (unreserved 1.5x {@code System.arraycopy} of 4B Compressed OOP
 *       references vs. pre-sized {@code ArrayList(150_000)}) with full C2 JIT warmup so standalone
 *       {@code --story=3} matches steady-state full-suite timing.
 * </ul>
 */
public final class JavaSequenceChallenge {

  private static final long SEED_RULES = 0x5441534B17L;
  private static final long SEED_NOISE = 0x99990017L;
  private static volatile long blackhole;
  private static volatile Object keepAlive;

  private static long splitMix64(long[] state) {
    state[0] += 0x9e3779b97f4a7c15L;
    long z = state[0];
    z = (z ^ (z >>> 30)) * 0xbf58476d1ce4e5b9L;
    z = (z ^ (z >>> 27)) * 0x94d049bb133111ebL;
    return z ^ (z >>> 31);
  }

  private static long mixOrderHash(long acc, long val) {
    return Long.rotateLeft(acc, 7) ^ (val + 0x9e3779b97f4a7c15L);
  }

  // ===========================================================================
  // Story 1A: SubscriptionRule Table Sweep & Mid-Table Unsubscribes
  // ===========================================================================

  private static final class SubscriptionRule {
    final long ruleId;
    final long tenantId;
    final long severityWeight;
    final long expiresAtMs;
    final int thresholdLimit;
    final int currentValue;
    final int triggerCount;
    int activeFlag;

    SubscriptionRule(long id, long[] rng) {
      this.ruleId = id;
      this.tenantId = 1000L + Long.remainderUnsigned(splitMix64(rng), 4096L);
      this.severityWeight = 100L + Long.remainderUnsigned(splitMix64(rng), 5000L);
      this.expiresAtMs = 1764090000000L + id * 10L;
      this.thresholdLimit = 100 + (int) Long.remainderUnsigned(splitMix64(rng), 900L);
      this.currentValue = (int) Long.remainderUnsigned(splitMix64(rng), 50L);
      this.triggerCount = (int) (id & 3L);
      this.activeFlag = 1;
    }

    SubscriptionRule copyActive() {
      return new SubscriptionRule(
          ruleId, tenantId, severityWeight, expiresAtMs, thresholdLimit, currentValue, triggerCount);
    }

    private SubscriptionRule(
        long ruleId,
        long tenantId,
        long severityWeight,
        long expiresAtMs,
        int thresholdLimit,
        int currentValue,
        int triggerCount) {
      this.ruleId = ruleId;
      this.tenantId = tenantId;
      this.severityWeight = severityWeight;
      this.expiresAtMs = expiresAtMs;
      this.thresholdLimit = thresholdLimit;
      this.currentValue = currentValue;
      this.triggerCount = triggerCount;
      this.activeFlag = 1;
    }

    long evaluateScore() {
      if (activeFlag == 0) {
        return 0L;
      }
      return severityWeight * (long) (thresholdLimit - currentValue);
    }
  }

  // ===========================================================================
  // Story 1B: Caffeine / Netty Style Intrusive Dual-Link ActiveConnection
  // ===========================================================================

  private static final class ActiveConnection {
    final long connId;
    long lastSeenMs;
    long bytesRx;
    long bytesTx;
    final int fd;
    int tlsState;
    final long[] socketMetrics = new long[12];

    // Embedded intrusive doubly-linked hooks (zero LinkedList$Node wrappers!):
    ActiveConnection lruPrev;
    ActiveConnection lruNext;
    ActiveConnection toPrev;
    ActiveConnection toNext;

    ActiveConnection(long connId, int fd) {
      this.connId = connId;
      this.fd = fd;
    }
  }

  private static final class IntrusiveDualQueues {
    ActiveConnection lruHead;
    ActiveConnection lruTail;
    ActiveConnection toHead;
    ActiveConnection toTail;

    void pushBackLru(ActiveConnection c) {
      ActiveConnection prev = lruTail;
      c.lruPrev = prev;
      c.lruNext = null;
      if (prev != null) {
        prev.lruNext = c;
      } else {
        lruHead = c;
      }
      lruTail = c;
    }

    void pushFrontLru(ActiveConnection c) {
      ActiveConnection next = lruHead;
      c.lruPrev = null;
      c.lruNext = next;
      if (next != null) {
        next.lruPrev = c;
      } else {
        lruTail = c;
      }
      lruHead = c;
    }

    void unlinkLru(ActiveConnection c) {
      ActiveConnection prev = c.lruPrev;
      ActiveConnection next = c.lruNext;
      if (prev != null) {
        prev.lruNext = next;
      } else {
        lruHead = next;
      }
      if (next != null) {
        next.lruPrev = prev;
      } else {
        lruTail = prev;
      }
    }

    void pushBackTimeout(ActiveConnection c) {
      ActiveConnection prev = toTail;
      c.toPrev = prev;
      c.toNext = null;
      if (prev != null) {
        prev.toNext = c;
      } else {
        toHead = c;
      }
      toTail = c;
    }

    void unlinkTimeout(ActiveConnection c) {
      ActiveConnection prev = c.toPrev;
      ActiveConnection next = c.toNext;
      if (prev != null) {
        prev.toNext = next;
      } else {
        toHead = next;
      }
      if (next != null) {
        next.toPrev = prev;
      } else {
        toTail = prev;
      }
    }
  }

  // ===========================================================================
  // Story 2: OutboundMessage
  // ===========================================================================

  private static final class OutboundMessage {
    final long messageId;
    final long enqueuedMs;
    final int channelId;
    final int payloadBytes;
    final int priority;
    final int flags;
    final byte routingTag0;

    OutboundMessage(long seq) {
      this.messageId = seq;
      this.enqueuedMs = 1764095000000L + seq * 2L;
      this.channelId = (int) (Long.remainderUnsigned(seq, 256L) * 2L + 1L);
      this.payloadBytes = (int) (64L + (seq & 0x3FFL));
      this.priority = (int) (seq & 7L);
      this.flags = 0x4;
      this.routingTag0 = (byte) (seq & 0xFFL);
    }

    long digest() {
      return messageId
          ^ enqueuedMs
          ^ ((long) channelId << 32)
          ^ (long) (payloadBytes + priority)
          ^ Byte.toUnsignedLong(routingTag0);
    }
  }

  // ===========================================================================
  // Story 3: ClientSession
  // ===========================================================================

  private static final class ClientSession {
    final long sessionId;
    final int accountId;
    final int flags;
    final String endpointUrl;
    final byte[] readBuffer;
    final ArrayDeque<Long> pendingMessageIds;

    ClientSession(long id) {
      this.sessionId = id;
      this.accountId = (int) ((id * 31L) & 0xFFFFL);
      this.flags = 1;
      this.endpointUrl =
          "https://gateway.service.internal/v2/sessions/slot-"
              + (char) ('0' + Long.remainderUnsigned(id, 10L));
      this.readBuffer = new byte[256];
      Arrays.fill(this.readBuffer, (byte) (id & 0xFFL));
      this.pendingMessageIds = new ArrayDeque<>(8);
      for (long msg = 0; msg < 4; ++msg) {
        this.pendingMessageIds.addLast(id * 16L + msg);
      }
    }

    long digest() {
      long urlLen = endpointUrl.length();
      long urlTail = endpointUrl.charAt((int) urlLen - 1);
      long firstMsg = pendingMessageIds.peekFirst();
      long bufByte = Byte.toUnsignedLong(readBuffer[0]);
      return sessionId
          ^ ((long) accountId << 16)
          ^ ((long) flags << 40)
          ^ (urlLen << 32)
          ^ (urlTail << 8)
          ^ (bufByte << 24)
          ^ firstMsg
          ^ (long) (readBuffer.length + pendingMessageIds.size());
    }
  }

  @FunctionalInterface
  private interface LongKernel {
    long run();
  }

  private static long lastChecksum;

  private static double measureMedianMs(
      int warmup, int iters, Runnable setup, LongKernel fn) {
    for (int i = 0; i < warmup; ++i) {
      setup.run();
      lastChecksum = fn.run();
      blackhole ^= lastChecksum;
    }
    double[] samples = new double[iters];
    for (int i = 0; i < iters; ++i) {
      setup.run();
      long t0 = System.nanoTime();
      lastChecksum = fn.run();
      long t1 = System.nanoTime();
      blackhole ^= lastChecksum;
      samples[i] = (t1 - t0) / 1e6;
    }
    Arrays.sort(samples);
    return samples[iters / 2];
  }

  private static void runStory1A() {
    System.out.println(
        "=== JAVA 21 LTS STORY 1A: Aged Out-of-Order Heap Rule Table Scan & Mid-Table Unsub ===");
    int[] sizes = {1024, 4096, 16384};
    int reps = 500;
    for (int n : sizes) {
      long[] rng = {SEED_RULES};
      long[] noiseRng = {SEED_NOISE ^ n};

      // Pre-generate deterministic logical rules, then allocate physical heap copies
      // and LinkedList$Node wrappers in a Fisher-Yates shuffled order across live heap
      // noise (WITHOUT linear System.gc() compaction!) so heap layout reflects real uptime.
      SubscriptionRule[] logical = new SubscriptionRule[n];
      for (int i = 0; i < n; ++i) {
        logical[i] = new SubscriptionRule(i + 1L, rng);
      }
      int[] order = new int[n];
      for (int i = 0; i < n; ++i) {
        order[i] = i;
      }
      for (int i = n - 1; i > 0; --i) {
        int j = (int) Long.remainderUnsigned(splitMix64(noiseRng), i + 1L);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
      }

      ArrayList<byte[]> liveHeapNoise = new ArrayList<>(n * 2);
      SubscriptionRule[] scatteredRules = new SubscriptionRule[n];
      for (int k = 0; k < n; ++k) {
        int idx = order[k];
        scatteredRules[idx] = logical[idx].copyActive();
        int sz = 64 + (int) Long.remainderUnsigned(splitMix64(noiseRng), 384L);
        liveHeapNoise.add(new byte[sz]);
      }

      ArrayList<SubscriptionRule> arrayList = new ArrayList<>(n);
      for (int i = 0; i < n; ++i) {
        arrayList.add(scatteredRules[i]);
      }

      // Build LinkedList while interleaving noise so Node wrappers scatter across TLABs/regions.
      LinkedList<SubscriptionRule> linkedList = new LinkedList<>();
      for (int i = 0; i < n; ++i) {
        linkedList.add(scatteredRules[i]);
        int sz = 48 + (int) Long.remainderUnsigned(splitMix64(noiseRng), 256L);
        liveHeapNoise.set(i % liveHeapNoise.size(), new byte[sz]);
      }
      keepAlive = liveHeapNoise;

      double arrMs =
          measureMedianMs(
              2,
              5,
              () -> {},
              () -> {
                long sum = 0;
                for (int r = 0; r < reps; ++r) {
                  for (int i = 0; i < arrayList.size(); ++i) {
                    sum += arrayList.get(i).evaluateScore();
                  }
                }
                return sum;
              });
      long arrCs = lastChecksum;

      double listMs =
          measureMedianMs(
              2,
              5,
              () -> {},
              () -> {
                long sum = 0;
                for (int r = 0; r < reps; ++r) {
                  for (SubscriptionRule rule : linkedList) {
                    sum += rule.evaluateScore();
                  }
                }
                return sum;
              });
      long listCs = lastChecksum;

      double totalEvals = (double) reps * n;
      System.out.printf(
          "  [Aged Scan x%d, N=%5d] ArrayList=%7.3f ms (%6.1f M/s) | LinkedList=%7.3f ms (%5.1f M/s, %.2fx slower) | parity=%s%n",
          reps,
          n,
          arrMs,
          (totalEvals / (arrMs / 1000.0)) / 1e6,
          listMs,
          (totalEvals / (listMs / 1000.0)) / 1e6,
          listMs / arrMs,
          arrCs == listCs ? "OK" : "MISMATCH");
    }

    // Mid-table unsubscribes (N = 8,192, 512 unsubs, full sweep every 8)
    final int depth = 8192;
    final int unsubs = 512;
    final int scanEvery = 8;
    long[] rng = {SEED_RULES};
    SubscriptionRule[] initial = new SubscriptionRule[depth];
    ArrayList<Long> liveIds = new ArrayList<>(depth);
    for (int i = 0; i < depth; ++i) {
      initial[i] = new SubscriptionRule(i + 1L, rng);
      liveIds.add(initial[i].ruleId);
    }
    long[] unsubIds = new long[unsubs];
    SubscriptionRule[] replacements = new SubscriptionRule[unsubs];
    long nextId = depth + 1L;
    for (int c = 0; c < unsubs; ++c) {
      int lo = depth / 5;
      int span = Math.max(1, (depth * 3) / 5);
      int idx = lo + (int) Long.remainderUnsigned(splitMix64(rng), span);
      unsubIds[c] = liveIds.get(idx);
      SubscriptionRule rep = new SubscriptionRule(nextId++, rng);
      replacements[c] = rep;
      liveIds.remove(idx);
      liveIds.add(rep.ruleId);
    }

    ArrayList<SubscriptionRule> workArr = new ArrayList<>(depth + 256);
    double arrOrdMs =
        measureMedianMs(
            2,
            5,
            () -> {
              workArr.clear();
              for (SubscriptionRule r : initial) {
                workArr.add(r.copyActive());
              }
            },
            () -> {
              long total = 0;
              for (int c = 0; c < unsubs; ++c) {
                if ((c % scanEvery) == 0) {
                  for (int i = 0; i < workArr.size(); ++i) {
                    total += workArr.get(i).evaluateScore();
                  }
                }
                long target = unsubIds[c];
                for (int i = 0; i < workArr.size(); ++i) {
                  if (workArr.get(i).ruleId == target) {
                    workArr.remove(i);
                    break;
                  }
                }
                workArr.add(replacements[c].copyActive());
              }
              for (int i = 0; i < workArr.size(); ++i) {
                total += workArr.get(i).evaluateScore();
              }
              return total;
            });
    long ordCs = lastChecksum;

    HashMap<Long, Integer> sparseMap = new HashMap<>(16384);
    double arrSparseMs =
        measureMedianMs(
            2,
            5,
            () -> {
              workArr.clear();
              sparseMap.clear();
              for (int i = 0; i < initial.length; ++i) {
                workArr.add(initial[i].copyActive());
                sparseMap.put(initial[i].ruleId, i);
              }
            },
            () -> {
              long total = 0;
              for (int c = 0; c < unsubs; ++c) {
                if ((c % scanEvery) == 0) {
                  for (int i = 0; i < workArr.size(); ++i) {
                    total += workArr.get(i).evaluateScore();
                  }
                }
                Integer slotObj = sparseMap.remove(unsubIds[c]);
                if (slotObj != null) {
                  int slot = slotObj;
                  int last = workArr.size() - 1;
                  SubscriptionRule back = workArr.remove(last);
                  if (slot != last) {
                    workArr.set(slot, back);
                    sparseMap.put(back.ruleId, slot);
                  }
                }
                SubscriptionRule rep = replacements[c].copyActive();
                sparseMap.put(rep.ruleId, workArr.size());
                workArr.add(rep);
              }
              for (int i = 0; i < workArr.size(); ++i) {
                total += workArr.get(i).evaluateScore();
              }
              return total;
            });
    long sparseCs = lastChecksum;

    HashMap<Long, Integer> posMap = new HashMap<>(16384);
    double arrTombMs =
        measureMedianMs(
            2,
            5,
            () -> {
              workArr.clear();
              posMap.clear();
              for (int i = 0; i < initial.length; ++i) {
                workArr.add(initial[i].copyActive());
                posMap.put(initial[i].ruleId, i);
              }
            },
            () -> {
              long total = 0;
              int dead = 0;
              for (int c = 0; c < unsubs; ++c) {
                if ((c % scanEvery) == 0) {
                  for (int i = 0; i < workArr.size(); ++i) {
                    total += workArr.get(i).evaluateScore();
                  }
                }
                Integer idx = posMap.remove(unsubIds[c]);
                if (idx != null) {
                  workArr.get(idx).activeFlag = 0;
                  ++dead;
                }
                SubscriptionRule rep = replacements[c].copyActive();
                posMap.put(rep.ruleId, workArr.size());
                workArr.add(rep);
                if (dead >= 128) {
                  workArr.removeIf(r -> r.activeFlag == 0);
                  posMap.clear();
                  for (int i = 0; i < workArr.size(); ++i) {
                    posMap.put(workArr.get(i).ruleId, i);
                  }
                  dead = 0;
                }
              }
              workArr.removeIf(r -> r.activeFlag == 0);
              for (int i = 0; i < workArr.size(); ++i) {
                total += workArr.get(i).evaluateScore();
              }
              return total;
            });
    long tombCs = lastChecksum;

    LinkedList<SubscriptionRule> workList = new LinkedList<>();
    double listLinMs =
        measureMedianMs(
            2,
            5,
            () -> {
              workList.clear();
              for (SubscriptionRule r : initial) {
                workList.add(r.copyActive());
              }
            },
            () -> {
              long total = 0;
              for (int c = 0; c < unsubs; ++c) {
                if ((c % scanEvery) == 0) {
                  for (SubscriptionRule r : workList) {
                    total += r.evaluateScore();
                  }
                }
                long target = unsubIds[c];
                var it = workList.iterator();
                while (it.hasNext()) {
                  if (it.next().ruleId == target) {
                    it.remove();
                    break;
                  }
                }
                workList.add(replacements[c].copyActive());
              }
              for (SubscriptionRule r : workList) {
                total += r.evaluateScore();
              }
              return total;
            });
    long listUnsubCs = lastChecksum;

    System.out.printf(
        "  [N=8192, 512 Unsubs + 65 Sweeps] ArrList rm=%7.3f ms | sparse=%7.3f ms | tomb+rmIf=%7.3f ms | LinkedList=%7.3f ms (%.2fx vs sparse) | cs=%016x (parity=%s)%n",
        arrOrdMs,
        arrSparseMs,
        arrTombMs,
        listLinMs,
        listLinMs / arrSparseMs,
        sparseCs,
        (ordCs == sparseCs && sparseCs == tombCs && tombCs == listUnsubCs) ? "OK" : "MISMATCH");
  }

  private static void runStory1B() {
    System.out.println(
        "=== JAVA 21 LTS STORY 1B: Caffeine-Style Intrusive Dual Links vs Array Shift (16,384 Conns, 200K Events) ===");
    final int numConnections = 16384;
    final int numEvents = 200_000;
    final int hotBand = numConnections / 5;
    long[] rng = {SEED_RULES ^ 0xBEEFL};
    int[] events = new int[numEvents];
    for (int i = 0; i < numEvents; ++i) {
      long r = splitMix64(rng);
      long pick = splitMix64(rng);
      events[i] =
          (Long.remainderUnsigned(r, 100L) < 80L)
              ? (int) Long.remainderUnsigned(pick, hotBand)
              : (int) Long.remainderUnsigned(pick, numConnections);
    }

    double intrMs =
        measureMedianMs(
            2,
            5,
            () -> {},
            () -> {
              ActiveConnection[] slab = new ActiveConnection[numConnections];
              IntrusiveDualQueues queues = new IntrusiveDualQueues();
              for (int i = 0; i < numConnections; ++i) {
                slab[i] = new ActiveConnection(i + 1L, i + 10);
                queues.pushBackLru(slab[i]);
                queues.pushBackTimeout(slab[i]);
              }
              long ts = 1L;
              for (int idx : events) {
                ActiveConnection c = slab[idx];
                c.lastSeenMs = ts++;
                c.bytesRx += 64L;
                queues.unlinkLru(c);
                queues.pushFrontLru(c);
                queues.unlinkTimeout(c);
                queues.pushBackTimeout(c);
              }
              return queues.lruHead.connId
                  ^ (queues.lruTail.connId << 16)
                  ^ (queues.toHead.connId << 32)
                  ^ queues.toTail.lastSeenMs;
            });
    long intrCs = lastChecksum;

    double idxArrMs =
        measureMedianMs(
            1,
            3,
            () -> {},
            () -> {
              ActiveConnection[] slab = new ActiveConnection[numConnections];
              int[] lru = new int[numConnections];
              int[] timeouts = new int[numConnections];
              int[] lruPos = new int[numConnections];
              int[] toPos = new int[numConnections];
              for (int i = 0; i < numConnections; ++i) {
                slab[i] = new ActiveConnection(i + 1L, i + 10);
                lru[i] = i;
                timeouts[i] = i;
                lruPos[i] = i;
                toPos[i] = i;
              }
              int last = numConnections - 1;
              long ts = 1L;
              for (int idx : events) {
                ActiveConnection c = slab[idx];
                c.lastSeenMs = ts++;
                c.bytesRx += 64L;

                int lp = lruPos[idx];
                if (lp > 0) {
                  System.arraycopy(lru, 0, lru, 1, lp);
                  lru[0] = idx;
                  for (int k = 1; k <= lp; ++k) {
                    lruPos[lru[k]] = k;
                  }
                  lruPos[idx] = 0;
                }

                int tp = toPos[idx];
                if (tp < last) {
                  System.arraycopy(timeouts, tp + 1, timeouts, tp, last - tp);
                  timeouts[last] = idx;
                  for (int k = tp; k < last; ++k) {
                    toPos[timeouts[k]] = k;
                  }
                  toPos[idx] = last;
                }
              }
              return slab[lru[0]].connId
                  ^ (slab[lru[last]].connId << 16)
                  ^ (slab[timeouts[0]].connId << 32)
                  ^ slab[timeouts[last]].lastSeenMs;
            });
    long idxArrCs = lastChecksum;

    double arrMs =
        measureMedianMs(
            1,
            3,
            () -> {},
            () -> {
              ActiveConnection[] slab = new ActiveConnection[numConnections];
              ArrayList<ActiveConnection> lru = new ArrayList<>(numConnections);
              ArrayList<ActiveConnection> timeouts = new ArrayList<>(numConnections);
              for (int i = 0; i < numConnections; ++i) {
                slab[i] = new ActiveConnection(i + 1L, i + 10);
                lru.add(slab[i]);
                timeouts.add(slab[i]);
              }
              long ts = 1L;
              for (int idx : events) {
                ActiveConnection c = slab[idx];
                c.lastSeenMs = ts++;
                c.bytesRx += 64L;
                int lruPos = lru.indexOf(c);
                lru.remove(lruPos);
                lru.add(0, c);
                int toPos = timeouts.indexOf(c);
                timeouts.remove(toPos);
                timeouts.add(c);
              }
              return lru.get(0).connId
                  ^ (lru.get(lru.size() - 1).connId << 16)
                  ^ (timeouts.get(0).connId << 32)
                  ^ timeouts.get(timeouts.size() - 1).lastSeenMs;
            });
    long arrCs = lastChecksum;

    System.out.printf(
        "  Caffeine-Style Intrusive Dual Links     | median=%7.3f ms | %6.2f M ev/s | cs=%016x%n",
        intrMs, (numEvents / (intrMs / 1000.0)) / 1e6, intrCs);
    System.out.printf(
        "  int[] O(1) Idx + arraycopy Shift        | median=%7.3f ms | %6.2f M ev/s (%.2fx slower) | cs=%016x%n",
        idxArrMs, (numEvents / (idxArrMs / 1000.0)) / 1e6, idxArrMs / intrMs, idxArrCs);
    System.out.printf(
        "  ArrayList<ActiveConnection> (indexOf)   | median=%7.3f ms | %6.2f M ev/s (%.2fx slower) | cs=%016x%n",
        arrMs, (numEvents / (arrMs / 1000.0)) / 1e6, arrMs / intrMs, arrCs);
    if (intrCs != idxArrCs || intrCs != arrCs) {
      throw new AssertionError("Story 1B checksum mismatch");
    }
  }

  private static void runStory2() {
    System.out.println(
        "=== JAVA 21 LTS STORY 2A & 2B: Idle ArrayDeque Footprint & 8,000,000 FIFO Stream ===");
    final int numIdle = 100_000;
    Runtime rt = Runtime.getRuntime();
    System.gc();
    long mem0 = rt.totalMemory() - rt.freeMemory();
    ArrayList<?>[] lazyLists = new ArrayList<?>[numIdle];
    for (int i = 0; i < numIdle; ++i) {
      lazyLists[i] = new ArrayList<>();
    }
    System.gc();
    long memArrList = rt.totalMemory() - rt.freeMemory();
    keepAlive = lazyLists;

    ArrayDeque<?>[] eagerDeques = new ArrayDeque<?>[numIdle];
    for (int i = 0; i < numIdle; ++i) {
      eagerDeques[i] = new ArrayDeque<>();
    }
    System.gc();
    long memArrDeque = rt.totalMemory() - rt.freeMemory();
    keepAlive = eagerDeques;
    System.out.printf(
        "  100,000 Empty ArrayList<> (lazy shared []) : +%6.2f MiB JVM heap%n",
        (memArrList - mem0) / (1024.0 * 1024.0));
    System.out.printf(
        "  100,000 Empty ArrayDeque<> (eager Obj[17]) : +%6.2f MiB JVM heap%n",
        (memArrDeque - memArrList) / (1024.0 * 1024.0));
    keepAlive = null;

    final int depth = 1024;
    final int cycles = 8_000_000;
    final OutboundMessage[] pool = new OutboundMessage[2048];
    for (int i = 0; i < pool.length; ++i) {
      pool[i] = new OutboundMessage(i);
    }

    double adSingleMs =
        measureMedianMs(
            2,
            5,
            () -> {},
            () -> {
              ArrayDeque<OutboundMessage> q = new ArrayDeque<>(depth);
              for (int i = 0; i < depth; ++i) {
                q.addLast(new OutboundMessage(i));
              }
              long cs = 0x17F1F0L;
              for (int i = 0; i < cycles; ++i) {
                cs = mixOrderHash(cs, q.removeFirst().digest());
                q.addLast(new OutboundMessage(depth + i));
              }
              for (OutboundMessage m : q) {
                cs = mixOrderHash(cs, m.digest());
              }
              return cs;
            });
    long adSingleCs = lastChecksum;

    double llSingleMs =
        measureMedianMs(
            2,
            5,
            () -> {},
            () -> {
              LinkedList<OutboundMessage> q = new LinkedList<>();
              for (int i = 0; i < depth; ++i) {
                q.addLast(new OutboundMessage(i));
              }
              long cs = 0x17F1F0L;
              for (int i = 0; i < cycles; ++i) {
                cs = mixOrderHash(cs, q.removeFirst().digest());
                q.addLast(new OutboundMessage(depth + i));
              }
              for (OutboundMessage m : q) {
                cs = mixOrderHash(cs, m.digest());
              }
              return cs;
            });
    long llSingleCs = lastChecksum;

    double muxAdMs =
        measureMedianMs(
            2,
            5,
            () -> {},
            () -> {
              ArrayList<ArrayDeque<OutboundMessage>> queues = new ArrayList<>(1024);
              long seq = 0;
              for (int qIdx = 0; qIdx < 1024; ++qIdx) {
                ArrayDeque<OutboundMessage> q = new ArrayDeque<>(16);
                for (int d = 0; d < 16; ++d) {
                  q.addLast(new OutboundMessage(seq++));
                }
                queues.add(q);
              }
              long cs = 0x17F1F0L;
              for (int i = 0; i < cycles; ++i) {
                ArrayDeque<OutboundMessage> q = queues.get((i * 73) & 1023);
                cs = mixOrderHash(cs, q.removeFirst().digest());
                q.addLast(new OutboundMessage(seq++));
              }
              for (int qIdx = 0; qIdx < 1024; ++qIdx) {
                for (OutboundMessage m : queues.get(qIdx)) {
                  cs = mixOrderHash(cs, m.digest());
                }
              }
              return cs;
            });
    long muxAdCs = lastChecksum;

    System.out.printf(
        "  Single Queue: ArrayDeque (8M fresh)     | median=%7.3f ms | %6.2f M msgs/s | cs=%016x%n",
        adSingleMs, (cycles / (adSingleMs / 1000.0)) / 1e6, adSingleCs);
    System.out.printf(
        "  Single Queue: LinkedList (8M fresh)     | median=%7.3f ms | %6.2f M msgs/s (%.2fx slower) | cs=%016x%n",
        llSingleMs, (cycles / (llSingleMs / 1000.0)) / 1e6, llSingleMs / adSingleMs, llSingleCs);
    System.out.printf(
        "  1,024 Queues: ArrayDeque (8M fresh)     | median=%7.3f ms | %6.2f M msgs/s | cs=%016x%n",
        muxAdMs, (cycles / (muxAdMs / 1000.0)) / 1e6, muxAdCs);
    blackhole ^= pool[0].digest();
  }

  private static void runStory3() {
    System.out.println(
        "=== JAVA 21 LTS STORY 3: Gateway Reconnect Wave — 150,000 Slot-Indexed ClientSessions ===");
    final int numSessions = 150_000;

    // Warm C2 JIT thoroughly (6 passes + clean G1 young gen) so standalone
    // `--story=3` hits identical steady-state performance as full-suite runs.
    double unresMs =
        measureMedianMs(
            6,
            5,
            System::gc,
            () -> {
              ArrayList<ClientSession> sessions = new ArrayList<>();
              for (int i = 0; i < numSessions; ++i) {
                sessions.add(new ClientSession(i + 1L));
              }
              long cs = 0x5E5510L;
              for (int i = 0; i < sessions.size(); ++i) {
                cs = mixOrderHash(cs, sessions.get(i).digest());
              }
              return cs;
            });
    long unresCs = lastChecksum;

    double resMs =
        measureMedianMs(
            6,
            5,
            System::gc,
            () -> {
              ArrayList<ClientSession> sessions = new ArrayList<>(numSessions);
              for (int i = 0; i < numSessions; ++i) {
                sessions.add(new ClientSession(i + 1L));
              }
              long cs = 0x5E5510L;
              for (int i = 0; i < sessions.size(); ++i) {
                cs = mixOrderHash(cs, sessions.get(i).digest());
              }
              return cs;
            });
    long resCs = lastChecksum;

    System.out.printf(
        "  ArrayList<ClientSession> (unreserved 1.5x) | median=%7.3f ms | %5.2f M/s | cs=%016x%n",
        unresMs, (numSessions / (unresMs / 1000.0)) / 1e6, unresCs);
    System.out.printf(
        "  ArrayList<ClientSession> (pre-sized 150K)  | median=%7.3f ms | %5.2f M/s | cs=%016x%n",
        resMs, (numSessions / (resMs / 1000.0)) / 1e6, resCs);
  }

  public static void main(String[] args) {
    String story = "all";
    for (String a : args) {
      if (a.startsWith("--story=")) {
        story = a.substring("--story=".length());
      }
    }
    if (story.equals("all") || story.equals("1a")) {
      runStory1A();
    }
    if (story.equals("all") || story.equals("1b")) {
      runStory1B();
    }
    if (story.equals("all") || story.equals("2") || story.equals("2a") || story.equals("2b")) {
      runStory2();
    }
    if (story.equals("all") || story.equals("3")) {
      runStory3();
    }
  }
}
