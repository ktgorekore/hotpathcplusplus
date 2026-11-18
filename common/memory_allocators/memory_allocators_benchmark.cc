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

/**
 * @file benchmark.cc
 * @brief Episode 12 Google Benchmark suite measuring:
 * - Battle 1: `new`/`delete` vs. `glibc` `tcache` burst spillover (`7` vs.
 *   `1,024`) vs. `FixedSlabPool` vs. `BumpArena` vs. 8-thread cross-thread
 *   `ThreadMagazinePool<OrderNode, 32>`.
 * - Battle 2: Fresh Heap vs. Aged Fragmented Heap vs. `FixedSlabPool` vs.
 *   Chained `std::pmr` vs. `BumpArena`, plus `std::unordered_map` vs.
 *   `std::pmr::unordered_map` bulk "wink-out" teardown.
 * - Battle 3: C++17 `std::pmr` stack scratchpad snapshot parsing (`Std` vs.
 *   `BrokenPmrVenueFill` vs. zero-heap `PmrVenueFill`) and `virtual
 * do_allocate` dispatch calibration.
 */

#include <benchmark/benchmark.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/memory_allocators/memory_allocators.h"

namespace hotpath::memory_allocators {
namespace {

constexpr std::string_view kLongVenueExecId = "XNAS-EXEC-20261118-00042";

const HeapAgingScenario& SharedAgingScenario() {
  static const HeapAgingScenario* scenario =
      new HeapAgingScenario(kDefaultOrderCount, 524288);
  return *scenario;
}

// ============================================================================
// BATTLE 1: malloc / new Mechanics, tcache Spillover & Cross-Thread Contention
// ============================================================================

/** @brief Battle 1A: Single `new OrderNode` + `delete` (`tcache` hot hit). */
void BM_Battle1A_SinglePair_StdNewDelete(benchmark::State& state) {
  PinThreadToCore(0);
  std::uint64_t seq = 1;
  for (auto _ : state) {
    auto* node = new OrderNode{seq++, 18525LL, 100U, 1U, nullptr};
    benchmark::DoNotOptimize(node);
    delete node;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Battle1A_SinglePair_StdNewDelete);

/** @brief Battle 1A: Single `FixedSlabPool::Allocate` + `Deallocate`. */
void BM_Battle1A_SinglePair_FixedSlabPool(benchmark::State& state) {
  PinThreadToCore(0);
  FixedSlabPool<OrderNode> pool(1024);
  std::uint64_t seq = 1;
  for (auto _ : state) {
    OrderNode* node = pool.Allocate(seq++, 18525LL, 100U, 1U, nullptr);
    benchmark::DoNotOptimize(node);
    pool.Deallocate(node);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Battle1A_SinglePair_FixedSlabPool);

/** @brief Battle 1A: Single 2-instruction `BumpArena::Create` (`Reset` outside
 * tight cap). */
void BM_Battle1A_SingleAlloc_BumpArena(benchmark::State& state) {
  PinThreadToCore(0);
  BumpArena arena(kHugePageBytes);
  std::uint64_t seq = 1;
  std::size_t count = 0;
  for (auto _ : state) {
    OrderNode* node =
        arena.Create<OrderNode>(seq++, 18525LL, 100U, 1U, nullptr);
    benchmark::DoNotOptimize(node);
    if (++count == 65536) [[unlikely]] {
      arena.Reset();
      count = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Battle1A_SingleAlloc_BumpArena);

/**
 * @brief Battle 1B: Burst batch `new`/`delete` inside `glibc` `7`-chunk
 * `tcache` bin (`Arg(7)`) vs. spilling into `fastbins`/bins (`Arg(1024)`).
 */
void BM_Battle1B_BatchBurst_StdNewDelete(benchmark::State& state) {
  PinThreadToCore(0);
  const std::size_t batch = static_cast<std::size_t>(state.range(0));
  std::vector<OrderNode*> ptrs(batch, nullptr);
  std::uint64_t seq = 1;
  for (auto _ : state) {
    for (std::size_t i = 0; i < batch; ++i) {
      ptrs[i] = new OrderNode{seq++, 18525LL, 100U, 1U, nullptr};
    }
    benchmark::DoNotOptimize(ptrs.data());
    for (std::size_t i = 0; i < batch; ++i) {
      delete ptrs[i];
    }
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(batch));
}
BENCHMARK(BM_Battle1B_BatchBurst_StdNewDelete)->Arg(7)->Arg(64)->Arg(1024);

/** @brief Battle 1B: Burst batch inside `FixedSlabPool<OrderNode>`. */
void BM_Battle1B_BatchBurst_FixedSlabPool(benchmark::State& state) {
  PinThreadToCore(0);
  const std::size_t batch = static_cast<std::size_t>(state.range(0));
  FixedSlabPool<OrderNode> pool(batch + 64);
  std::vector<OrderNode*> ptrs(batch, nullptr);
  std::uint64_t seq = 1;
  for (auto _ : state) {
    for (std::size_t i = 0; i < batch; ++i) {
      ptrs[i] = pool.Allocate(seq++, 18525LL, 100U, 1U, nullptr);
    }
    benchmark::DoNotOptimize(ptrs.data());
    for (std::size_t i = 0; i < batch; ++i) {
      pool.Deallocate(ptrs[i]);
    }
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(batch));
}
BENCHMARK(BM_Battle1B_BatchBurst_FixedSlabPool)->Arg(7)->Arg(64)->Arg(1024);

/** @brief Battle 1B: Burst batch using `BumpArena` + `O(1)` `Reset()`. */
void BM_Battle1B_BatchBurst_BumpArenaReset(benchmark::State& state) {
  PinThreadToCore(0);
  const std::size_t batch = static_cast<std::size_t>(state.range(0));
  BumpArena arena(batch * sizeof(OrderNode) + kPageBytes4KiB);
  std::vector<OrderNode*> ptrs(batch, nullptr);
  std::uint64_t seq = 1;
  for (auto _ : state) {
    for (std::size_t i = 0; i < batch; ++i) {
      ptrs[i] = arena.Create<OrderNode>(seq++, 18525LL, 100U, 1U, nullptr);
    }
    benchmark::DoNotOptimize(ptrs.data());
    arena.Reset();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(batch));
}
BENCHMARK(BM_Battle1B_BatchBurst_BumpArenaReset)->Arg(7)->Arg(64)->Arg(1024);

/**
 * @brief Battle 1C: Asymmetric 4-Producer / 4-Consumer cross-thread handoff via
 * standard `new` (Thread A) and `delete` (Thread B) over lock-free SPSC rings.
 */
void BM_Battle1C_CrossThread_AsymmetricStdNewDelete(benchmark::State& state) {
  constexpr std::size_t kPairs = 4;
  constexpr std::size_t kBatchItems = 65536;

  for (auto _ : state) {
    std::vector<SpscRingQueue<OrderNode*, 4096>> rings(kPairs);
    std::atomic<bool> go{false};
    std::vector<std::thread> workers;
    workers.reserve(kPairs * 2);

    for (std::size_t p = 0; p < kPairs; ++p) {
      workers.emplace_back([&, p]() {
        PinThreadToCore(static_cast<int>(p * 2));
        while (!go.load(std::memory_order_acquire)) {
        }
        for (std::size_t i = 0; i < kBatchItems; ++i) {
          auto* node = new OrderNode{i, 18525LL, 100U,
                                     static_cast<std::uint32_t>(p), nullptr};
          while (!rings[p].TryPush(node)) {
          }
        }
      });
      workers.emplace_back([&, p]() {
        PinThreadToCore(static_cast<int>(p * 2 + 1));
        while (!go.load(std::memory_order_acquire)) {
        }
        std::size_t done = 0;
        std::uint64_t sum = 0;
        while (done < kBatchItems) {
          OrderNode* node = nullptr;
          if (rings[p].TryPop(node)) {
            sum += static_cast<std::uint64_t>(node->price_ticks);
            delete node;
            ++done;
          }
        }
        benchmark::DoNotOptimize(sum);
      });
    }

    const auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (auto& w : workers) {
      w.join();
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kPairs * kBatchItems));
}
BENCHMARK(BM_Battle1C_CrossThread_AsymmetricStdNewDelete)->UseManualTime();

/** @brief Battle 1C: Asymmetric 4P/4C handoff using `ThreadMagazinePool<32>`.
 */
void BM_Battle1C_CrossThread_ThreadMagazinePool(benchmark::State& state) {
  constexpr std::size_t kPairs = 4;
  constexpr std::size_t kBatchItems = 65536;
  ThreadMagazinePool<OrderNode, 32> pool(kPairs * 4096);

  for (auto _ : state) {
    std::vector<SpscRingQueue<OrderNode*, 4096>> rings(kPairs);
    std::atomic<bool> go{false};
    std::vector<std::thread> workers;
    workers.reserve(kPairs * 2);

    for (std::size_t p = 0; p < kPairs; ++p) {
      workers.emplace_back([&, p]() {
        PinThreadToCore(static_cast<int>(p * 2));
        ThreadMagazinePool<OrderNode, 32>::LocalCache cache(pool);
        while (!go.load(std::memory_order_acquire)) {
        }
        for (std::size_t i = 0; i < kBatchItems; ++i) {
          OrderNode* node = pool.Allocate(
              cache, i, 18525LL, 100U, static_cast<std::uint32_t>(p), nullptr);
          while (!rings[p].TryPush(node)) {
          }
        }
      });
      workers.emplace_back([&, p]() {
        PinThreadToCore(static_cast<int>(p * 2 + 1));
        ThreadMagazinePool<OrderNode, 32>::LocalCache cache(pool);
        while (!go.load(std::memory_order_acquire)) {
        }
        std::size_t done = 0;
        std::uint64_t sum = 0;
        while (done < kBatchItems) {
          OrderNode* node = nullptr;
          if (rings[p].TryPop(node)) {
            sum += static_cast<std::uint64_t>(node->price_ticks);
            pool.Deallocate(cache, node);
            ++done;
          }
        }
        benchmark::DoNotOptimize(sum);
      });
    }

    const auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (auto& w : workers) {
      w.join();
    }
    const auto t1 = std::chrono::steady_clock::now();
    state.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kPairs * kBatchItems));
}
BENCHMARK(BM_Battle1C_CrossThread_ThreadMagazinePool)->UseManualTime();

// ============================================================================
// BATTLE 2: Spatial Locality, Heap Aging & Node-Container Teardown Physics
// ============================================================================

/** @brief Battle 2A: Fresh startup heap scan (sequential `48B` stride
 * illusion).
 */
void BM_Battle2A_OrderChainScan_FreshHeapIllusion(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = SharedAgingScenario();
  for (auto _ : state) {
    std::uint64_t checksum = ScanOrderChain(scenario.fresh_head());
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(scenario.order_count()));
}
BENCHMARK(BM_Battle2A_OrderChainScan_FreshHeapIllusion);

/** @brief Battle 2B: Aged fragmented heap scan (post mixed-size churn). */
void BM_Battle2B_OrderChainScan_AgedFragmentedHeap(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = SharedAgingScenario();
  for (auto _ : state) {
    std::uint64_t checksum = ScanOrderChain(scenario.aged_head());
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(scenario.order_count()));
}
BENCHMARK(BM_Battle2B_OrderChainScan_AgedFragmentedHeap);

/** @brief Battle 2C: Isolated `FixedSlabPool<OrderNode>` scan after churn. */
void BM_Battle2C_OrderChainScan_FixedSlabPool(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = SharedAgingScenario();
  for (auto _ : state) {
    std::uint64_t checksum = ScanOrderChain(scenario.slab_head());
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(scenario.order_count()));
}
BENCHMARK(BM_Battle2C_OrderChainScan_FixedSlabPool);

/** @brief Battle 2D: Chained `std::pmr::unsynchronized_pool_resource` scan. */
void BM_Battle2D_OrderChainScan_ChainedPmrPool(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = SharedAgingScenario();
  for (auto _ : state) {
    std::uint64_t checksum = ScanOrderChain(scenario.pmr_head());
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(scenario.order_count()));
}
BENCHMARK(BM_Battle2D_OrderChainScan_ChainedPmrPool);

/** @brief Battle 2E: Compacted `BumpArena` scan (`32B` contiguous packing). */
void BM_Battle2E_OrderChainScan_BumpArenaCompacted(benchmark::State& state) {
  PinThreadToCore(0);
  const auto& scenario = SharedAgingScenario();
  for (auto _ : state) {
    std::uint64_t checksum = ScanOrderChain(scenario.arena_head());
    benchmark::DoNotOptimize(checksum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(scenario.order_count()));
}
BENCHMARK(BM_Battle2E_OrderChainScan_BumpArenaCompacted);

/**
 * @brief Battle 2F: Building, probing, and destroying `std::unordered_map` of
 * `4,096` orders using default global `std::allocator` (node-by-node `free`).
 */
void BM_Battle2F_UnorderedMap_StdAllocatorLifecycle(benchmark::State& state) {
  PinThreadToCore(0);
  constexpr std::size_t kEntries = 4096;
  for (auto _ : state) {
    std::unordered_map<std::uint64_t, OrderNode> map;
    map.reserve(kEntries);
    for (std::size_t i = 0; i < kEntries; ++i) {
      map.emplace(i, OrderNode{i, 18525LL, 100U, 1U, nullptr});
    }
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < kEntries; ++i) {
      sum += static_cast<std::uint64_t>(map.find(i)->second.price_ticks);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kEntries));
}
BENCHMARK(BM_Battle2F_UnorderedMap_StdAllocatorLifecycle);

/**
 * @brief Battle 2G: Identical `4,096`-order `std::pmr::unordered_map` backed by
 * `unsynchronized_pool_resource` over a reused pre-faulted monotonic buffer.
 */
void BM_Battle2G_UnorderedMap_PmrPoolOnArenaLifecycle(benchmark::State& state) {
  PinThreadToCore(0);
  constexpr std::size_t kEntries = 4096;
  BumpArena backing(512ULL * 1024ULL, true);
  void* buf = backing.AllocateBytes(backing.Capacity(), kCacheLineBytes);
  for (auto _ : state) {
    std::pmr::monotonic_buffer_resource mono(buf, backing.Capacity(),
                                             std::pmr::null_memory_resource());
    std::pmr::unsynchronized_pool_resource pool(&mono);
    std::pmr::unordered_map<std::uint64_t, OrderNode> map(&pool);
    map.reserve(kEntries);
    for (std::size_t i = 0; i < kEntries; ++i) {
      map.emplace(i, OrderNode{i, 18525LL, 100U, 1U, nullptr});
    }
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < kEntries; ++i) {
      sum += static_cast<std::uint64_t>(map.find(i)->second.price_ticks);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kEntries));
}
BENCHMARK(BM_Battle2G_UnorderedMap_PmrPoolOnArenaLifecycle);

/**
 * @brief Battle 2H: Short-lived request-scoped `std::pmr::unordered_map` backed
 * directly by `monotonic_buffer_resource` (`O(1)` no-op deallocation).
 */
void BM_Battle2H_UnorderedMap_PmrMonotonicWinkOut(benchmark::State& state) {
  PinThreadToCore(0);
  constexpr std::size_t kEntries = 4096;
  BumpArena backing(512ULL * 1024ULL, true);
  void* buf = backing.AllocateBytes(backing.Capacity(), kCacheLineBytes);
  for (auto _ : state) {
    std::pmr::monotonic_buffer_resource mono(buf, backing.Capacity(),
                                             std::pmr::null_memory_resource());
    std::pmr::unordered_map<std::uint64_t, OrderNode> map(&mono);
    map.reserve(kEntries);
    for (std::size_t i = 0; i < kEntries; ++i) {
      map.emplace(i, OrderNode{i, 18525LL, 100U, 1U, nullptr});
    }
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < kEntries; ++i) {
      sum += static_cast<std::uint64_t>(map.find(i)->second.price_ticks);
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<std::int64_t>(kEntries));
}
BENCHMARK(BM_Battle2H_UnorderedMap_PmrMonotonicWinkOut);

// ============================================================================
// BATTLE 3: Modern C++17 std::pmr Stack Buffers, Traps & Virtual Calibration
// ============================================================================

/** @brief Battle 3A: Standard `std::vector<StdVenueFill>` (`25` heap allocs).
 */
void BM_Battle3A_SnapshotParser_StdVectorStdString(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    std::vector<StdVenueFill> fills;
    fills.reserve(24);
    for (std::size_t i = 0; i < 24; ++i) {
      fills.push_back(
          StdVenueFill{i, 18525LL, 100U, 1U, std::string(kLongVenueExecId)});
    }
    benchmark::DoNotOptimize(fills.data());
  }
  state.SetItemsProcessed(state.iterations() * 24);
}
BENCHMARK(BM_Battle3A_SnapshotParser_StdVectorStdString);

/** @brief Battle 3B: Broken `std::pmr` fill missing `allocator_type` (`24`
 * leaks).
 */
void BM_Battle3B_SnapshotParser_PmrBrokenStringTrap(benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    std::array<std::byte, 4096> stack_buf{};
    std::pmr::monotonic_buffer_resource mono(stack_buf.data(),
                                             stack_buf.size());
    std::pmr::vector<BrokenPmrVenueFill> fills(&mono);
    fills.reserve(24);
    for (std::size_t i = 0; i < 24; ++i) {
      fills.emplace_back(i, 18525LL, 100U, 1U, kLongVenueExecId);
    }
    benchmark::DoNotOptimize(fills.data());
  }
  state.SetItemsProcessed(state.iterations() * 24);
}
BENCHMARK(BM_Battle3B_SnapshotParser_PmrBrokenStringTrap);

/** @brief Battle 3C: Allocator-aware `PmrVenueFill` on `4 KiB` stack buffer
 * (`0` heap allocs). */
void BM_Battle3C_SnapshotParser_PmrStackMonotonicZeroHeap(
    benchmark::State& state) {
  PinThreadToCore(0);
  for (auto _ : state) {
    std::array<std::byte, 4096> stack_buf{};
    std::pmr::monotonic_buffer_resource mono(stack_buf.data(), stack_buf.size(),
                                             std::pmr::null_memory_resource());
    std::pmr::vector<PmrVenueFill> fills(&mono);
    fills.reserve(24);
    for (std::size_t i = 0; i < 24; ++i) {
      fills.emplace_back(i, 18525LL, 100U, 1U, kLongVenueExecId);
    }
    benchmark::DoNotOptimize(fills.data());
  }
  state.SetItemsProcessed(state.iterations() * 24);
}
BENCHMARK(BM_Battle3C_SnapshotParser_PmrStackMonotonicZeroHeap);

/** @brief Battle 3D: Raw inlined `BumpArena::AllocateBytes` (no virtual call).
 */
void BM_Battle3D_DispatchCalibration_RawInlinedBumpArena(
    benchmark::State& state) {
  PinThreadToCore(0);
  BumpArena arena(8192);
  for (auto _ : state) {
    for (std::size_t i = 0; i < 64; ++i) {
      void* p = arena.AllocateBytes(sizeof(OrderNode), alignof(OrderNode));
      benchmark::DoNotOptimize(p);
    }
    arena.Reset();
  }
  state.SetItemsProcessed(state.iterations() * 64);
}
BENCHMARK(BM_Battle3D_DispatchCalibration_RawInlinedBumpArena);

/** @brief Battle 3E: `std::pmr::monotonic_buffer_resource` virtual dispatch. */
void BM_Battle3E_DispatchCalibration_PmrVirtualMonotonic(
    benchmark::State& state) {
  PinThreadToCore(0);
  std::array<std::byte, 8192> buf{};
  for (auto _ : state) {
    std::pmr::monotonic_buffer_resource mono(buf.data(), buf.size(),
                                             std::pmr::null_memory_resource());
    std::pmr::memory_resource* res = &mono;
    for (std::size_t i = 0; i < 64; ++i) {
      void* p = res->allocate(sizeof(OrderNode), alignof(OrderNode));
      benchmark::DoNotOptimize(p);
    }
  }
  state.SetItemsProcessed(state.iterations() * 64);
}
BENCHMARK(BM_Battle3E_DispatchCalibration_PmrVirtualMonotonic);

}  // namespace
}  // namespace hotpath::memory_allocators
