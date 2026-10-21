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
 * @file mesi_coherency.h
 * @brief Cache-coherency (MESI/MOESI) primitives demonstrating Shared (S) vs
 * Modified (M) state transitions, Naive CAS vs Read-Before-CAS (TTAS)
 * spinlocks, and 64-byte cache-line alignment vs `#pragma pack(1)` Split Locks.
 */

#ifndef HOTPATH_COMMON_MESI_COHERENCY_MESI_COHERENCY_H_
#define HOTPATH_COMMON_MESI_COHERENCY_MESI_COHERENCY_H_

#include <immintrin.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace hotpath::mesi_coherency {

/**
 * @brief Hardware L1D cache-line size in bytes on x86-64 processors.
 */
inline constexpr std::size_t kCacheLineBytes = 64;

/**
 * @struct CacheLineAlignedAtomic
 * @brief Isolates a `std::atomic<std::uint64_t>` on its own dedicated 64-byte
 * cache line to eliminate false sharing with adjacent variables.
 */
struct alignas(kCacheLineBytes) CacheLineAlignedAtomic {
  std::atomic<std::uint64_t> value{0};
  std::array<std::uint8_t, kCacheLineBytes - sizeof(std::atomic<std::uint64_t>)>
      padding{};
};

static_assert(sizeof(CacheLineAlignedAtomic) == kCacheLineBytes,
              "CacheLineAlignedAtomic must occupy exactly one 64B cache line.");

#pragma pack(push, 1)
/**
 * @struct PackedSplitLockHeader
 * @brief Pathological `#pragma pack(1)` binary wire header where the 8-byte
 * atomic sequence counter sits at byte offset 60, straddling two 64-byte L1D
 * cache lines (bytes 60..63 in Cache Line A and bytes 64..67 in Cache Line B)
 * and triggering a hardware Global Bus Lock on every atomic RMW.
 */
struct PackedSplitLockHeader {
  std::array<std::uint8_t, 60> preamble{};
  std::atomic<std::uint64_t> seq_no{0};
  std::array<std::uint8_t, 60> trailing{};
};
#pragma pack(pop)

/**
 * @struct AlignedTelemetryHeader
 * @brief Cache-line aligned binary header where the 8-byte atomic sequence
 * counter sits at byte offset 0 inside a single 64-byte L1D cache line,
 * allowing hardware atomic operations to use a fast L1D Cache Lock.
 */
struct alignas(kCacheLineBytes) AlignedTelemetryHeader {
  std::atomic<std::uint64_t> seq_no{0};
  std::array<std::uint8_t, 56> preamble{};
};

static_assert(offsetof(PackedSplitLockHeader, seq_no) == 60,
              "PackedSplitLockHeader::seq_no must sit at byte offset 60.");
static_assert(offsetof(AlignedTelemetryHeader, seq_no) == 0,
              "AlignedTelemetryHeader::seq_no must sit at byte offset 0.");

/**
 * @struct SharedOrderBookLedger
 * @brief Shared order-book risk ledger updated inside contended critical
 * sections.
 */
struct alignas(kCacheLineBytes) SharedOrderBookLedger {
  std::uint64_t sequence = 0;
  std::uint64_t total_Fill_qty = 0;
  std::uint64_t rolling_notional = 0;
  std::array<std::uint64_t, 5> venue_buckets{};

  /**
   * @brief Records a deterministic order execution update across all ledger
   * fields.
   * @param qty Executed quantity.
   * @param price_ticks Execution price in ticks.
   */
  void RecordExecution(std::uint64_t qty, std::uint64_t price_ticks) noexcept {
    const std::uint64_t seq = ++sequence;
    total_Fill_qty += qty;
    rolling_notional += qty * price_ticks;
    for (std::size_t idx = 0; idx < venue_buckets.size(); ++idx) {
      venue_buckets[idx] =
          (venue_buckets[idx] ^ (seq + idx)) * 6364136223846793005ULL +
          1442695040888963407ULL;
    }
  }
};

/**
 * @class NaiveCasSpinlock
 * @brief Naive Compare-And-Swap spinlock where every failed
 * `compare_exchange_weak` iteration issues `lock cmpxchg`, demanding the
 * Exclusive/Modified (`M`) MESI state and flooding the interconnect with
 * Request For Ownership (RFO) broadcasts.
 */
class alignas(kCacheLineBytes) NaiveCasSpinlock {
 public:
  NaiveCasSpinlock() = default;

  /**
   * @brief Acquires the spinlock via an unshielded `compare_exchange_weak`
   * loop.
   */
  void Lock() noexcept {
    std::uint64_t expected = 0;
    while (!flag_.compare_exchange_weak(expected, 1, std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
      expected = 0;
    }
  }

  /**
   * @brief Releases the spinlock with `std::memory_order_release`.
   */
  void Unlock() noexcept { flag_.store(0, std::memory_order_release); }

 private:
  std::atomic<std::uint64_t> flag_{0};
};

/**
 * @class ReadBeforeCasSpinlock
 * @brief Test-and-Test-and-Set (TTAS / Read-Before-CAS) spinlock that spins on
 * `load(std::memory_order_relaxed)` with `_mm_pause()` in the MESI Shared (`S`)
 * state until the lock appears free, eliminating RFO broadcast storms while
 * waiting.
 */
class alignas(kCacheLineBytes) ReadBeforeCasSpinlock {
 public:
  ReadBeforeCasSpinlock() = default;

  /**
   * @brief Acquires the spinlock, spinning in MESI Shared (`S`) state on
   * contention.
   */
  void Lock() noexcept {
    std::uint64_t expected = 0;
    while (!flag_.compare_exchange_weak(expected, 1, std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
      while (flag_.load(std::memory_order_relaxed) != 0) {
        _mm_pause();
      }
      expected = 0;
    }
  }

  /**
   * @brief Releases the spinlock with `std::memory_order_release`.
   */
  void Unlock() noexcept { flag_.store(0, std::memory_order_release); }

 private:
  std::atomic<std::uint64_t> flag_{0};
};

}  // namespace hotpath::mesi_coherency

#endif  // HOTPATH_COMMON_MESI_COHERENCY_MESI_COHERENCY_H_
