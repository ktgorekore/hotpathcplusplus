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
 * @brief Hot Path C++ — Episode 08 Benchmark: Store Buffers, RMW & C++ Atomics.
 *
 * Demonstrates realistic silicon behavior without contrived compiler barriers:
 * 1. Store-Buffer Store-Load sequence:
 *    - Acquire/Release on x86-64 (TSO): compiles to plain `mov` instructions,
 *      allowing the CPU Store Buffer to absorb the store in ~1 cycle while
 *      the subsequent load executes immediately via Store-Load bypassing.
 *    - Sequential Consistency (`seq_cst`): emits `xchg` (implicit `LOCK`
 *      prefix), forcing the Store Buffer to drain completely to L1D cache
 *      before any subsequent load can proceed.
 * 2. Single-Producer Single-Consumer (SPSC) ring buffer handoff using
 *    `memory_order_acquire` / `memory_order_release` vs `memory_order_seq_cst`.
 * 3. Read-Modify-Write (`fetch_add`) uncontended vs multi-core cache-line RFO
 *    contention vs local register accumulation with release publication.
 */

#include "benchmark/benchmark.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace hotpath::episode08 {
namespace {

constexpr size_t kBatchSize = 4096;

/**
 * @struct AlignedAtomicState
 * @brief Two cache-line-isolated atomic variables modeling Dekker /
 * work-stealing state flags on separate 64-byte hardware cache lines.
 */
struct AlignedAtomicState {
  alignas(64) std::atomic<uint64_t> local_outbox{0};
  alignas(64) std::atomic<uint64_t> peer_inbox{42};
};

/**
 * @struct MarketTick
 * @brief Realistic market data payload handed off between producer and
 * consumer.
 */
struct MarketTick {
  uint64_t sequence_id = 0;
  double price = 0.0;
  uint64_t quantity = 0;
};

/**
 * @class SpscRingBuffer
 * @brief Realistic Single-Producer Single-Consumer ring buffer parameterized
 * by memory ordering strategy (Acquire/Release vs Sequentially Consistent).
 */
template <size_t Capacity>
class SpscRingBuffer {
 public:
  bool TryPublishAcqRel(const MarketTick& tick) noexcept {
    const uint64_t w = write_idx_.load(std::memory_order_relaxed);
    const uint64_t r = read_idx_.load(std::memory_order_acquire);
    if (w - r >= Capacity) {
      return false;
    }
    slots_[w & (Capacity - 1)] = tick;
    write_idx_.store(w + 1, std::memory_order_release);
    return true;
  }

  bool TryConsumeAcqRel(MarketTick& out) noexcept {
    const uint64_t r = read_idx_.load(std::memory_order_relaxed);
    const uint64_t w = write_idx_.load(std::memory_order_acquire);
    if (r == w) {
      return false;
    }
    out = slots_[r & (Capacity - 1)];
    read_idx_.store(r + 1, std::memory_order_release);
    return true;
  }

  bool TryPublishSeqCst(const MarketTick& tick) noexcept {
    const uint64_t w = write_idx_.load(std::memory_order_seq_cst);
    const uint64_t r = read_idx_.load(std::memory_order_seq_cst);
    if (w - r >= Capacity) {
      return false;
    }
    slots_[w & (Capacity - 1)] = tick;
    write_idx_.store(w + 1, std::memory_order_seq_cst);
    return true;
  }

  bool TryConsumeSeqCst(MarketTick& out) noexcept {
    const uint64_t r = read_idx_.load(std::memory_order_seq_cst);
    const uint64_t w = write_idx_.load(std::memory_order_seq_cst);
    if (r == w) {
      return false;
    }
    out = slots_[r & (Capacity - 1)];
    read_idx_.store(r + 1, std::memory_order_seq_cst);
    return true;
  }

 private:
  alignas(64) std::atomic<uint64_t> write_idx_{0};
  alignas(64) std::atomic<uint64_t> read_idx_{0};
  alignas(64) std::array<MarketTick, Capacity> slots_{};
};

/**
 * @brief Store followed by Load using `memory_order_relaxed`.
 *
 * On x86-64, compiles to plain `mov` instructions. The CPU drops the store into
 * its private Store Buffer (~1 cycle) and immediately executes the load from
 * L1D cache via Store-Load bypassing.
 *
 * @param state Google Benchmark runner state.
 */
void BM_StoreLoad_Relaxed(benchmark::State& state) {
  AlignedAtomicState sync_state;

  for (auto _ : state) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < kBatchSize; ++i) {
      sync_state.local_outbox.store(i, std::memory_order_relaxed);
      checksum += sync_state.peer_inbox.load(std::memory_order_relaxed);
    }
    benchmark::DoNotOptimize(checksum);
  }

  state.SetItemsProcessed(state.iterations() * kBatchSize);
}
BENCHMARK(BM_StoreLoad_Relaxed);

