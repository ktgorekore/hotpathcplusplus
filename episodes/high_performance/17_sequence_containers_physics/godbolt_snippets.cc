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
 * @file godbolt_snippets.cc
 * @brief Self-contained Compiler Explorer (Godbolt) inspection functions for
 *        Episode 17 ("Sequence Containers on Real Hardware").
 *
 * Designed to be compiled with `-std=c++20 -O3 -march=native -masm=intel -S`
 * to inspect three concrete silicon behaviors side by side:
 *   1. `ScanRuleVector` vs. `ScanRuleList`:
 *      Constant +64B cache-line stride (`add rax, 64`) unlocking hardware
 *      prefetching vs. a serial pointer-chasing load (`mov rax, [rax]`).
 *   2. `IndexCompactRing` vs. `IndexStdDeque`:
 *      A 6-instruction branchless bitwise ring lookup vs. `std::deque`'s
 *      two-level pointer-map arithmetic and double dereference.
 *   3. `PushSessionWithStdQueue` vs. `PushSessionWithRingQueue`:
 *      `std::move_if_noexcept` copy fallback vs. pointer-stealing move.
 */

#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <queue>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace hotpath::episode17::godbolt {

/** @brief Minimal 32B contiguous power-of-2 ring queue with `noexcept` move. */
struct MiniRingQueue {
  std::uint64_t* data = nullptr;
  std::size_t cap = 0;
  std::size_t head = 0;
  std::size_t size = 0;

  MiniRingQueue() noexcept = default;
  MiniRingQueue(const MiniRingQueue& other)
      : data(other.cap ? new std::uint64_t[other.cap] : nullptr),
        cap(other.cap),
        head(other.head),
        size(other.size) {
    for (std::size_t i = 0; i < cap; ++i) {
      data[i] = other.data[i];
    }
  }
  MiniRingQueue(MiniRingQueue&& other) noexcept
      : data(std::exchange(other.data, nullptr)),
        cap(std::exchange(other.cap, 0)),
        head(std::exchange(other.head, 0)),
        size(std::exchange(other.size, 0)) {}
  ~MiniRingQueue() { delete[] data; }
};

/** @brief Rule-of-Zero session embedding `std::queue` (`std::deque`). */
struct SessionWithStdQueue {
  std::uint64_t session_id = 0;
  std::string endpoint_url;
  std::vector<std::byte> read_buffer;
  std::queue<std::uint64_t> pending_message_ids;
};

/** @brief Rule-of-Zero session embedding `noexcept`-movable `MiniRingQueue`. */
struct SessionWithRingQueue {
  std::uint64_t session_id = 0;
  std::string endpoint_url;
  std::vector<std::byte> read_buffer;
  MiniRingQueue pending_message_ids;
};

#if defined(__GLIBCXX__)
static_assert(!std::is_nothrow_move_constructible_v<SessionWithStdQueue>);
#endif
static_assert(std::is_nothrow_move_constructible_v<SessionWithRingQueue>);

/** @brief Triggers vector reallocation on Rule-of-Zero `std::queue` session. */
void PushSessionWithStdQueue(std::vector<SessionWithStdQueue>& vec,
                             SessionWithStdQueue&& s) {
  vec.push_back(std::move(s));
}

/** @brief Triggers vector reallocation on `noexcept` ring-queue session. */
void PushSessionWithRingQueue(std::vector<SessionWithRingQueue>& vec,
                              SessionWithRingQueue&& s) {
  vec.push_back(std::move(s));
}

/** @brief Random-access read from `std::deque<uint64_t>` (2-level chunk map).
 */
std::uint64_t IndexStdDeque(const std::deque<std::uint64_t>& dq,
                            std::size_t idx) noexcept {
  return dq[idx];
}

/** @brief Random-access read from contiguous power-of-2 ring buffer. */
std::uint64_t IndexCompactRing(const MiniRingQueue& rq,
                               std::size_t idx) noexcept {
  return rq.data[(rq.head + idx) & (rq.cap - 1U)];
}

/** @brief 64B SubscriptionRule for alert-routing scan codegen comparison. */
struct alignas(64) Rule64 {
  std::uint64_t rule_id;
  std::int64_t severity_weight;
  std::uint32_t threshold_limit;
  std::uint32_t current_value;
  std::byte pad[40];
};

/** @brief Contiguous vector alert-rule table scan. */
std::uint64_t ScanRuleVector(const std::vector<Rule64>& rules) noexcept {
  std::uint64_t sum = 0;
  for (const auto& r : rules) {
    sum += static_cast<std::uint64_t>(r.severity_weight) *
           static_cast<std::uint64_t>(r.threshold_limit - r.current_value);
  }
  return sum;
}

/** @brief Pointer-chasing `std::list` alert-rule table scan. */
std::uint64_t ScanRuleList(const std::list<Rule64>& rules) noexcept {
  std::uint64_t sum = 0;
  for (const auto& r : rules) {
    sum += static_cast<std::uint64_t>(r.severity_weight) *
           static_cast<std::uint64_t>(r.threshold_limit - r.current_value);
  }
  return sum;
}

}  // namespace hotpath::episode17::godbolt
