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
 * @file live_heap_aging_repro.cc
 * @brief Episode 12 Cold-Open Forensic Reproducer:
 *
 * 1. Part A — The Fresh vs. Aged Heap Illusion:
 *    Measures the exact same `ScanOrderChain()` function over `65,536`
 *    32-byte `OrderNode`s allocated sequentially at startup (`Fresh Heap`),
 *    after realistic mixed-size market/audit churn (`Aged Heap`), inside an
 *    isolated `FixedSlabPool<OrderNode>`, and inside a `BumpArena`. Prints
 *    exact chunk pointer strides (`48B` glibc stride vs. `32B` arena stride vs.
 *    megabyte jumps across aged `4 KiB` pages).
 *
 * 2. Part B — The Silent `std::pmr` Nested-String Heap Escape:
 *    Counts global `::operator new` calls during `ParseMarketSnapshot()` across
 *    `std::vector<StdVenueFill>`, `std::pmr::vector<BrokenPmrVenueFill>`, and
 *    allocator-aware `std::pmr::vector<PmrVenueFill>` on a `4 KiB` stack
 * buffer.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string_view>
#include <vector>

#include "common/memory_allocators/memory_allocators.h"

namespace {
thread_local std::uint64_t g_tl_new_calls = 0;
thread_local std::uint64_t g_tl_new_bytes = 0;
}  // namespace

