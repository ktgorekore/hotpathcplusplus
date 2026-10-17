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
 * @file store_buffer.h
 * @brief Cache-line isolated SPSC ring buffer and store-buffer litmus
 * primitives demonstrating C++20 memory orderings (relaxed, acquire/release,
 * and seq_cst).
 */

#ifndef HOTPATH_COMMON_STORE_BUFFER_STORE_BUFFER_H_
#define HOTPATH_COMMON_STORE_BUFFER_STORE_BUFFER_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace hotpath::store_buffer {

/**
 * @brief Hardware destructive interference size (64-byte L1D cache line).
 */
inline constexpr size_t kCacheLineBytes = 64;

/**
 * @brief Realistic market tick payload transferred across producer and consumer
 * threads.
 */
struct MarketTick {
  uint64_t sequence = 0;
  uint64_t price_micros = 0;
  uint32_t quantity = 0;
  uint32_t venue_id = 0;
};

/**
 * @brief Cache-line isolated Single-Producer Single-Consumer (SPSC) ring buffer
 * comparing `std::memory_order_seq_cst` against
 * `std::memory_order_acquire`/`std::memory_order_release`.
 *
 * @tparam T Element type stored in the ring buffer.
 * @tparam Capacity Power-of-two ring capacity.
 */
template <typename T, size_t Capacity>
class SpscRingBuffer {
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of two.");

 public:
  SpscRingBuffer() = default;

  /**
   * @brief Pushes an item using `std::memory_order_seq_cst` (drains the
   * hardware Store Buffer on every index update).
   * @param item Payload to enqueue.
   * @return True if enqueued, false if the ring buffer is full.
   */
  bool TryPushSeqCst(const T& item) noexcept {
    const size_t w = write_idx_.load(std::memory_order_seq_cst);
    const size_t r = read_idx_.load(std::memory_order_seq_cst);
    if (w - r >= Capacity) {
      return false;
    }
    slots_[w & (Capacity - 1)] = item;
    write_idx_.store(w + 1, std::memory_order_seq_cst);
    return true;
  }

  /**
   * @brief Pops an item using `std::memory_order_seq_cst`.
   * @param out Destination for the dequeued item.
   * @return True if dequeued, false if the ring buffer is empty.
   */
  bool TryPopSeqCst(T& out) noexcept {
    const size_t r = read_idx_.load(std::memory_order_seq_cst);
    const size_t w = write_idx_.load(std::memory_order_seq_cst);
    if (r == w) {
      return false;
    }
    out = slots_[r & (Capacity - 1)];
    read_idx_.store(r + 1, std::memory_order_seq_cst);
    return true;
  }

  /**
   * @brief Pushes an item using `std::memory_order_acquire` and
   * `std::memory_order_release`, avoiding full Store Buffer drains on x86-64
   * TSO while guaranteeing happens-before visibility of the slot payload.
   * @param item Payload to enqueue.
   * @return True if enqueued, false if the ring buffer is full.
   */
  bool TryPushAcquireRelease(const T& item) noexcept {
    const size_t w = write_idx_.load(std::memory_order_relaxed);
    const size_t r = read_idx_.load(std::memory_order_acquire);
    if (w - r >= Capacity) {
      return false;
    }
    slots_[w & (Capacity - 1)] = item;
    write_idx_.store(w + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Pops an item using `std::memory_order_acquire` and
   * `std::memory_order_release`.
   * @param out Destination for the dequeued item.
   * @return True if dequeued, false if the ring buffer is empty.
   */
  bool TryPopAcquireRelease(T& out) noexcept {
    const size_t r = read_idx_.load(std::memory_order_relaxed);
    const size_t w = write_idx_.load(std::memory_order_acquire);
    if (r == w) {
      return false;
    }
    out = slots_[r & (Capacity - 1)];
    read_idx_.store(r + 1, std::memory_order_release);
    return true;
  }

 private:
  alignas(kCacheLineBytes) std::atomic<size_t> write_idx_{0};
  alignas(kCacheLineBytes) std::atomic<size_t> read_idx_{0};
  alignas(kCacheLineBytes) std::array<T, Capacity> slots_{};
};

}  // namespace hotpath::store_buffer

#endif  // HOTPATH_COMMON_STORE_BUFFER_STORE_BUFFER_H_
