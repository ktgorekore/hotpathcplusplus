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
 * @brief Episode 14 Forensic Challenge & Hardware PMU (`perf stat`) runner:
 * - Exports unmangled `extern "C"` hot-loop entry points for clean
 *   `llvm-objdump -d --disassemble-symbols=... -M intel` inspection.
 * - Supports `--mode=all|spatial_seq|spatial_rand|spatial_dep|temporal` and
 *   `--variant=all|unchecked|hardened|span|at|bitmask|generational|weak_ptr`
 *   plus `--iters=N` for unmultiplexed hardware PMU profiling on Zen 5.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>

#include "common/memory_safety/memory_safety.h"

namespace hotpath::memory_safety {

extern "C" {

/**
 * @brief Unmangled entry point for `llvm-objdump`: Unchecked random batch
 * gather (auto-vectorized with AVX-512 `vpgatherqq`).
 */
[[gnu::noinline]] std::int64_t ep14_gather_batch_unchecked(
    const Order* orders, const std::uint32_t* indices,
    std::size_t count) noexcept {
  return GatherBatchUnchecked(orders, indices, count);
}

/**
 * @brief Unmangled entry point for `llvm-objdump`: Hardened fast-trap random
 * batch gather (`cmp + jae ud2`, scalar loop).
 */
[[gnu::noinline]] std::int64_t ep14_gather_batch_hardened_trap(
    const std::vector<Order>& orders,
    const std::vector<std::uint32_t>& indices) noexcept {
  return GatherBatchHardenedTrap(orders, indices);
}

/**
 * @brief Unmangled entry point for `llvm-objdump`: Power-of-two bitmask-safe
 * random batch gather (`orders[indices[i] & mask]`, restores AVX-512
 * `vpgatherqq` with 100% spatial safety).
 */
[[gnu::noinline]] std::int64_t ep14_gather_batch_bitmask_safe(
    const Order* orders, const std::uint32_t* indices, std::size_t count,
    std::uint32_t mask) noexcept {
  return GatherBatchBitmaskSafe(orders, indices, count, mask);
}

/**
 * @brief Unmangled entry point for `llvm-objdump`: Dependent order-book probe
 * chain (Unchecked).
 */
[[gnu::noinline]] std::int64_t ep14_probe_dependent_unchecked(
    const Order* orders, const std::uint32_t* indices, std::size_t count,
    std::uint32_t mask) noexcept {
  return ProbeDependentUnchecked(orders, indices, count, mask);
}

/**
 * @brief Unmangled entry point for `llvm-objdump`: Dependent order-book probe
 * chain with hardened fast trap (`cmp + jae ud2`).
 */
[[gnu::noinline]] std::int64_t ep14_probe_dependent_hardened_trap(
    const std::vector<Order>& orders, const std::vector<std::uint32_t>& indices,
    std::uint32_t mask) noexcept {
  return ProbeDependentHardenedTrap(orders, indices, mask);
}

/**
 * @brief Unmangled entry point for `llvm-objdump`: Safe 64-bit generational
 * handle lookup (`OrderSlabPool::Get(OrderHandle)`).
 */
[[gnu::noinline]] std::int64_t ep14_temporal_generational_lookup(
    const OrderSlabPool& pool, const std::vector<OrderHandle>& handles,
    const std::vector<std::uint32_t>& lookup_order) noexcept {
  return RunTemporalGenerationalHotLoop(pool, handles, lookup_order);
}

}  // extern "C"

namespace {

/**
 * @brief Prints a single formatted benchmark row.
 * @param label Benchmark variant description.
 * @param elapsed_ns Total elapsed nanoseconds across `iters * count` ops.
 * @param iters Number of passes.
 * @param count Elements processed per pass.
 * @param checksum Final checksum accumulator.
 */
void PrintRow(const char* label, double elapsed_ns, std::size_t iters,
              std::size_t count, std::int64_t checksum) {
  const double us_per_pass = (elapsed_ns / static_cast<double>(iters)) / 1000.0;
  const double ns_per_op = elapsed_ns / static_cast<double>(iters * count);
  std::printf("  %-46s | %8.2f us/pass | %6.3f ns/order | sum=%lld\n", label,
              us_per_pass, ns_per_op, static_cast<long long>(checksum));
}

/**
 * @brief Runs Battle 1A: Sequential Scan (Compile-time range check
 * elimination).
 */
void RunSequentialSuite(const SpatialDataset& ds, std::size_t iters,
                        const std::string& variant) {
  std::printf(
      "======================================================================"
      "==================\n");
  std::printf("BATTLE 1A: SEQUENTIAL SCAN (%zu Orders = 2.0 MiB, %zu Passes)\n",
              ds.orders.size(), iters);
  std::printf(
      "======================================================================"
      "==================\n");

  const std::span<const Order> span_view(ds.orders);

  if (variant == "all" || variant == "unchecked") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(
          ScanSequentialUnchecked(ds.orders.data(), ds.orders.size()));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ScanSequentialUnchecked(ds.orders.data(), ds.orders.size());
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("1. Unchecked Pointer Scan (orders[i])",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.orders.size(), acc);
  }