void* operator new(std::size_t size) {
  ++g_tl_new_calls;
  g_tl_new_bytes += size;
  void* ptr = std::malloc(size == 0 ? 1 : size);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

void* operator new(std::size_t size, std::align_val_t align) {
  ++g_tl_new_calls;
  g_tl_new_bytes += size;
  const std::size_t a =
      std::max(static_cast<std::size_t>(align), sizeof(void*));
  const std::size_t aligned_size = ((size == 0 ? 1 : size) + a - 1) & ~(a - 1);
  void* ptr = std::aligned_alloc(a, aligned_size);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

void operator delete(void* ptr) noexcept { std::free(ptr); }

void operator delete(void* ptr, std::size_t /*size*/) noexcept {
  std::free(ptr);
}

void operator delete(void* ptr, std::align_val_t /*align*/) noexcept {
  std::free(ptr);
}

void operator delete(void* ptr, std::size_t /*size*/,
                     std::align_val_t /*align*/) noexcept {
  std::free(ptr);
}

namespace hotpath::memory_allocators {
namespace {

constexpr std::string_view kExecId = "XNAS-EXEC-20261118-00042";
constexpr std::size_t kPacketFills = 24;
constexpr std::size_t kScanPasses = 64;

template <typename Fn>
double MeasureNsPerNode(Fn&& fn, std::size_t passes, std::size_t nodes) {
  // Warm up branch predictor and instruction cache slightly? No, run enough
  // passes directly so cold/warm hierarchy effects match production loops.
  const auto t0 = std::chrono::steady_clock::now();
  std::uint64_t acc = 0;
  for (std::size_t p = 0; p < passes; ++p) {
    acc ^= fn();
  }
  const auto t1 = std::chrono::steady_clock::now();
  DoNotOptimize(acc);
  const double total_ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count();
  return total_ns / static_cast<double>(passes * nodes);
}

void PrintChainReport(const char* label, const OrderNode* head,
                      std::size_t order_count) {
  const StrideStats stats = MeasureChainStride(head);
  const double ns_per_node = MeasureNsPerNode(
      [head]() { return ScanOrderChain(head); }, kScanPasses, order_count);
  const double scan_us =
      (ns_per_node * static_cast<double>(order_count)) / 1000.0;
  std::printf(
      "  %-32s | %7.2f us/scan (%5.2f ns/node) | <=64B stride: %5.1f%% | "
      "same 4KiB page: %5.1f%% | avg jump: %9.1f B\n",
      label, scan_us, ns_per_node, stats.pct_same_cacheline_stride,
      stats.pct_same_4k_page, stats.avg_abs_jump_bytes);
}

void RunHeapAgingDemo() {
  std::printf(
      "========================================================================"
      "\n");
  std::printf(
      "PART A: THE FRESH HEAP ILLUSION vs. AGED PRODUCTION HEAP (65,536 "
      "Orders)\n");
  std::printf(
      "========================================================================"
      "\n");

  // Show exact glibc vs arena pointer delta on two back-to-back OrderNodes.
  auto* g1 = new OrderNode{};
  auto* g2 = new OrderNode{};
  const auto glibc_stride = reinterpret_cast<std::uintptr_t>(g2) -
                            reinterpret_cast<std::uintptr_t>(g1);
  delete g1;
  delete g2;

  BumpArena demo_arena(4096);
  auto* a1 = demo_arena.Create<OrderNode>();
  auto* a2 = demo_arena.Create<OrderNode>();
  const auto arena_stride = reinterpret_cast<std::uintptr_t>(a2) -
                            reinterpret_cast<std::uintptr_t>(a1);
  std::printf(
      "  sizeof(OrderNode): %zu B | Fresh new OrderNode stride: %zu B | "
      "BumpArena/Slab stride: %zu B\n\n",
      sizeof(OrderNode), static_cast<std::size_t>(glibc_stride),
      static_cast<std::size_t>(arena_stride));

  HeapAgingScenario scenario(kDefaultOrderCount, 524288);
  PrintChainReport("1. Fresh Startup Heap (new)", scenario.fresh_head(),
                   scenario.order_count());
  PrintChainReport("2. Aged Fragmented Heap (new)", scenario.aged_head(),
                   scenario.order_count());
  PrintChainReport("3. Isolated FixedSlabPool", scenario.slab_head(),
                   scenario.order_count());
  PrintChainReport("4. Chained std::pmr Pool+Arena", scenario.pmr_head(),
                   scenario.order_count());
  PrintChainReport("5. Compacted BumpArena (32B)", scenario.arena_head(),
                   scenario.order_count());
}

void RunPmrStringTrapDemo() {
  std::printf(
      "\n======================================================================"
      "==\n");
  std::printf(
      "PART B: THE SILENT std::pmr NESTED-STRING HEAP ESCAPE (24 "
      "Fills/Packet)\n");
  std::printf(
      "========================================================================"
      "\n");

  // 1. Standard heap std::vector<StdVenueFill>
  g_tl_new_calls = 0;
  g_tl_new_bytes = 0;
  {
    std::vector<StdVenueFill> fills;
    fills.reserve(kPacketFills);
    for (std::size_t i = 0; i < kPacketFills; ++i) {
      fills.push_back(StdVenueFill{i + 1, 18525, 100, 1, std::string(kExecId)});
    }
    DoNotOptimize(fills.data());
  }
  std::printf(
      "  1. std::vector<StdVenueFill>         : %2llu global ::operator new "
      "calls (%llu bytes)\n",
      static_cast<unsigned long long>(g_tl_new_calls),
      static_cast<unsigned long long>(g_tl_new_bytes));

  // 2. Broken std::pmr::vector<BrokenPmrVenueFill> on 4 KiB stack buffer
  g_tl_new_calls = 0;
  g_tl_new_bytes = 0;
  {
    std::array<std::byte, 4096> stack_buf{};
    std::pmr::monotonic_buffer_resource arena(stack_buf.data(),
                                              stack_buf.size());
    std::pmr::vector<BrokenPmrVenueFill> fills(&arena);
    fills.reserve(kPacketFills);
    for (std::size_t i = 0; i < kPacketFills; ++i) {
      fills.emplace_back(i + 1, 18525, 100, 1, kExecId);
    }
    DoNotOptimize(fills.data());
  }
  std::printf(
      "  2. pmr::vector<BrokenPmrVenueFill>   : %2llu global ::operator new "
      "calls (%llu bytes) <-- TRAP!\n",
      static_cast<unsigned long long>(g_tl_new_calls),
      static_cast<unsigned long long>(g_tl_new_bytes));

  // 3. Fixed allocator-aware std::pmr::vector<PmrVenueFill> on 4 KiB stack buf
  g_tl_new_calls = 0;
  g_tl_new_bytes = 0;
  {
    std::array<std::byte, 4096> stack_buf{};
    std::pmr::monotonic_buffer_resource arena(stack_buf.data(),
                                              stack_buf.size());
    std::pmr::vector<PmrVenueFill> fills(&arena);
    fills.reserve(kPacketFills);
    for (std::size_t i = 0; i < kPacketFills; ++i) {
      fills.emplace_back(i + 1, 18525, 100, 1, kExecId);
    }
    DoNotOptimize(fills.data());
  }
  std::printf(
      "  3. pmr::vector<PmrVenueFill> (AA)    : %2llu global ::operator new "
      "calls (%llu bytes) <-- ZERO HEAP!\n",
      static_cast<unsigned long long>(g_tl_new_calls),
      static_cast<unsigned long long>(g_tl_new_bytes));
}

}  // namespace
}  // namespace hotpath::memory_allocators

int main() {
  hotpath::memory_allocators::PinThreadToCore(0);
  hotpath::memory_allocators::RunHeapAgingDemo();
  hotpath::memory_allocators::RunPmrStringTrapDemo();
  return 0;
}
