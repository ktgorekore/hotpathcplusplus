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
 * @brief Hot Path C++ — Episode 08 Community Concurrency Challenge.
 *
 * MISSION:
 * 1. Observe real hardware Store-Load reordering on bare-metal x86-64 silicon
 *    using Dekker's 4-line litmus test (`x = 1; r1 = y;` / `y = 1; r2 = x;`)
 *    with `std::memory_order_relaxed` (plain `mov` instructions, zero UB), and
 *    eliminate it using `std::memory_order_seq_cst` (`xchg` Store Buffer
 * drain).
 * 2. Optimize a cross-core Producer-Consumer message handoff by replacing heavy
 *    `std::memory_order_seq_cst` Store Buffer flushes with zero-fence
 *    `std::memory_order_release` and `std::memory_order_acquire`
 * synchronization.
 */

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

namespace hotpath::episode08 {

constexpr size_t kLitmusIterations = 200'000;
constexpr size_t kMessageCount = 2'000'000;

/**
 * @struct LitmusResult
 * @brief Captures the number of times `r1 == 0 && r2 == 0` occurred in
 * hardware.
 */
struct LitmusResult {
  size_t total_trials = 0;
  size_t both_zero_reordered = 0;
  double duration_ms = 0.0;
};

/**
 * @brief Runs Dekker's 4-line Store-Load litmus test across two threads.
 *
 * When `UseSeqCst` is false (`memory_order_relaxed`), x86-64 emits plain `mov`
 * instructions. Both cores buffer their writes (`x = 1`, `y = 1`) in their
 * private Store Buffers while their subsequent loads (`r1 = y`, `r2 = x`) hit
 * L1 cache immediately, producing `r1 == 0 && r2 == 0` in hardware!
 *
 * When `UseSeqCst` is true (`memory_order_seq_cst`), x86-64 emits `xchg`,
 * draining the Store Buffer before the subsequent load can execute.
 *
 * @tparam UseSeqCst True to enforce `memory_order_seq_cst`, false for
 * `relaxed`.
 * @return LitmusResult containing observed Store-Load reorderings.
 */
template <bool UseSeqCst>
LitmusResult RunStoreLoadLitmusTest() {
  alignas(64) std::atomic<int> x{0};
  alignas(64) std::atomic<int> y{0};
  alignas(64) int r1 = 0;
  alignas(64) int r2 = 0;

  std::barrier sync_point(2);
  size_t reordered_count = 0;

  auto start = std::chrono::high_resolution_clock::now();

  std::thread t0([&]() {
    for (size_t i = 0; i < kLitmusIterations; ++i) {
      sync_point.arrive_and_wait();
      if constexpr (UseSeqCst) {
        x.store(1, std::memory_order_seq_cst);
        r1 = y.load(std::memory_order_seq_cst);
      } else {
        x.store(1, std::memory_order_relaxed);
        r1 = y.load(std::memory_order_relaxed);
      }
      sync_point.arrive_and_wait();
    }
  });

  std::thread t1([&]() {
    for (size_t i = 0; i < kLitmusIterations; ++i) {
      x.store(0, std::memory_order_relaxed);
      y.store(0, std::memory_order_relaxed);
      sync_point.arrive_and_wait();
      if constexpr (UseSeqCst) {
        y.store(1, std::memory_order_seq_cst);
        r2 = x.load(std::memory_order_seq_cst);
      } else {
        y.store(1, std::memory_order_relaxed);
        r2 = x.load(std::memory_order_relaxed);
      }
      sync_point.arrive_and_wait();
      if (r1 == 0 && r2 == 0) {
        ++reordered_count;
      }
    }
  });

  t0.join();
  t1.join();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end - start;

  return {kLitmusIterations, reordered_count, elapsed.count()};
}

/**
 * @struct HandoffMetrics
 * @brief Encapsulates verification checksum and timing for Producer-Consumer.
 */
struct HandoffMetrics {
  uint64_t checksum = 0;
  double duration_ms = 0.0;
  double messages_per_sec = 0.0;
};

/**
 * @brief Multi-threaded Producer-Consumer SPSC ring handoff benchmark.
 *
 * Compares `std::memory_order_seq_cst` (flushing the Store Buffer on every
 * index update) against `std::memory_order_release` /
 * `std::memory_order_acquire` (zero hardware fence overhead on x86-64 TSO).
 *
 * @tparam UseAcquireRelease True to use Acquire/Release, false for SeqCst.
 * @return HandoffMetrics containing checksum and throughput.
 */
template <bool UseAcquireRelease>
HandoffMetrics RunProducerConsumerHandoff() {
  constexpr size_t kRingSize = 1024;
  struct Slot {
    uint64_t payload_a = 0;
    uint64_t payload_b = 0;
  };

  alignas(64) std::atomic<uint64_t> write_idx{0};
  alignas(64) std::atomic<uint64_t> read_idx{0};
  alignas(64) std::array<Slot, kRingSize> buffer{};

  uint64_t consumer_checksum = 0;
  std::barrier start_barrier(2);

  auto start_time = std::chrono::high_resolution_clock::now();

  std::thread producer([&]() {
    start_barrier.arrive_and_wait();
    for (uint64_t i = 1; i <= kMessageCount; ++i) {
      if constexpr (UseAcquireRelease) {
        while (i - 1 - read_idx.load(std::memory_order_acquire) >= kRingSize) {
        }
        Slot& slot = buffer[(i - 1) & (kRingSize - 1)];
        slot.payload_a = i * 3;
        slot.payload_b = i ^ 0x5A5A5A5AULL;
        write_idx.store(i, std::memory_order_release);
      } else {
        while (i - 1 - read_idx.load(std::memory_order_seq_cst) >= kRingSize) {
        }
        Slot& slot = buffer[(i - 1) & (kRingSize - 1)];
        slot.payload_a = i * 3;
        slot.payload_b = i ^ 0x5A5A5A5AULL;
        write_idx.store(i, std::memory_order_seq_cst);
      }
    }
  });

  std::thread consumer([&]() {
    start_barrier.arrive_and_wait();
    uint64_t sum = 0;
    for (uint64_t i = 1; i <= kMessageCount; ++i) {
      if constexpr (UseAcquireRelease) {
        while (write_idx.load(std::memory_order_acquire) < i) {
        }
        const Slot& slot = buffer[(i - 1) & (kRingSize - 1)];
        sum += (slot.payload_a ^ slot.payload_b);
        read_idx.store(i, std::memory_order_release);
      } else {
        while (write_idx.load(std::memory_order_seq_cst) < i) {
        }
        const Slot& slot = buffer[(i - 1) & (kRingSize - 1)];
        sum += (slot.payload_a ^ slot.payload_b);
        read_idx.store(i, std::memory_order_seq_cst);
      }
    }
    consumer_checksum = sum;
  });

  producer.join();
  consumer.join();

  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

  return {consumer_checksum, elapsed.count(),
          static_cast<double>(kMessageCount) / (elapsed.count() / 1000.0)};
}

}  // namespace hotpath::episode08