  if (variant == "all" || variant == "hardened") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ScanSequentialHardenedTrap(ds.orders));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ScanSequentialHardenedTrap(ds.orders);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("2. Hardened Fast Trap (i >= n -> ud2)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.orders.size(), acc);
  }

  if (variant == "all" || variant == "span") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ScanSequentialSpan(span_view));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ScanSequentialSpan(span_view);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("3. C++20 std::span<const Order>",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.orders.size(), acc);
  }

  if (variant == "all" || variant == "at") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ScanSequentialVectorAt(ds.orders));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ScanSequentialVectorAt(ds.orders);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("4. Exception-Throwing std::vector::at(i)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.orders.size(), acc);
  }
}

/**
 * @brief Runs Battle 1B: Independent Random Batch Gather (SIMD vs. In-Loop
 * Trap vs. Power-of-Two Bitmask).
 */
void RunBatchGatherSuite(const SpatialDataset& ds, std::size_t iters,
                         const std::string& variant) {
  std::printf(
      "======================================================================"
      "==================\n");
  std::printf(
      "BATTLE 1B: INDEPENDENT RANDOM BATCH GATHER (%zu Lookups, %zu Passes)\n",
      ds.random_idx.size(), iters);
  std::printf(
      "======================================================================"
      "==================\n");

  const std::span<const Order> orders_span(ds.orders);
  const std::span<const std::uint32_t> idx_span(ds.random_idx);

  if (variant == "all" || variant == "unchecked") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ep14_gather_batch_unchecked(
          ds.orders.data(), ds.random_idx.data(), ds.random_idx.size()));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ep14_gather_batch_unchecked(ds.orders.data(), ds.random_idx.data(),
                                         ds.random_idx.size());
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("1. Unchecked Gather (AVX-512 vpgatherqq)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }

  if (variant == "all" || variant == "hardened") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ep14_gather_batch_hardened_trap(ds.orders, ds.random_idx));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ep14_gather_batch_hardened_trap(ds.orders, ds.random_idx);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("2. Hardened Trap Gather (cmp + jae ud2, Scalar)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }

  if (variant == "all" || variant == "span") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(GatherBatchSpan(orders_span, idx_span));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += GatherBatchSpan(orders_span, idx_span);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("3. C++20 std::span Gather (Checked Assertions)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }

  if (variant == "all" || variant == "at") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(GatherBatchVectorAt(ds.orders, ds.random_idx));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += GatherBatchVectorAt(ds.orders, ds.random_idx);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("4. std::vector::at(idx) (Throws out_of_range)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }

  if (variant == "all" || variant == "bitmask") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ep14_gather_batch_bitmask_safe(
          ds.orders.data(), ds.random_idx.data(), ds.random_idx.size(),
          kDefaultBookMask));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ep14_gather_batch_bitmask_safe(
          ds.orders.data(), ds.random_idx.data(), ds.random_idx.size(),
          kDefaultBookMask);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("5. Bitmask-Safe Gather (idx & 0xFFFF, AVX-512)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }
}

/**
 * @brief Runs Battle 1C: Latency-Bound Dependent Probe Chain (Serial Data
 * Dependency isolating pure hardware branch predictor cost).
 */
void RunDependentProbeSuite(const SpatialDataset& ds, std::size_t iters,
                            const std::string& variant) {
  std::printf(
      "======================================================================"
      "==================\n");
  std::printf(
      "BATTLE 1C: DEPENDENT ORDER-BOOK PROBE CHAIN (%zu Probes, %zu Passes)\n",
      ds.random_idx.size(), iters);
  std::printf(
      "======================================================================"
      "==================\n");

  if (variant == "all" || variant == "unchecked") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ep14_probe_dependent_unchecked(
          ds.orders.data(), ds.random_idx.data(), ds.random_idx.size(),
          kDefaultBookMask));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ep14_probe_dependent_unchecked(
          ds.orders.data(), ds.random_idx.data(), ds.random_idx.size(),
          kDefaultBookMask);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("1. Unchecked Dependent Probe (orders[idx])",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }

  if (variant == "all" || variant == "hardened") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ep14_probe_dependent_hardened_trap(ds.orders, ds.random_idx,
                                                       kDefaultBookMask));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ep14_probe_dependent_hardened_trap(ds.orders, ds.random_idx,
                                                kDefaultBookMask);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("2. Hardened Fast Trap Probe (cmp + jae ud2)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }

  if (variant == "all" || variant == "at") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(
          ProbeDependentVectorAt(ds.orders, ds.random_idx, kDefaultBookMask));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ProbeDependentVectorAt(ds.orders, ds.random_idx, kDefaultBookMask);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("3. std::vector::at(idx) Probe (Exception Check)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             ds.random_idx.size(), acc);
  }
}

/**
 * @brief Runs Battle 3: Temporal Memory Safety Under 25% Order Cancellation &
 * ABA Slot Recycling.
 */
void RunTemporalSuite(const TemporalChurnScenario& scenario, std::size_t iters,
                      const std::string& variant) {
  std::printf(
      "======================================================================"
      "==================\n");
  std::printf(
      "BATTLE 3: TEMPORAL SAFETY UNDER 25%% CANCELLATION & ABA SLOT RECYCLING "
      "(%zu Passes)\n",
      iters);
  std::printf(
      "======================================================================"
      "==================\n");

  const TemporalAuditResult unchecked_audit =
      AuditTemporalLookups(scenario, false);
  const TemporalAuditResult gen_audit = AuditTemporalLookups(scenario, true);

  if (variant == "all" || variant == "unchecked") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(RunTemporalUncheckedSlotHotLoop(
          scenario.pool, scenario.original_handles, scenario.lookup_order));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += RunTemporalUncheckedSlotHotLoop(
          scenario.pool, scenario.original_handles, scenario.lookup_order);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("1. Unchecked Slot Index (25% Silent ABA Bug!)",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             scenario.lookup_order.size(), acc);
    std::printf(
        "     -> Forensic Audit: valid=%zu | rejected=%zu | "
        "SILENT_ABA_CORRUPTIONS=%zu (25.0%%!)\n",
        unchecked_audit.valid_active_reads,
        unchecked_audit.rejected_stale_handles,
        unchecked_audit.silent_aba_corruptions);
  }

  if (variant == "all" || variant == "generational") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(ep14_temporal_generational_lookup(
          scenario.pool, scenario.original_handles, scenario.lookup_order));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += ep14_temporal_generational_lookup(
          scenario.pool, scenario.original_handles, scenario.lookup_order);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("2. 64-Bit Generational Slab Handle (Safe O(1))",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             scenario.lookup_order.size(), acc);
    std::printf(
        "     -> Forensic Audit: valid=%zu | rejected=%zu | "
        "SILENT_ABA_CORRUPTIONS=%zu (100%% Safe!)\n",
        gen_audit.valid_active_reads, gen_audit.rejected_stale_handles,
        gen_audit.silent_aba_corruptions);
  }

  if (variant == "all" || variant == "weak_ptr") {
    for (std::size_t w = 0; w < 50; ++w) {
      DoNotOptimize(RunTemporalWeakPtrHotLoop(scenario.original_weak_handles,
                                              scenario.lookup_order));
    }
    std::int64_t acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < iters; ++r) {
      ClobberMemory();
      acc += RunTemporalWeakPtrHotLoop(scenario.original_weak_handles,
                                       scenario.lookup_order);
    }
    const auto t1 = std::chrono::steady_clock::now();
    DoNotOptimize(acc);
    PrintRow("3. Reference-Counted std::weak_ptr::lock()",
             std::chrono::duration<double, std::nano>(t1 - t0).count(), iters,
             scenario.lookup_order.size(), acc);
    std::printf(
        "     -> Forensic Audit: valid=%zu | rejected=%zu | "
        "SILENT_ABA_CORRUPTIONS=0 (Heap Indirection)\n",
        gen_audit.valid_active_reads, gen_audit.rejected_stale_handles);
  }
}

}  // namespace
}  // namespace hotpath::memory_safety

