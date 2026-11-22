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
 * @file challenge.cc
 * @brief Episode 12 Forensic Challenge & Hardware PMU (`perf stat`) Runner.
 *
 * Exports unmangled `extern "C"` hot-path functions for `llvm-objdump`
 * inspection and supports isolated `--mode=` subcommands so two-pass
 * unmultiplexed Linux `perf stat` runs isolate each hardware bottleneck
 * cleanly.
 */

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory_resource>
#include <string_view>
#include <thread>
#include <vector>

#include "common/memory_allocators/memory_allocators.h"

namespace hotpath::memory_allocators {

extern "C" {

/** @brief Unmangled symbol for disassembly: standard `new` + `delete`. */
[[gnu::noinline]] std::uint64_t ep12_alloc_free_std_new(
    std::uint64_t id) noexcept {
  auto* node = new OrderNode{id, 18525, 100U, 1U, nullptr};
  DoNotOptimize(node);
  const std::uint64_t px = static_cast<std::uint64_t>(node->price_ticks);
  delete node;
  return px;
}

/** @brief Unmangled symbol for disassembly: `BumpArena::Create<OrderNode>`. */
[[gnu::noinline]] OrderNode* ep12_alloc_bump_arena(BumpArena& arena,
                                                   std::uint64_t id) noexcept {
  return arena.Create<OrderNode>(id, 18525LL, 100U, 1U, nullptr);
}

/** @brief Unmangled symbol for disassembly: `FixedSlabPool::Allocate`. */
[[gnu::noinline]] OrderNode* ep12_alloc_fixed_slab(
    FixedSlabPool<OrderNode>& pool, std::uint64_t id) noexcept {
  return pool.Allocate(id, 18525LL, 100U, 1U, nullptr);
}

/** @brief Unmangled symbol for disassembly: `std::pmr::memory_resource`. */
[[gnu::noinline]] void* ep12_alloc_pmr_resource(
    std::pmr::memory_resource* res) noexcept {
  return res->allocate(sizeof(OrderNode), alignof(OrderNode));
}

}  // extern "C"

namespace {

constexpr std::string_view kLongVenueExecId = "XNAS-EXEC-20261118-00042";

template <typename Fn>
double ElapsedNs(Fn&& fn) {
  const auto t0 = std::chrono::steady_clock::now();
  fn();
  const auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

void RunBatchBurst(std::size_t batch_size, std::size_t total_ops,
                   bool use_arena) {
  const std::size_t batches = total_ops / batch_size;
  std::vector<OrderNode*> ptrs(batch_size, nullptr);
  BumpArena arena(batch_size * sizeof(OrderNode) + 4096);
  std::uint64_t acc = 0;

  const double ns = ElapsedNs([&]() {
    if (use_arena) {
      for (std::size_t b = 0; b < batches; ++b) {
        for (std::size_t i = 0; i < batch_size; ++i) {
          ptrs[i] = arena.Create<OrderNode>(b * batch_size + i, 18525LL, 100U,
                                            1U, nullptr);
        }
        DoNotOptimize(ptrs.data());
        acc += static_cast<std::uint64_t>(ptrs[0]->price_ticks);
        arena.Reset();
      }
    } else {
      for (std::size_t b = 0; b < batches; ++b) {
        for (std::size_t i = 0; i < batch_size; ++i) {
          ptrs[i] =
              new OrderNode{b * batch_size + i, 18525LL, 100U, 1U, nullptr};
        }
        DoNotOptimize(ptrs.data());
        acc += static_cast<std::uint64_t>(ptrs[0]->price_ticks);
        for (std::size_t i = 0; i < batch_size; ++i) {
          delete ptrs[i];
        }
      }
    }
  });
  DoNotOptimize(acc);
  std::printf("[B1] %-26s batch=%4zu ops=%8zu -> %6.2f ns/op\n",
              use_arena ? "BumpArena+O(1) Reset" : "std new/delete burst",
              batch_size, batches * batch_size,
              ns / static_cast<double>(batches * batch_size));
}

void RunCrossThread(bool use_magazines, std::size_t pairs,
                    std::size_t items_per_pair) {
  ThreadMagazinePool<OrderNode, 32> pool(pairs * 4096);
  std::vector<SpscRingQueue<OrderNode*, 4096>> rings(pairs);
  std::atomic<bool> start{false};

  std::vector<std::thread> threads;
  threads.reserve(pairs * 2);

  for (std::size_t p = 0; p < pairs; ++p) {
    threads.emplace_back([&, p]() {
      PinThreadToCore(static_cast<int>(p * 2));
      typename ThreadMagazinePool<OrderNode, 32>::LocalCache cache(pool);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (std::size_t i = 0; i < items_per_pair; ++i) {
        OrderNode* node =
            use_magazines
                ? pool.Allocate(cache, i, 18525LL, 100U,
                                static_cast<std::uint32_t>(p), nullptr)
                : new OrderNode{i, 18525LL, 100U, static_cast<std::uint32_t>(p),
                                nullptr};
        while (!rings[p].TryPush(node)) {
        }
      }
    });

    threads.emplace_back([&, p]() {
      PinThreadToCore(static_cast<int>(p * 2 + 1));
      typename ThreadMagazinePool<OrderNode, 32>::LocalCache cache(pool);
      while (!start.load(std::memory_order_acquire)) {
      }
      std::uint64_t sum = 0;
      std::size_t done = 0;
      while (done < items_per_pair) {
        OrderNode* node = nullptr;
        if (rings[p].TryPop(node)) {
          sum += static_cast<std::uint64_t>(node->price_ticks);
          if (use_magazines) {
            pool.Deallocate(cache, node);
          } else {
            delete node;
          }
          ++done;
        }
      }
      DoNotOptimize(sum);
    });
  }

  const auto t0 = std::chrono::steady_clock::now();
  start.store(true, std::memory_order_release);
  for (auto& t : threads) {
    t.join();
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double total_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count();
  const double total_ops = static_cast<double>(pairs * items_per_pair);
  const double mops = (total_ops / total_ns) * 1000.0;
  std::printf(
      "[B1C] %-25s (%zuP/%zuC) -> %6.2f ns/op agg | %7.2f M ops/s\n",
      use_magazines ? "ThreadMagazinePool<32>" : "Cross-Thread new/delete",
      pairs, pairs, total_ns / total_ops, mops);
}

void RunScanMode(std::string_view mode, std::size_t passes) {
  HeapAgingScenario scenario(kDefaultOrderCount, 524288);
  const OrderNode* head = scenario.fresh_head();
  const char* label = "Fresh Heap Scan";
  if (mode == "b2_aged_scan") {
    head = scenario.aged_head();
    label = "Aged Fragmented Heap";
  } else if (mode == "b2_slab_scan") {
    head = scenario.slab_head();
    label = "FixedSlabPool (2 MiB)";
  } else if (mode == "b2_pmr_scan") {
    head = scenario.pmr_head();
    label = "Chained std::pmr Pool";
  } else if (mode == "b2_arena_scan") {
    head = scenario.arena_head();
    label = "BumpArena (32B packed)";
  }

  std::uint64_t acc = 0;
  const double ns = ElapsedNs([&]() {
    for (std::size_t p = 0; p < passes; ++p) {
      acc ^= ScanOrderChain(head);
    }
  });
  DoNotOptimize(acc);
  const double per_node =
      ns / static_cast<double>(passes * scenario.order_count());
  const double per_scan_us = (ns / static_cast<double>(passes)) / 1000.0;
  std::printf("[B2] %-26s (%zu passes) -> %7.2f us/scan (%5.2f ns/node)\n",
              label, passes, per_scan_us, per_node);
}

void RunParseMode(std::string_view mode, std::size_t packets) {
  std::uint64_t acc = 0;
  const double ns = ElapsedNs([&]() {
    if (mode == "b3_std_parse") {
      for (std::size_t p = 0; p < packets; ++p) {
        std::vector<StdVenueFill> fills;
        fills.reserve(24);
        for (std::size_t i = 0; i < 24; ++i) {
          fills.push_back(StdVenueFill{p * 24 + i, 18525, 100, 1,
                                       std::string(kLongVenueExecId)});
        }
        acc += fills[0].order_id + fills[23].venue_exec_id.size();
        DoNotOptimize(fills.data());
      }
    } else if (mode == "b3_broken_pmr") {
      for (std::size_t p = 0; p < packets; ++p) {
        std::array<std::byte, 4096> stack_buf{};
        std::pmr::monotonic_buffer_resource arena(stack_buf.data(),
                                                  stack_buf.size());
        std::pmr::vector<BrokenPmrVenueFill> fills(&arena);
        fills.reserve(24);
        for (std::size_t i = 0; i < 24; ++i) {
          fills.emplace_back(p * 24 + i, 18525, 100, 1, kLongVenueExecId);
        }
        acc += fills[0].order_id + fills[23].venue_exec_id.size();
        DoNotOptimize(fills.data());
      }
    } else {
      for (std::size_t p = 0; p < packets; ++p) {
        std::array<std::byte, 4096> stack_buf{};
        std::pmr::monotonic_buffer_resource arena(
            stack_buf.data(), stack_buf.size(),
            std::pmr::null_memory_resource());
        std::pmr::vector<PmrVenueFill> fills(&arena);
        fills.reserve(24);
        for (std::size_t i = 0; i < 24; ++i) {
          fills.emplace_back(p * 24 + i, 18525, 100, 1, kLongVenueExecId);
        }
        acc += fills[0].order_id + fills[23].venue_exec_id.size();
        DoNotOptimize(fills.data());
      }
    }
  });
  DoNotOptimize(acc);
  std::printf("[B3] %-26s (%zu pkts) -> %7.1f ns/packet\n", mode.data(),
              packets, ns / static_cast<double>(packets));
}

}  // namespace

/**
 * @brief Dispatches Episode 12 memory allocator forensic challenge modes.
 * @param argc Argument count passed from process main().
 * @param argv Argument vector passed from process main().
 * @return Process exit code (0 on success).
 */
int RunChallengeCli(int argc, char** argv) {
  std::string_view mode = "all";
  std::size_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg.rfind("--mode=", 0) == 0) {
      mode = arg.substr(7);
    } else if (arg.rfind("--iters=", 0) == 0) {
      iters = std::strtoull(arg.substr(8).data(), nullptr, 10);
    } else {
      mode = arg;
    }
  }