/**
 * @brief Store (`release`) followed by Load (`acquire`).
 *
 * On x86-64 Total Store Order (TSO), hardware natively preserves Store-Store
 * and Load-Load order, so `release` and `acquire` compile down to plain `mov`
 * instructions with zero hardware fence overhead!
 *
 * @param state Google Benchmark runner state.
 */
void BM_StoreLoad_AcquireRelease(benchmark::State& state) {
  AlignedAtomicState sync_state;

  for (auto _ : state) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < kBatchSize; ++i) {
      sync_state.local_outbox.store(i, std::memory_order_release);
      checksum += sync_state.peer_inbox.load(std::memory_order_acquire);
    }
    benchmark::DoNotOptimize(checksum);
  }

  state.SetItemsProcessed(state.iterations() * kBatchSize);
}
BENCHMARK(BM_StoreLoad_AcquireRelease);

/**
 * @brief Store (`seq_cst`) followed by Load (`seq_cst`).
 *
 * To prevent Store-Load reordering, x86-64 emits `xchg` (carrying an implicit
 * hardware `LOCK` prefix), forcing the CPU core to pause subsequent loads until
 * the entire Store Buffer drains to the L1 Data Cache.
 *
 * @param state Google Benchmark runner state.
 */
void BM_StoreLoad_SeqCst(benchmark::State& state) {
  AlignedAtomicState sync_state;

  for (auto _ : state) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < kBatchSize; ++i) {
      sync_state.local_outbox.store(i, std::memory_order_seq_cst);
      checksum += sync_state.peer_inbox.load(std::memory_order_seq_cst);
    }
    benchmark::DoNotOptimize(checksum);
  }

  state.SetItemsProcessed(state.iterations() * kBatchSize);
}
BENCHMARK(BM_StoreLoad_SeqCst);

/**
 * @brief Realistic SPSC Queue Publish + Consume using Acquire-Release
 * semantics.
 * @param state Google Benchmark runner state.
 */
void BM_SpscQueue_AcquireRelease(benchmark::State& state) {
  SpscRingBuffer<1024> ring;

  for (auto _ : state) {
    uint64_t total_qty = 0;
    for (size_t i = 0; i < kBatchSize; ++i) {
      MarketTick tick{i, 150.25, 100};
      ring.TryPublishAcqRel(tick);
      MarketTick out;
      if (ring.TryConsumeAcqRel(out)) {
        total_qty += out.quantity;
      }
    }
    benchmark::DoNotOptimize(total_qty);
  }

  state.SetItemsProcessed(state.iterations() * kBatchSize);
}
BENCHMARK(BM_SpscQueue_AcquireRelease);

/**
 * @brief Realistic SPSC Queue Publish + Consume using `memory_order_seq_cst`.
 * @param state Google Benchmark runner state.
 */
void BM_SpscQueue_SeqCst(benchmark::State& state) {
  SpscRingBuffer<1024> ring;

  for (auto _ : state) {
    uint64_t total_qty = 0;
    for (size_t i = 0; i < kBatchSize; ++i) {
      MarketTick tick{i, 150.25, 100};
      ring.TryPublishSeqCst(tick);
      MarketTick out;
      if (ring.TryConsumeSeqCst(out)) {
        total_qty += out.quantity;
      }
    }
    benchmark::DoNotOptimize(total_qty);
  }

  state.SetItemsProcessed(state.iterations() * kBatchSize);
}
BENCHMARK(BM_SpscQueue_SeqCst);

alignas(64) std::atomic<uint64_t> g_shared_counter{0};

/**
 * @brief Atomic Read-Modify-Write (`fetch_add` / `lock add`) across threads.
 *
 * Demonstrates the hardware cost of locked RMW instructions draining the Store
 * Buffer and bouncing cache lines via MESI Request For Ownership (RFO) when
 * contended across multiple CPU cores.
 *
 * @param state Google Benchmark runner state.
 */
void BM_AtomicRMW_FetchAdd(benchmark::State& state) {
  if (state.thread_index() == 0) {
    g_shared_counter.store(0, std::memory_order_relaxed);
  }

  for (auto _ : state) {
    for (size_t i = 0; i < kBatchSize; ++i) {
      g_shared_counter.fetch_add(1, std::memory_order_relaxed);
    }
  }

  state.SetItemsProcessed(state.iterations() * kBatchSize);
}
BENCHMARK(BM_AtomicRMW_FetchAdd)->Threads(1)->Threads(2)->Threads(4);

}  // namespace
}  // namespace hotpath::episode08