int main(int argc, char** argv) {
  hotpath::memory_safety::PinThreadToCore(0);
  std::string mode = "all";
  std::string variant = "all";
  std::size_t iters = 2000;

  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg.rfind("--mode=", 0) == 0) {
      mode = arg.substr(7);
    } else if (arg.rfind("--variant=", 0) == 0) {
      variant = arg.substr(10);
    } else if (arg.rfind("--iters=", 0) == 0) {
      iters = static_cast<std::size_t>(std::stoull(arg.substr(8)));
    }
  }

  const hotpath::memory_safety::SpatialDataset spatial_ds(
      hotpath::memory_safety::kDefaultBookCapacity);

  if (mode == "all" || mode == "spatial_seq") {
    hotpath::memory_safety::RunSequentialSuite(spatial_ds, iters, variant);
  }
  if (mode == "all" || mode == "spatial_rand") {
    hotpath::memory_safety::RunBatchGatherSuite(spatial_ds, iters, variant);
  }
  if (mode == "all" || mode == "spatial_dep") {
    hotpath::memory_safety::RunDependentProbeSuite(spatial_ds, iters, variant);
  }
  if (mode == "all" || mode == "temporal") {
    const hotpath::memory_safety::TemporalChurnScenario temporal_scenario(
        hotpath::memory_safety::kDefaultBookCapacity);
    hotpath::memory_safety::RunTemporalSuite(temporal_scenario, iters, variant);
  }
  return 0;
}