  if (mode == "b1_tcache7") {
    PinThreadToCore(0);
    RunBatchBurst(7, iters > 0 ? iters : 14000000ULL, false);
  } else if (mode == "b1_tcache1024") {
    PinThreadToCore(0);
    RunBatchBurst(1024, iters > 0 ? iters : 16384000ULL, false);
  } else if (mode == "b1_arena1024") {
    PinThreadToCore(0);
    RunBatchBurst(1024, iters > 0 ? iters : 16384000ULL, true);
  } else if (mode == "b1_xthread_std") {
    RunCrossThread(false, 4, iters > 0 ? iters : 500000ULL);
  } else if (mode == "b1_xthread_mag") {
    RunCrossThread(true, 4, iters > 0 ? iters : 500000ULL);
  } else if (mode == "b2_fresh_scan" || mode == "b2_aged_scan" ||
             mode == "b2_slab_scan" || mode == "b2_pmr_scan" ||
             mode == "b2_arena_scan") {
    PinThreadToCore(0);
    RunScanMode(mode, iters > 0 ? iters : 2048ULL);
  } else if (mode == "b3_std_parse" || mode == "b3_broken_pmr" ||
             mode == "b3_pmr_stack") {
    PinThreadToCore(0);
    RunParseMode(mode, iters > 0 ? iters : 200000ULL);
  } else {
    PinThreadToCore(0);
    std::printf(
        "=== EPISODE 12 FORENSIC CHALLENGE: ALLOCATORS, ARENAS & std::pmr "
        "===\n");
    RunBatchBurst(7, 7000000ULL, false);
    RunBatchBurst(1024, 8192000ULL, false);
    RunBatchBurst(1024, 8192000ULL, true);
    RunCrossThread(false, 4, 250000ULL);
    RunCrossThread(true, 4, 250000ULL);
    PinThreadToCore(0);
    RunScanMode("b2_fresh_scan", 128);
    RunScanMode("b2_aged_scan", 128);
    RunScanMode("b2_slab_scan", 128);
    RunScanMode("b2_pmr_scan", 128);
    RunScanMode("b2_arena_scan", 128);
    RunParseMode("b3_std_parse", 100000);
    RunParseMode("b3_broken_pmr", 100000);
    RunParseMode("b3_pmr_stack", 100000);
  }
  return 0;
}

}  // namespace hotpath::memory_allocators

int main(int argc, char** argv) {
  return hotpath::memory_allocators::RunChallengeCli(argc, argv);
}