/**
 * @brief Main entrypoint for the Episode 08 Community Concurrency Challenge.
 * @return 0 if all ordering invariants and checksums pass, 1 otherwise.
 */
int main() {
  std::cout << "=========================================================\n"
            << "🔥 Hot Path C++ — Episode 08 Community Challenge 🔥\n"
            << "   Store Buffers, Memory Reordering & C++20 Atomics\n"
            << "=========================================================\n\n";

  std::cout << "[Part 1] Dekker's 4-Line Store-Load Litmus Test ("
            << hotpath::episode08::kLitmusIterations << " trials)...\n";
  auto relaxed_litmus =
      hotpath::episode08::RunStoreLoadLitmusTest</*UseSeqCst=*/false>();
  auto seq_cst_litmus =
      hotpath::episode08::RunStoreLoadLitmusTest</*UseSeqCst=*/true>();

  std::cout << "  • memory_order_relaxed (Plain MOV -> Store Buffer):\n"
            << "      Observed r1 == 0 && r2 == 0 : "
            << relaxed_litmus.both_zero_reordered << " / "
            << relaxed_litmus.total_trials
            << " trials (Store-Load Reordered in Silicon!)\n";
  std::cout << "  • memory_order_seq_cst (XCHG -> Drains Store Buffer):\n"
            << "      Observed r1 == 0 && r2 == 0 : "
            << seq_cst_litmus.both_zero_reordered << " / "
            << seq_cst_litmus.total_trials
            << " trials (100% Sequentially Consistent ✅)\n\n";

  std::cout << "[Part 2] Cross-Core Producer-Consumer Handoff ("
            << hotpath::episode08::kMessageCount << " messages)...\n";
  auto seq_cst_handoff = hotpath::episode08::RunProducerConsumerHandoff<
      /*UseAcquireRelease=*/false>();
  auto acq_rel_handoff = hotpath::episode08::RunProducerConsumerHandoff<
      /*UseAcquireRelease=*/true>();

  const double speedup =
      seq_cst_handoff.duration_ms / acq_rel_handoff.duration_ms;
  const bool valid = (seq_cst_handoff.checksum == acq_rel_handoff.checksum) &&
                     (seq_cst_litmus.both_zero_reordered == 0);

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "---------------------------------------------------------\n";
  std::cout << "Baseline (memory_order_seq_cst) : "
            << seq_cst_handoff.duration_ms << " ms ("
            << (seq_cst_handoff.messages_per_sec / 1e6) << " M msgs/sec)\n";
  std::cout << "Optimized (acquire / release)   : "
            << acq_rel_handoff.duration_ms << " ms ("
            << (acq_rel_handoff.messages_per_sec / 1e6) << " M msgs/sec)\n";
  std::cout << "Throughput Speedup              : " << speedup << "x\n";
  std::cout << "Payload Integrity Checksum      : 0x" << std::hex
            << acq_rel_handoff.checksum << std::dec << " ("
            << (valid ? "PASSED ✅" : "FAILED ❌") << ")\n";
  std::cout << "---------------------------------------------------------\n";

  return valid ? 0 : 1;
}
