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

#ifndef HOTPATH_COMMON_SEQUENCE_CONTAINERS_SEQUENCE_CONTAINERS_H_
#define HOTPATH_COMMON_SEQUENCE_CONTAINERS_SEQUENCE_CONTAINERS_H_

/**
 * @file sequence_containers.h
 * @brief Episode 17 ("Sequence Containers on Real Hardware") production domain
 *        models, contiguous ring deque, and benchmark kernels.
 *
 * All three comprehensive backend stories live inside one cohesive service:
 * `RealtimeGatewayService` (a multi-tenant event/alert router and WebSocket
 * messaging gateway):
 *   - Story 1 (Memory Layout vs. Big-O & Dual-Queue Active Connections):
 *       * 1A (Active Subscription Rule Evaluator on an Aged Heap): `64B`
 *         `SubscriptionRule` entries undergoing periodic full-table alert/rule
 *         evaluation sweeps (`500` ticks — where a binary heap `priority_queue`
 *         or exact-key hash map cannot replace full-table evaluation) and
 *         arbitrary mid-table unsubscribes (`512` across `8,192` rules),
 *         comparing `std::list` (linear AND `O(1)` iterator-map erase) against
 *         `std::vector` (ordered `erase`, `O(1)` sparse-set indexed swap-pop,
 *         and lazy tombstone flag + batch `std::erase_if` compaction).
 *       * 1B (Where Intrusive Lists Win): Pinned `256B` non-movable
 *         `ActiveConnection` contexts registered with OS `epoll` and belonging
 *         simultaneously to an Active LRU eviction list and an Idle-Timeout
 *         expiration list across `16,384` live sockets and `200,000` skewed
 *         events, comparing `boost::intrusive::list` against `std::list`
 *         `splice`, unindexed `vector` (`find + rotate`), AND `O(1)` indexed
 *         `vector::rotate` (zero linear search, pure pointer-shift cost).
 *   - Story 2 (`std::deque` Cross-STL Behavior vs. Contiguous Ring Buffers):
 *       * 2A (Idle Per-Connection Queue Memory Footprint): `10,000` and
 *         `100,000` empty per-connection `OutboundMessage` FIFO queues
 *         comparing `std::deque`, `boost::circular_buffer_space_optimized`, and
 *         growable `CompactRingDeque`.
 *       * 2B (High-Volume FIFO Message Streaming): `8,000,000` `push_back` +
 *         `pop_front` operations both on a single worker channel (depth
 *         `1,024`) and multiplexed across `1,024` active client sockets,
 *         verified with an order-sensitive non-zero rolling hash.
 *   - Story 3 (`std::vector` Growth, Rule-of-Zero `std::queue`, `noexcept` &
 *     Pointer Indirection):
 *       * Growing a slot-indexed `ClientSession` registry (`150,000` sessions
 *         reconnecting during a gateway rollout), isolating `std::deque`'s
 *         per-move allocation overhead (via a `.reserve()` control) from
 *         `std::vector`'s `std::move_if_noexcept` copy fallback, alongside
 *         `CompactRingDeque`, `std::vector<std::unique_ptr<ClientSession>>`
 *         (heap pointer indirection control), `folly::fbvector` (`1.5x`), and
 *         `folly::IsRelocatable`.
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <queue>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "boost/circular_buffer.hpp"
#include "boost/intrusive/list.hpp"
#include "folly/FBVector.h"

namespace hotpath::episode17 {

// -----------------------------------------------------------------------------
// Deterministic PRNG, Order-Sensitive Rolling Hash & Process Memory Helpers
// -----------------------------------------------------------------------------

/** @brief Deterministic seed for Story 1 scheduled tasks & connection events.
 */
inline constexpr std::uint64_t kSeedTasks = 0x5441534B17ULL;
/** @brief Deterministic seed for Story 2 outbound FIFO messages. */
inline constexpr std::uint64_t kSeedMessages = 0x4D53475317ULL;
/** @brief Deterministic seed for Story 3 active client session ingestion. */
inline constexpr std::uint64_t kSeedSessions = 0x5345535317ULL;
/** @brief Deterministic seed for aged-heap fragmentation noise. */
inline constexpr std::uint64_t kSeedHeapNoise = 0x99990017ULL;

/**
 * @brief Stateful SplitMix64 PRNG for deterministic cross-language parity.
 * @param state Mutable 64-bit PRNG state.
 * @return Next uniformly distributed 64-bit unsigned integer.
 */
[[nodiscard]] inline std::uint64_t SplitMix64(std::uint64_t& state) noexcept {
  state += 0x9e3779b97f4a7c15ULL;
  std::uint64_t z = state;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

/**
 * @brief Non-commutative rolling accumulator verifying exact sequence order.
 * @param acc Current rolling hash state.
 * @param val Next element digest.
 * @return Updated non-commutative rolling hash.
 */
[[nodiscard]] inline std::uint64_t MixOrderHash(std::uint64_t acc,
                                                std::uint64_t val) noexcept {
  return std::rotl(acc, 7) ^ (val + 0x9e3779b97f4a7c15ULL);
}

/**
 * @brief Reads current process Resident Set Size (`VmRSS`) from Linux procfs.
 * @return Current `VmRSS` in bytes, or 0 if unavailable.
 */
[[nodiscard]] inline std::uint64_t ReadProcessVmRssBytes() {
  std::ifstream status("/proc/self/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      std::size_t pos = line.find_first_of("0123456789");
      if (pos != std::string::npos) {
        return std::stoull(line.substr(pos)) * 1024ULL;
      }
    }
  }
  return 0;
}

/**
 * @class HeapChurnGuard
 * @brief Pre-fragments the allocator heap during fixture setup (strictly
 *        outside timed benchmark loops) so linked nodes sit across realistic
 *        long-running service addresses rather than pristine startup memory.
 */
class HeapChurnGuard {
 public:
  /**
   * @brief Allocates mixed-size blocks and releases half to create realistic
   *        free-list holes across heap arenas.
   * @param num_blocks Total temporary blocks to allocate before partial free.
   * @param seed Deterministic PRNG seed controlling block sizes and churn.
   */
  explicit HeapChurnGuard(std::size_t num_blocks = 24'000,
                          std::uint64_t seed = kSeedHeapNoise) {
    blocks_.reserve(num_blocks);
    std::uint64_t rng = seed;
    for (std::size_t i = 0; i < num_blocks; ++i) {
      const std::size_t sz =
          32U + static_cast<std::size_t>(SplitMix64(rng) % 512U);
      auto buf = std::make_unique<std::byte[]>(sz);
      buf[0] = static_cast<std::byte>(i & 0xFFU);
      buf[sz - 1] = static_cast<std::byte>((i >> 8) & 0xFFU);
      blocks_.push_back(std::move(buf));
    }
    for (std::size_t i = 0; i < num_blocks; i += 2) {
      blocks_[i].reset();
    }
  }

  /**
   * @brief Allocates a small transient block interleaved during fixture setup
   *        so adjacent logical nodes do not share cache lines.
   * @param rng Mutable PRNG state.
   */
  void InterleaveSetupNoise(std::uint64_t& rng) {
    const std::size_t sz =
        48U + static_cast<std::size_t>(SplitMix64(rng) % 256U);
    auto buf = std::make_unique<std::byte[]>(sz);
    buf[0] = static_cast<std::byte>(rng & 0xFFU);
    noise_.push_back(std::move(buf));
  }

  /** @brief Releases interleaved setup blocks while retaining arena holes. */
  void ReleaseSetupNoise() { noise_.clear(); }

 private:
  std::vector<std::unique_ptr<std::byte[]>> blocks_;
  std::vector<std::unique_ptr<std::byte[]>> noise_;
};

// -----------------------------------------------------------------------------
// Contiguous Power-of-2 Growable Ring Deque (`CompactRingDeque<T>`)
// -----------------------------------------------------------------------------

/**
 * @class CompactRingDeque
 * @brief Contiguous, dynamically growable power-of-2 circular ring buffer
 *        modeled after Chromium's `base::circular_deque` and Rust's `VecDeque`.
 *
 * Key architectural contrasts:
 *   - vs. `std::deque`: Zero allocations when empty, single contiguous array,
 *     zero steady-state allocations under `push_back` + `pop_front`, and an
 *     unconditionally `noexcept` move constructor.
 *   - vs. `boost::circular_buffer`: Dynamically grows capacity when full
 *     rather than silently overwriting the oldest queued element.
 *
 * @tparam T Element value type.
 */
template <typename T>
class CompactRingDeque {
 public:
  using value_type = T;
  using size_type = std::size_t;

  /** @brief Constructs an empty ring deque with zero heap allocation. */
  CompactRingDeque() noexcept = default;

  /**
   * @brief Constructs an empty ring deque with pre-reserved power-of-2 slots.
   * @param min_capacity Minimum number of elements before next reallocation.
   */
  explicit CompactRingDeque(size_type min_capacity) {
    if (min_capacity > 0) {
      Reserve(min_capacity);
    }
  }

  /**
   * @brief Copy-constructs a compact ring buffer containing all elements.
   * @param other Source ring deque to copy from.
   */
  CompactRingDeque(const CompactRingDeque& other) {
    if (other.size_ == 0) {
      return;
    }
    const size_type cap = NextPowerOfTwo(other.size_);
    data_ = AllocateRaw(cap);
    cap_ = cap;
    head_ = 0;
    size_type constructed = 0;
    try {
      for (size_type i = 0; i < other.size_; ++i) {
        std::construct_at(data_ + i, other[i]);
        ++constructed;
      }
      size_ = constructed;
    } catch (...) {
      for (size_type i = 0; i < constructed; ++i) {
        std::destroy_at(data_ + i);
      }
      DeallocateRaw(data_, cap_);
      data_ = nullptr;
      cap_ = 0;
      throw;
    }
  }

  /**
   * @brief Unconditionally `noexcept` move constructor (zero allocation).
   * @param other Source ring deque left empty after move.
   */
  CompactRingDeque(CompactRingDeque&& other) noexcept
      : data_(std::exchange(other.data_, nullptr)),
        cap_(std::exchange(other.cap_, 0)),
        head_(std::exchange(other.head_, 0)),
        size_(std::exchange(other.size_, 0)) {}

  /**
   * @brief Copy-assignment operator with strong exception safety.
   * @param other Source ring deque to copy from.
   * @return Reference to `*this`.
   */
  CompactRingDeque& operator=(const CompactRingDeque& other) {
    if (this != &other) {
      CompactRingDeque copy(other);
      Swap(copy);
    }
    return *this;
  }

  /**
   * @brief Unconditionally `noexcept` move-assignment operator.
   * @param other Source ring deque left empty after move.
   * @return Reference to `*this`.
   */
  CompactRingDeque& operator=(CompactRingDeque&& other) noexcept {
    if (this != &other) {
      DestroyAllAndFree();
      data_ = std::exchange(other.data_, nullptr);
      cap_ = std::exchange(other.cap_, 0);
      head_ = std::exchange(other.head_, 0);
      size_ = std::exchange(other.size_, 0);
    }
    return *this;
  }

  /** @brief Destroys live elements and frees the contiguous backing buffer. */
  ~CompactRingDeque() noexcept { DestroyAllAndFree(); }

  /**
   * @brief Swaps internal buffers and indices in O(1) without throwing.
   * @param other Target ring deque to swap state with.
   */
  void Swap(CompactRingDeque& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(cap_, other.cap_);
    std::swap(head_, other.head_);
    std::swap(size_, other.size_);
  }

  /**
   * @brief Checks whether the queue holds zero elements.
   * @return `true` if the queue is empty, `false` otherwise.
   */
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  /**
   * @brief Returns the number of live elements currently in the queue.
   * @return Current live element count.
   */
  [[nodiscard]] size_type size() const noexcept { return size_; }

  /**
   * @brief Returns the allocated slot capacity (0 or power of two).
   * @return Allocated backing array capacity in elements.
   */
  [[nodiscard]] size_type capacity() const noexcept { return cap_; }

  /**
   * @brief Ensures at least `min_capacity` slots are allocated contiguously.
   * @param min_capacity Required element capacity.
   */
  void Reserve(size_type min_capacity) {
    if (min_capacity <= cap_) {
      return;
    }
    GrowTo(NextPowerOfTwo(min_capacity));
  }

  /**
   * @brief Appends a newly constructed element at the back in amortized O(1).
   * @tparam Args Constructor argument types forwarded to `T`.
   * @param args Constructor arguments.
   * @return Reference to the emplaced element.
   */
  template <typename... Args>
  T& emplace_back(Args&&... args) {
    if (size_ == cap_) [[unlikely]] {
      GrowCold();
    }
    const size_type tail = (head_ + size_) & (cap_ - 1U);
    T* slot = std::construct_at(data_ + tail, std::forward<Args>(args)...);
    ++size_;
    return *slot;
  }

  /**
   * @brief Appends an element by copy to the back of the queue.
   * @param value Element to append.
   */
  void push_back(const T& value) { emplace_back(value); }

  /**
   * @brief Appends an element by move to the back of the queue.
   * @param value Element to move-append.
   */
  void push_back(T&& value) { emplace_back(std::move(value)); }

  /** @brief Removes the front element in O(1) with zero deallocation. */
  void pop_front() noexcept {
    std::destroy_at(data_ + head_);
    head_ = (head_ + 1U) & (cap_ - 1U);
    --size_;
  }

  /**
   * @brief Accesses the front element mutably.
   * @return Mutable reference to the front element.
   */
  [[nodiscard]] T& front() noexcept { return data_[head_]; }

  /**
   * @brief Accesses the front element immutably.
   * @return Const reference to the front element.
   */
  [[nodiscard]] const T& front() const noexcept { return data_[head_]; }

  /**
   * @brief Accesses the logical `idx`-th element mutably.
   * @param idx Zero-based logical offset from the front.
   * @return Mutable reference to the element at `idx`.
   */
  [[nodiscard]] T& operator[](size_type idx) noexcept {
    return data_[(head_ + idx) & (cap_ - 1U)];
  }

  /**
   * @brief Accesses the logical `idx`-th element immutably.
   * @param idx Zero-based logical offset from the front.
   * @return Const reference to the element at `idx`.
   */
  [[nodiscard]] const T& operator[](size_type idx) const noexcept {
    return data_[(head_ + idx) & (cap_ - 1U)];
  }

  /**
   * @brief Iterates across all live elements via at most two contiguous spans.
   * @tparam Fn Callable accepting `const T&`.
   * @param fn Visitor invoked in FIFO order from front to back.
   */
  template <typename Fn>
  void ForEach(Fn&& fn) const {
    if (size_ == 0) {
      return;
    }
    const size_type first_leg = std::min(size_, cap_ - head_);
    for (size_type i = 0; i < first_leg; ++i) {
      fn(data_[head_ + i]);
    }
    const size_type second_leg = size_ - first_leg;
    for (size_type i = 0; i < second_leg; ++i) {
      fn(data_[i]);
    }
  }

 private:
  static constexpr size_type NextPowerOfTwo(size_type n) noexcept {
    if (n <= 8U) {
      return 8U;
    }
    return std::bit_ceil(n);
  }

  static T* AllocateRaw(size_type count) {
    return std::allocator<T>{}.allocate(count);
  }

  static void DeallocateRaw(T* ptr, size_type count) noexcept {
    if (ptr != nullptr) {
      std::allocator<T>{}.deallocate(ptr, count);
    }
  }

  [[gnu::noinline]] void GrowCold() { GrowTo(cap_ == 0 ? 8U : cap_ * 2U); }

  void GrowTo(size_type new_cap) {
    T* new_buf = AllocateRaw(new_cap);
    size_type moved = 0;
    try {
      const size_type first_leg =
          (size_ == 0) ? 0U : std::min(size_, cap_ - head_);
      for (size_type i = 0; i < first_leg; ++i) {
        std::construct_at(new_buf + moved,
                          std::move_if_noexcept(data_[head_ + i]));
        ++moved;
      }
      const size_type second_leg = size_ - first_leg;
      for (size_type i = 0; i < second_leg; ++i) {
        std::construct_at(new_buf + moved, std::move_if_noexcept(data_[i]));
        ++moved;
      }
    } catch (...) {
      for (size_type i = 0; i < moved; ++i) {
        std::destroy_at(new_buf + i);
      }
      DeallocateRaw(new_buf, new_cap);
      throw;
    }

    if (data_ != nullptr) {
      const size_type first_leg = std::min(size_, cap_ - head_);
      for (size_type i = 0; i < first_leg; ++i) {
        std::destroy_at(data_ + head_ + i);
      }
      const size_type second_leg = size_ - first_leg;
      for (size_type i = 0; i < second_leg; ++i) {
        std::destroy_at(data_ + i);
      }
      DeallocateRaw(data_, cap_);
    }

    data_ = new_buf;
    cap_ = new_cap;
    head_ = 0;
  }

  void DestroyAllAndFree() noexcept {
    if (data_ == nullptr) {
      return;
    }
    const size_type first_leg = std::min(size_, cap_ - head_);
    for (size_type i = 0; i < first_leg; ++i) {
      std::destroy_at(data_ + head_ + i);
    }
    const size_type second_leg = size_ - first_leg;
    for (size_type i = 0; i < second_leg; ++i) {
      std::destroy_at(data_ + i);
    }
    DeallocateRaw(data_, cap_);
    data_ = nullptr;
    cap_ = 0;
    head_ = 0;
    size_ = 0;
  }

  T* data_ = nullptr;
  size_type cap_ = 0;
  size_type head_ = 0;
  size_type size_ = 0;
};

static_assert(sizeof(CompactRingDeque<std::uint64_t>) == 32,
              "CompactRingDeque must stay 32 bytes (4 words)");
static_assert(
    std::is_nothrow_move_constructible_v<CompactRingDeque<std::uint64_t>>,
    "CompactRingDeque must be nothrow move constructible");

// =============================================================================
// STORY 1: Active Alert / Event-Routing Subscription Table (`SubscriptionRule`)
//          & Pinned Dual-Hook Connection Manager (`ActiveConnection`)
// =============================================================================

/**
 * @struct SubscriptionRule
 * @brief 64-byte cache-line-aligned active alert / event-routing filter rule
 *        evaluated across every incoming telemetry sweep (a workload where a
 *        `priority_queue` or exact-key hash map cannot replace full-table
 *        predicate evaluation, while tenants dynamically unsubscribe
 * mid-table).
 */
struct alignas(64) SubscriptionRule {
  std::uint64_t rule_id = 0;
  std::uint64_t tenant_id = 0;
  std::int64_t severity_weight = 0;
  std::uint64_t expires_at_ms = 0;
  std::uint32_t threshold_limit = 0;
  std::uint32_t current_value = 0;
  std::uint32_t trigger_count = 0;
  std::uint32_t active_flag = 0;  // 1 = active, 0 = unsubscribed tombstone.
  std::array<char, 16> topic_prefix{};
};
static_assert(sizeof(SubscriptionRule) == 64,
              "SubscriptionRule must be 64 bytes");
static_assert(std::is_trivially_copyable_v<SubscriptionRule>,
              "SubscriptionRule must be trivially copyable");

/**
 * @struct IntrusiveRuleNode
 * @brief `SubscriptionRule` payload plus an embedded `boost::intrusive::list`
 *        hook (eliminating separate wrapper allocations on insert/erase).
 */
struct IntrusiveRuleNode {
  SubscriptionRule rule;
  boost::intrusive::list_member_hook<> table_hook;
};

/**
 * @brief Intrusive doubly-linked list over `IntrusiveRuleNode::table_hook`.
 */
using IntrusiveRuleList = boost::intrusive::list<
    IntrusiveRuleNode,
    boost::intrusive::member_hook<IntrusiveRuleNode,
                                  boost::intrusive::list_member_hook<>,
                                  &IntrusiveRuleNode::table_hook>>;

/**
 * @brief Constructs a deterministic 64-byte `SubscriptionRule` record.
 * @param id Unique rule identifier.
 * @param rng Mutable PRNG state.
 * @return Populated `SubscriptionRule`.
 */
[[nodiscard]] inline SubscriptionRule MakeSubscriptionRule(
    std::uint64_t id, std::uint64_t& rng) noexcept {
  SubscriptionRule r;
  r.rule_id = id;
  r.tenant_id = 1000U + (SplitMix64(rng) % 4096U);
  r.severity_weight = 100 + static_cast<std::int64_t>(SplitMix64(rng) % 5000U);
  r.expires_at_ms = 1764090000000ULL + id * 10ULL;
  r.threshold_limit = 100U + static_cast<std::uint32_t>(SplitMix64(rng) % 900U);
  r.current_value = static_cast<std::uint32_t>(SplitMix64(rng) % 50U);
  r.trigger_count = static_cast<std::uint32_t>(id & 3U);
  r.active_flag = 1U;
  std::memcpy(r.topic_prefix.data(), "alerts-prod\0\0\0\0", 16);
  return r;
}

/**
 * @brief Evaluates weighted alert headroom for active rules (`0` if
 *        unsubscribed via tombstone `active_flag == 0`).
 * @param rule Single active subscription rule.
 * @return Weighted score (`severity_weight * (threshold_limit -
 * current_value)`).
 */
[[nodiscard]] inline std::uint64_t EvaluateRuleScore(
    const SubscriptionRule& rule) noexcept {
  if (rule.active_flag == 0U) {
    return 0ULL;
  }
  const std::uint64_t headroom = rule.threshold_limit - rule.current_value;
  return static_cast<std::uint64_t>(rule.severity_weight) * headroom;
}

/**
 * @struct AgedRuleTableFixture
 * @brief Pre-populated aged-heap fixture holding identical `N`
 *        `SubscriptionRule` records across `std::vector`, `std::list`, and
 *        `boost::intrusive::list`. All heap-aging noise runs strictly inside
 *        this constructor outside timed benchmark loops.
 */
struct AgedRuleTableFixture {
  HeapChurnGuard churn;
  std::vector<SubscriptionRule> vec_rules;
  std::list<SubscriptionRule> list_rules;
  std::vector<std::unique_ptr<IntrusiveRuleNode>> intrusive_storage;
  IntrusiveRuleList intrusive_list;

  /**
   * @brief Populates all three containers with realistic heap fragmentation.
   * @param rule_count Number of active subscription rules `N`.
   * @param seed Deterministic PRNG seed.
   */
  explicit AgedRuleTableFixture(std::size_t rule_count,
                                std::uint64_t seed = kSeedTasks)
      : churn(24'000, kSeedHeapNoise ^ rule_count) {
    vec_rules.reserve(rule_count);
    intrusive_storage.reserve(rule_count);
    std::uint64_t rng = seed;
    std::uint64_t noise_rng = kSeedHeapNoise ^ (rule_count * 31U);
    for (std::size_t i = 0; i < rule_count; ++i) {
      const SubscriptionRule r = MakeSubscriptionRule(i + 1U, rng);
      vec_rules.push_back(r);
      list_rules.push_back(r);
      churn.InterleaveSetupNoise(noise_rng);
      auto node = std::make_unique<IntrusiveRuleNode>();
      node->rule = r;
      intrusive_list.push_back(*node);
      intrusive_storage.push_back(std::move(node));
      churn.InterleaveSetupNoise(noise_rng);
    }
    churn.ReleaseSetupNoise();
  }

  /** @brief Unlinks intrusive hooks before releasing owning node storage. */
  ~AgedRuleTableFixture() { intrusive_list.clear(); }

  AgedRuleTableFixture(const AgedRuleTableFixture&) = delete;
  AgedRuleTableFixture& operator=(const AgedRuleTableFixture&) = delete;
};

/**
 * @struct RuleTableWorkload
 * @brief Deterministic active subscription table and mid-table unsubscribe
 *        sequence for Story 1A (`scan_every` controls how many unsubscribes
 *        occur per periodic full-table rule evaluation sweep).
 */
struct RuleTableWorkload {
  std::vector<SubscriptionRule> initial_rules;
  std::vector<std::uint64_t> unsubscribe_ids;
  std::vector<SubscriptionRule> replacement_rules;
  std::size_t scan_every = 8;
};

/**
 * @brief Builds a deterministic Story 1A active subscription evaluator
 * workload.
 * @param rule_count Active rule count `N`.
 * @param num_unsubscribes Number of mid-table unsubscribe + subscribe cycles.
 * @param scan_every Full-table evaluation sweep cadence (every K unsubscribes).
 * @param seed Deterministic PRNG seed.
 * @return Populated `RuleTableWorkload`.
 */
[[nodiscard]] inline RuleTableWorkload BuildRuleTableWorkload(
    std::size_t rule_count, std::size_t num_unsubscribes,
    std::size_t scan_every = 8, std::uint64_t seed = kSeedTasks) {
  RuleTableWorkload wl;
  wl.scan_every = std::max<std::size_t>(1U, scan_every);
  wl.initial_rules.reserve(rule_count);
  wl.unsubscribe_ids.reserve(num_unsubscribes);
  wl.replacement_rules.reserve(num_unsubscribes);

  std::uint64_t rng = seed;
  for (std::size_t i = 0; i < rule_count; ++i) {
    wl.initial_rules.push_back(MakeSubscriptionRule(i + 1U, rng));
  }

  std::vector<std::uint64_t> live_ids;
  live_ids.reserve(rule_count);
  for (const auto& r : wl.initial_rules) {
    live_ids.push_back(r.rule_id);
  }
  std::uint64_t next_id = rule_count + 1U;
  for (std::size_t c = 0; c < num_unsubscribes; ++c) {
    const std::size_t lo = rule_count / 5U;
    const std::size_t span = std::max<std::size_t>(1U, (rule_count * 3U) / 5U);
    const std::size_t idx =
        lo + static_cast<std::size_t>(SplitMix64(rng) % span);
    wl.unsubscribe_ids.push_back(live_ids[idx]);
    SubscriptionRule rep = MakeSubscriptionRule(next_id++, rng);
    wl.replacement_rules.push_back(rep);
    live_ids.erase(live_ids.begin() + static_cast<std::ptrdiff_t>(idx));
    live_ids.push_back(rep.rule_id);
  }
  return wl;
}

/**
 * @struct PreAgedListUnsubscribeState
 * @brief Pre-aged `std::list<SubscriptionRule>` state built outside timed loops
 *        so Story 1A unsubscribe benchmarks contain zero setup noise overhead.
 */
struct PreAgedListUnsubscribeState {
  HeapChurnGuard churn;
  std::list<SubscriptionRule> prototype_list;

  /**
   * @brief Pre-populates `prototype_list` across fragmented heap holes.
   * @param wl Rule table workload.
   */
  explicit PreAgedListUnsubscribeState(const RuleTableWorkload& wl)
      : churn(24'000, kSeedHeapNoise ^ wl.initial_rules.size()) {
    std::uint64_t noise_rng = kSeedHeapNoise ^ (wl.initial_rules.size() * 17U);
    for (const auto& r : wl.initial_rules) {
      prototype_list.push_back(r);
      churn.InterleaveSetupNoise(noise_rng);
    }
    churn.ReleaseSetupNoise();
  }

  /**
   * @brief Resets a working `std::list` in place without reallocating nodes.
   * @param working Mutable list already holding `initial_rules.size()` nodes.
   * @param wl Rule table workload.
   */
  static void RestoreInPlace(std::list<SubscriptionRule>& working,
                             const RuleTableWorkload& wl) {
    auto it = working.begin();
    for (const auto& r : wl.initial_rules) {
      *it = r;
      ++it;
    }
  }
};

/**
 * @brief Runs Story 1A on a pre-aged `std::list` using linear search + `O(1)`
 *        `erase` (zero `InterleaveSetupNoise` inside the timed region).
 * @param rules Pre-aged `std::list` restored to `wl.initial_rules`.
 * @param wl Story 1A rule table workload.
 * @return Aggregate weighted score checksum.
 */
[[nodiscard]] inline std::uint64_t RunRuleTableStdListLinear(
    std::list<SubscriptionRule>& rules, const RuleTableWorkload& wl) {
  std::uint64_t total_score = 0;
  for (std::size_t c = 0; c < wl.unsubscribe_ids.size(); ++c) {
    if ((c % wl.scan_every) == 0U) {
      for (const auto& r : rules) {
        total_score += EvaluateRuleScore(r);
      }
    }
    const std::uint64_t target = wl.unsubscribe_ids[c];
    for (auto it = rules.begin(); it != rules.end(); ++it) {
      if (it->rule_id == target) {
        rules.erase(it);
        break;
      }
    }
    rules.push_back(wl.replacement_rules[c]);
  }
  for (const auto& r : rules) {
    total_score += EvaluateRuleScore(r);
  }
  return total_score;
}

/**
 * @brief Runs Story 1A on a pre-aged `std::list` paired with an `O(1)`
 *        `absl::flat_hash_map<rule_id, list::iterator>` index so unsubscribing
 *        itself requires zero pointer-chasing search.
 * @param rules Pre-aged `std::list` restored to `wl.initial_rules`.
 * @param index Pre-built `rule_id -> iterator` map matching `rules`.
 * @param wl Story 1A rule table workload.
 * @return Aggregate weighted score checksum (exact parity).
 */
[[nodiscard]] inline std::uint64_t RunRuleTableStdListIndexed(
    std::list<SubscriptionRule>& rules,
    absl::flat_hash_map<std::uint64_t, std::list<SubscriptionRule>::iterator>&
        index,
    const RuleTableWorkload& wl) {
  std::uint64_t total_score = 0;
  for (std::size_t c = 0; c < wl.unsubscribe_ids.size(); ++c) {
    if ((c % wl.scan_every) == 0U) {
      for (const auto& r : rules) {
        total_score += EvaluateRuleScore(r);
      }
    }
    const std::uint64_t target = wl.unsubscribe_ids[c];
    auto found = index.find(target);
    if (found != index.end()) {
      rules.erase(found->second);
      index.erase(found);
    }
    rules.push_back(wl.replacement_rules[c]);
    index.emplace(wl.replacement_rules[c].rule_id, std::prev(rules.end()));
  }
  for (const auto& r : rules) {
    total_score += EvaluateRuleScore(r);
  }
  return total_score;
}

/**
 * @brief Runs contiguous `std::vector` rule evaluation + order-preserving
 *        `erase` (`memmove` shift).
 * @param rules Pre-allocated vector restored to `wl.initial_rules`.
 * @param wl Story 1A rule table workload.
 * @return Aggregate weighted score checksum (exact parity).
 */
[[nodiscard]] inline std::uint64_t RunRuleTableVectorOrderedErase(
    std::vector<SubscriptionRule>& rules, const RuleTableWorkload& wl) {
  std::uint64_t total_score = 0;
  for (std::size_t c = 0; c < wl.unsubscribe_ids.size(); ++c) {
    if ((c % wl.scan_every) == 0U) {
      for (const auto& r : rules) {
        total_score += EvaluateRuleScore(r);
      }
    }
    const std::uint64_t target = wl.unsubscribe_ids[c];
    for (auto it = rules.begin(); it != rules.end(); ++it) {
      if (it->rule_id == target) {
        rules.erase(it);
        break;
      }
    }
    rules.push_back(wl.replacement_rules[c]);
  }
  for (const auto& r : rules) {
    total_score += EvaluateRuleScore(r);
  }
  return total_score;
}

/**
 * @brief Runs contiguous `std::vector` rule evaluation + unindexed
 * swap-and-pop.
 * @param rules Pre-allocated vector restored to `wl.initial_rules`.
 * @param wl Story 1A rule table workload.
 * @return Aggregate weighted score checksum (exact parity).
 */
[[nodiscard]] inline std::uint64_t RunRuleTableVectorSwapPop(
    std::vector<SubscriptionRule>& rules, const RuleTableWorkload& wl) {
  std::uint64_t total_score = 0;
  for (std::size_t c = 0; c < wl.unsubscribe_ids.size(); ++c) {
    if ((c % wl.scan_every) == 0U) {
      for (const auto& r : rules) {
        total_score += EvaluateRuleScore(r);
      }
    }
    const std::uint64_t target = wl.unsubscribe_ids[c];
    for (std::size_t i = 0; i < rules.size(); ++i) {
      if (rules[i].rule_id == target) {
        rules[i] = rules.back();
        rules.pop_back();
        break;
      }
    }
    rules.push_back(wl.replacement_rules[c]);
  }
  for (const auto& r : rules) {
    total_score += EvaluateRuleScore(r);
  }
  return total_score;
}

/**
 * @brief Runs the production Sparse-Set pattern (`std::vector` + `O(1)`
 *        `absl::flat_hash_map<rule_id, slot_index>` + `O(1)` swap-and-pop):
 *        zero linear search, zero compaction passes, 100% dense contiguous
 *        iteration (optimal when relative rule order does not matter).
 * @param rules Pre-allocated vector restored to `wl.initial_rules`.
 * @param index Pre-built `rule_id -> slot_index` map matching `rules`.
 * @param wl Story 1A rule table workload.
 * @return Aggregate weighted score checksum (exact parity).
 */
[[nodiscard]] inline std::uint64_t RunRuleTableVectorIndexedSwapPop(
    std::vector<SubscriptionRule>& rules,
    absl::flat_hash_map<std::uint64_t, std::size_t>& index,
    const RuleTableWorkload& wl) {
  std::uint64_t total_score = 0;
  for (std::size_t c = 0; c < wl.unsubscribe_ids.size(); ++c) {
    if ((c % wl.scan_every) == 0U) {
      for (const auto& r : rules) {
        total_score += EvaluateRuleScore(r);
      }
    }
    const std::uint64_t target = wl.unsubscribe_ids[c];
    auto found = index.find(target);
    if (found != index.end()) {
      const std::size_t slot = found->second;
      index.erase(found);
      const std::size_t last = rules.size() - 1U;
      if (slot != last) {
        rules[slot] = rules[last];
        index[rules[slot].rule_id] = slot;
      }
      rules.pop_back();
    }
    index.emplace(wl.replacement_rules[c].rule_id, rules.size());
    rules.push_back(wl.replacement_rules[c]);
  }
  for (const auto& r : rules) {
    total_score += EvaluateRuleScore(r);
  }
  return total_score;
}

/**
 * @brief Runs order-preserving `std::vector` + `O(1)` indexed tombstone flag
 *        (`active_flag = 0`) + periodic `std::erase_if` compaction, combining
 *        `O(1)` unsubscribe with contiguous hardware-prefetched scans when
 *        rule registration order must be preserved.
 * @param rules Pre-allocated vector restored to `wl.initial_rules`.
 * @param index Pre-built `rule_id -> slot_index` map matching `rules`.
 * @param wl Story 1A rule table workload.
 * @return Aggregate weighted score checksum (exact parity).
 */
[[nodiscard]] inline std::uint64_t RunRuleTableVectorTombstoneCompact(
    std::vector<SubscriptionRule>& rules,
    absl::flat_hash_map<std::uint64_t, std::size_t>& index,
    const RuleTableWorkload& wl) {
  std::uint64_t total_score = 0;
  std::size_t dead_slots = 0;
  for (std::size_t c = 0; c < wl.unsubscribe_ids.size(); ++c) {
    if ((c % wl.scan_every) == 0U) {
      for (const auto& r : rules) {
        total_score += EvaluateRuleScore(r);
      }
    }
    const std::uint64_t target = wl.unsubscribe_ids[c];
    auto found = index.find(target);
    if (found != index.end()) {
      rules[found->second].active_flag = 0U;
      index.erase(found);
      ++dead_slots;
    }
    index.emplace(wl.replacement_rules[c].rule_id, rules.size());
    rules.push_back(wl.replacement_rules[c]);

    if (dead_slots >= 128U) {
      std::erase_if(
          rules, [](const SubscriptionRule& r) { return r.active_flag == 0U; });
      index.clear();
      for (std::size_t i = 0; i < rules.size(); ++i) {
        index.emplace(rules[i].rule_id, i);
      }
      dead_slots = 0;
    }
  }
  std::erase_if(rules,
                [](const SubscriptionRule& r) { return r.active_flag == 0U; });
  for (const auto& r : rules) {
    total_score += EvaluateRuleScore(r);
  }
  return total_score;
}

// -----------------------------------------------------------------------------
// STORY 1B: Where Intrusive Lists Shine — Pinned 256B Dual-Hook Connections
//           (Active LRU Eviction List + Idle-Timeout Expiration List)
// -----------------------------------------------------------------------------

/**
 * @struct ActiveConnection
 * @brief 256-byte non-movable active client socket/session context referenced
 *        directly by OS event loops (`epoll` / `io_uring`) and belonging
 *        simultaneously to two ordered lists with zero list-node allocations:
 *          1. `lru_hook`: Active LRU eviction list.
 *          2. `timeout_hook`: Idle-timeout expiration list.
 */
struct alignas(64) ActiveConnection {
  std::uint64_t conn_id = 0;
  std::uint64_t last_seen_ms = 0;
  std::uint64_t bytes_rx = 0;
  std::uint64_t bytes_tx = 0;
  std::uint32_t fd = 0;
  std::uint32_t tls_state = 0;
  std::array<std::uint64_t, 12> socket_metrics{};
  std::array<char, 88> peer_host{};
  boost::intrusive::list_member_hook<> lru_hook;
  boost::intrusive::list_member_hook<> timeout_hook;

  /** @brief Default-constructs an idle, pinned connection context. */
  ActiveConnection() = default;
  ActiveConnection(const ActiveConnection&) = delete;
  ActiveConnection& operator=(const ActiveConnection&) = delete;
  ActiveConnection(ActiveConnection&&) = delete;
  ActiveConnection& operator=(ActiveConnection&&) = delete;
};

static_assert(sizeof(ActiveConnection) == 256,
              "ActiveConnection must be exactly 256 bytes (4 cache lines)");

/** @brief Intrusive LRU eviction list over `ActiveConnection::lru_hook`. */
using ConnectionLruList = boost::intrusive::list<
    ActiveConnection,
    boost::intrusive::member_hook<ActiveConnection,
                                  boost::intrusive::list_member_hook<>,
                                  &ActiveConnection::lru_hook>>;

/** @brief Intrusive timeout list over `ActiveConnection::timeout_hook`. */
using ConnectionTimeoutList = boost::intrusive::list<
    ActiveConnection,
    boost::intrusive::member_hook<ActiveConnection,
                                  boost::intrusive::list_member_hook<>,
                                  &ActiveConnection::timeout_hook>>;

/**
 * @struct DualQueueEventWorkload
 * @brief Deterministic sequence of socket activity indices touching
 *        `num_connections` pinned client connections (80/20 hot-client skew).
 */
struct DualQueueEventWorkload {
  std::size_t num_connections = 0;
  std::vector<std::uint32_t> active_indices;
};

/**
 * @brief Generates a deterministic 80/20 hot-client socket activity workload
 *        shared verbatim across C++20, Rust 1.91, and Java 21 LTS.
 * @param num_connections Total live pinned connections (`16,384` default).
 * @param num_events Number of LRU-promote + timeout-refresh events (`200,000`).
 * @param seed Deterministic PRNG seed.
 * @return Populated `DualQueueEventWorkload`.
 */
[[nodiscard]] inline DualQueueEventWorkload BuildDualQueueEventWorkload(
    std::size_t num_connections = 16'384, std::size_t num_events = 200'000,
    std::uint64_t seed = kSeedTasks ^ 0xBEEFULL) {
  DualQueueEventWorkload wl;
  wl.num_connections = num_connections;
  wl.active_indices.reserve(num_events);
  const std::size_t hot_band = std::max<std::size_t>(1U, num_connections / 5U);
  std::uint64_t rng = seed;
  for (std::size_t i = 0; i < num_events; ++i) {
    const std::uint64_t r = SplitMix64(rng);
    const std::uint64_t pick = SplitMix64(rng);
    const std::size_t idx =
        ((r % 100ULL) < 80ULL)
            ? static_cast<std::size_t>(pick % hot_band)
            : static_cast<std::size_t>(pick % num_connections);
    wl.active_indices.push_back(static_cast<std::uint32_t>(idx));
  }
  return wl;
}

/**
 * @brief Executes Story 1B using dual `boost::intrusive::list` queues: `O(1)`
 *        relinking across both LRU and timeout lists with `1` slab allocation
 *        and `0` wrapper-node allocations.
 * @param wl Deterministic connection activity workload.
 * @return Order-sensitive verification checksum across both queues.
 */
[[nodiscard]] inline std::uint64_t RunDualQueueIntrusive(
    const DualQueueEventWorkload& wl) {
  auto slab = std::make_unique<ActiveConnection[]>(wl.num_connections);
  ConnectionLruList lru;
  ConnectionTimeoutList timeouts;
  for (std::size_t i = 0; i < wl.num_connections; ++i) {
    slab[i].conn_id = i + 1U;
    slab[i].fd = static_cast<std::uint32_t>(i + 10U);
    lru.push_back(slab[i]);
    timeouts.push_back(slab[i]);
  }

  std::uint64_t ts = 1;
  for (std::uint32_t idx : wl.active_indices) {
    ActiveConnection& conn = slab[idx];
    conn.last_seen_ms = ts++;
    conn.bytes_rx += 64U;
    lru.erase(lru.iterator_to(conn));
    lru.push_front(conn);
    timeouts.erase(timeouts.iterator_to(conn));
    timeouts.push_back(conn);
  }

  const std::uint64_t checksum =
      lru.front().conn_id ^ (lru.back().conn_id << 16) ^
      (timeouts.front().conn_id << 32) ^ timeouts.back().last_seen_ms;
  lru.clear();
  timeouts.clear();
  return checksum;
}

/**
 * @brief Executes Story 1B using two `std::list<ActiveConnection*>` queues
 *        with stored iterators (`splice`), isolating wrapper-node overhead
 *        (`2 * num_connections` heap allocations).
 * @param wl Deterministic connection activity workload.
 * @return Verification checksum (exact parity with `RunDualQueueIntrusive`).
 */
[[nodiscard]] inline std::uint64_t RunDualQueueStdListSplice(
    const DualQueueEventWorkload& wl) {
  auto slab = std::make_unique<ActiveConnection[]>(wl.num_connections);
  std::list<ActiveConnection*> lru;
  std::list<ActiveConnection*> timeouts;
  std::vector<std::list<ActiveConnection*>::iterator> lru_pos;
  std::vector<std::list<ActiveConnection*>::iterator> timeout_pos;
  lru_pos.reserve(wl.num_connections);
  timeout_pos.reserve(wl.num_connections);

  for (std::size_t i = 0; i < wl.num_connections; ++i) {
    slab[i].conn_id = i + 1U;
    slab[i].fd = static_cast<std::uint32_t>(i + 10U);
    lru.push_back(&slab[i]);
    lru_pos.push_back(std::prev(lru.end()));
    timeouts.push_back(&slab[i]);
    timeout_pos.push_back(std::prev(timeouts.end()));
  }

  std::uint64_t ts = 1;
  for (std::uint32_t idx : wl.active_indices) {
    ActiveConnection& conn = slab[idx];
    conn.last_seen_ms = ts++;
    conn.bytes_rx += 64U;
    lru.splice(lru.begin(), lru, lru_pos[idx]);
    timeouts.splice(timeouts.end(), timeouts, timeout_pos[idx]);
  }

  return lru.front()->conn_id ^ (lru.back()->conn_id << 16) ^
         (timeouts.front()->conn_id << 32) ^ timeouts.back()->last_seen_ms;
}

/**
 * @brief Executes Story 1B using `std::vector<ActiveConnection*>` (stable
 *        addresses via pointers, requiring linear locate + `rotate` shift).
 * @param wl Deterministic connection activity workload.
 * @return Verification checksum (exact parity with `RunDualQueueIntrusive`).
 */
[[nodiscard]] inline std::uint64_t RunDualQueueVectorOfPointers(
    const DualQueueEventWorkload& wl) {
  auto slab = std::make_unique<ActiveConnection[]>(wl.num_connections);
  std::vector<ActiveConnection*> lru;
  std::vector<ActiveConnection*> timeouts;
  lru.reserve(wl.num_connections);
  timeouts.reserve(wl.num_connections);
  for (std::size_t i = 0; i < wl.num_connections; ++i) {
    slab[i].conn_id = i + 1U;
    slab[i].fd = static_cast<std::uint32_t>(i + 10U);
    lru.push_back(&slab[i]);
    timeouts.push_back(&slab[i]);
  }

  std::uint64_t ts = 1;
  for (std::uint32_t idx : wl.active_indices) {
    ActiveConnection* ptr = &slab[idx];
    ptr->last_seen_ms = ts++;
    ptr->bytes_rx += 64U;

    auto lru_it = std::find(lru.begin(), lru.end(), ptr);
    std::rotate(lru.begin(), lru_it, std::next(lru_it));

    auto to_it = std::find(timeouts.begin(), timeouts.end(), ptr);
    std::rotate(to_it, std::next(to_it), timeouts.end());
  }

  return lru.front()->conn_id ^ (lru.back()->conn_id << 16) ^
         (timeouts.front()->conn_id << 32) ^ timeouts.back()->last_seen_ms;
}

/**
 * @brief Executes Story 1B using compact `4B` slab indices
 *        (`std::vector<std::uint32_t>`) paired with `O(1)` stored slot
 *        positions (`lru_pos`, `timeout_pos`) — eliminating linear `std::find`
 *        completely to isolate pure `O(N)` `memmove` + index-update cost on
 *        strict dual-queue LRU + timeout promotion.
 * @param wl Deterministic connection activity workload.
 * @return Verification checksum (exact parity with `RunDualQueueIntrusive`).
 */
[[nodiscard]] inline std::uint64_t RunDualQueueVectorIndexedRotate(
    const DualQueueEventWorkload& wl) {
  const std::size_t n = wl.num_connections;
  auto slab = std::make_unique<ActiveConnection[]>(n);
  std::vector<std::uint32_t> lru(n);
  std::vector<std::uint32_t> timeouts(n);
  std::vector<std::uint32_t> lru_pos(n);
  std::vector<std::uint32_t> timeout_pos(n);
  for (std::size_t i = 0; i < n; ++i) {
    slab[i].conn_id = i + 1U;
    slab[i].fd = static_cast<std::uint32_t>(i + 10U);
    const auto u = static_cast<std::uint32_t>(i);
    lru[i] = u;
    timeouts[i] = u;
    lru_pos[i] = u;
    timeout_pos[i] = u;
  }

  std::uint64_t ts = 1;
  for (std::uint32_t idx : wl.active_indices) {
    ActiveConnection& conn = slab[idx];
    conn.last_seen_ms = ts++;
    conn.bytes_rx += 64U;

    const std::uint32_t lp = lru_pos[idx];
    if (lp > 0U) {
      std::memmove(lru.data() + 1U, lru.data(), lp * sizeof(std::uint32_t));
      lru[0] = idx;
      for (std::uint32_t k = 1U; k <= lp; ++k) {
        lru_pos[lru[k]] = k;
      }
      lru_pos[idx] = 0U;
    }

    const std::uint32_t tp = timeout_pos[idx];
    const std::uint32_t last = static_cast<std::uint32_t>(n - 1U);
    if (tp < last) {
      std::memmove(timeouts.data() + tp, timeouts.data() + tp + 1U,
                   (last - tp) * sizeof(std::uint32_t));
      timeouts[last] = idx;
      for (std::uint32_t k = tp; k < last; ++k) {
        timeout_pos[timeouts[k]] = k;
      }
      timeout_pos[idx] = last;
    }
  }

  return slab[lru.front()].conn_id ^ (slab[lru.back()].conn_id << 16) ^
         (slab[timeouts.front()].conn_id << 32) ^
         slab[timeouts.back()].last_seen_ms;
}

// =============================================================================
// STORY 2: Outbound FIFO Message Queues (`std::deque` Cross-STL Behavior vs.
//          Contiguous Circular Ring Buffers)
// =============================================================================

/**
 * @struct OutboundMessage
 * @brief 64-byte outbound RPC / notification message queued per client socket
 *        or worker channel.
 */
struct alignas(64) OutboundMessage {
  std::uint64_t message_id = 0;
  std::uint64_t enqueued_ms = 0;
  std::uint32_t channel_id = 0;
  std::uint32_t payload_bytes = 0;
  std::uint32_t priority = 0;
  std::uint32_t flags = 0;
  std::array<std::byte, 32> routing_tag{};
};
static_assert(sizeof(OutboundMessage) == 64,
              "OutboundMessage must be 64 bytes");
static_assert(std::is_trivially_copyable_v<OutboundMessage>,
              "OutboundMessage must be trivially copyable");

/**
 * @brief Constructs a deterministic 64-byte `OutboundMessage`.
 * @param seq Monotonic message sequence number.
 * @return Populated `OutboundMessage`.
 */
[[nodiscard]] inline OutboundMessage MakeOutboundMessage(
    std::uint64_t seq) noexcept {
  OutboundMessage m;
  m.message_id = seq;
  m.enqueued_ms = 1764095000000ULL + seq * 2ULL;
  m.channel_id = static_cast<std::uint32_t>((seq % 256U) * 2U + 1U);
  m.payload_bytes = static_cast<std::uint32_t>(64U + (seq & 0x3FFU));
  m.priority = static_cast<std::uint32_t>(seq & 7U);
  m.flags = 0x4U;
  m.routing_tag[0] = static_cast<std::byte>(seq & 0xFFU);
  return m;
}

/**
 * @brief Computes a verification digest over an `OutboundMessage`.
 * @param msg Outbound message record.
 * @return 64-bit digest.
 */
[[nodiscard]] inline std::uint64_t DigestMessage(
    const OutboundMessage& msg) noexcept {
  return msg.message_id ^ msg.enqueued_ms ^
         (static_cast<std::uint64_t>(msg.channel_id) << 32) ^
         static_cast<std::uint64_t>(msg.payload_bytes + msg.priority) ^
         static_cast<std::uint64_t>(msg.routing_tag[0]);
}

/**
 * @brief Executes Story 2B steady-state FIFO streaming (`num_cycles`
 *        `push_back` + `pop_front` operations at constant queue depth `depth`)
 *        on `std::deque<OutboundMessage>`.
 * @param depth Steady-state queue depth.
 * @param num_cycles Total steady-state push+pop pairs.
 * @return Non-zero order-sensitive rolling hash over processed messages.
 */
[[nodiscard]] inline std::uint64_t RunFifoStreamStdDeque(
    std::size_t depth = 1'024, std::size_t num_cycles = 8'000'000) {
  std::deque<OutboundMessage> q;
  for (std::size_t i = 0; i < depth; ++i) {
    q.push_back(MakeOutboundMessage(i));
  }
  std::uint64_t checksum = 0x17F1F0ULL;
  for (std::size_t i = 0; i < num_cycles; ++i) {
    checksum = MixOrderHash(checksum, DigestMessage(q.front()));
    q.pop_front();
    q.push_back(MakeOutboundMessage(depth + i));
  }
  for (const auto& m : q) {
    checksum = MixOrderHash(checksum, DigestMessage(m));
  }
  return checksum;
}

/**
 * @brief Executes Story 2B steady-state FIFO streaming on bounded
 *        `boost::circular_buffer<OutboundMessage>` (zero steady-state allocs).
 * @param depth Steady-state queue depth (must not be exceeded without
 *        checking `.full()`, as `boost::circular_buffer` overwrites when full).
 * @param num_cycles Total steady-state push+pop pairs.
 * @return Order-sensitive rolling hash (exact parity with `std::deque`).
 */
[[nodiscard]] inline std::uint64_t RunFifoStreamBoostCircular(
    std::size_t depth = 1'024, std::size_t num_cycles = 8'000'000) {
  boost::circular_buffer<OutboundMessage> q(depth);
  for (std::size_t i = 0; i < depth; ++i) {
    q.push_back(MakeOutboundMessage(i));
  }
  std::uint64_t checksum = 0x17F1F0ULL;
  for (std::size_t i = 0; i < num_cycles; ++i) {
    checksum = MixOrderHash(checksum, DigestMessage(q.front()));
    q.pop_front();
    q.push_back(MakeOutboundMessage(depth + i));
  }
  for (const auto& m : q) {
    checksum = MixOrderHash(checksum, DigestMessage(m));
  }
  return checksum;
}

/**
 * @brief Executes Story 2B steady-state FIFO streaming on growable
 *        `CompactRingDeque<OutboundMessage>` (zero steady-state allocs).
 * @param depth Steady-state queue depth.
 * @param num_cycles Total steady-state push+pop pairs.
 * @return Order-sensitive rolling hash (exact parity with `std::deque`).
 */
[[nodiscard]] inline std::uint64_t RunFifoStreamCompactRing(
    std::size_t depth = 1'024, std::size_t num_cycles = 8'000'000) {
  CompactRingDeque<OutboundMessage> q(depth);
  for (std::size_t i = 0; i < depth; ++i) {
    q.push_back(MakeOutboundMessage(i));
  }
  std::uint64_t checksum = 0x17F1F0ULL;
  for (std::size_t i = 0; i < num_cycles; ++i) {
    checksum = MixOrderHash(checksum, DigestMessage(q.front()));
    q.pop_front();
    q.push_back(MakeOutboundMessage(depth + i));
  }
  q.ForEach([&](const OutboundMessage& m) {
    checksum = MixOrderHash(checksum, DigestMessage(m));
  });
  return checksum;
}

/**
 * @brief Executes Story 2B multiplexed gateway FIFO streaming across
 *        `num_queues` active client socket queues (`std::deque` vs.
 *        `CompactRingDeque`), modeling real multi-connection dispatch where
 *        allocator chunks do not sit in a single L1-warm slot.
 * @tparam QueueT `std::deque<OutboundMessage>` or
 *         `CompactRingDeque<OutboundMessage>`.
 * @param num_queues Number of active per-connection queues (`1,024`).
 * @param per_queue_depth Active buffered messages per connection (`16`).
 * @param total_messages Total outbound messages multiplexed across connections.
 * @return Non-zero order-sensitive verification checksum.
 */
template <typename QueueT>
[[nodiscard]] inline std::uint64_t RunMultiplexedGatewayFifo(
    std::size_t num_queues = 1'024, std::size_t per_queue_depth = 16,
    std::size_t total_messages = 8'000'000) {
  std::vector<QueueT> queues(num_queues);
  std::uint64_t seq = 0;
  for (std::size_t q_idx = 0; q_idx < num_queues; ++q_idx) {
    if constexpr (std::is_same_v<QueueT, CompactRingDeque<OutboundMessage>>) {
      queues[q_idx].Reserve(per_queue_depth);
    }
    for (std::size_t d = 0; d < per_queue_depth; ++d) {
      queues[q_idx].push_back(MakeOutboundMessage(seq++));
    }
  }

  std::uint64_t checksum = 0x17F1F0ULL;
  const std::size_t mask = num_queues - 1U;
  for (std::size_t i = 0; i < total_messages; ++i) {
    QueueT& q = queues[(i * 73U) & mask];
    checksum = MixOrderHash(checksum, DigestMessage(q.front()));
    q.pop_front();
    q.push_back(MakeOutboundMessage(seq++));
  }
  for (const auto& q : queues) {
    if constexpr (std::is_same_v<QueueT, CompactRingDeque<OutboundMessage>>) {
      q.ForEach([&](const OutboundMessage& m) {
        checksum = MixOrderHash(checksum, DigestMessage(m));
      });
    } else {
      for (const auto& m : q) {
        checksum = MixOrderHash(checksum, DigestMessage(m));
      }
    }
  }
  return checksum;
}

// =============================================================================
// STORY 3: Growing Slot-Indexed `ClientSession` Registry (`150,000` Reconnects,
//          Rule-of-Zero `std::queue` `noexcept` Cliff & `folly::fbvector`)
// =============================================================================

/**
 * @struct SessionRuleOfZeroWithStdQueue
 * @brief Clean, idiomatic Rule-of-Zero C++20 client session struct embedding a
 *        standard `std::queue<std::uint64_t>` (which wraps `std::deque`).
 *
 * Cross-STL behavior:
 *   - GNU `libstdc++` (GCC & default Linux Clang): `_Deque_base(_Deque_base&&)`
 *     calls `_M_initialize_map(0)` and is `noexcept(false)`.
 *   - Microsoft `MSVC STL`: `deque(deque&&)` allocates a container proxy and is
 *     `noexcept(false)`.
 *   - LLVM `libc++` (macOS / iOS / Android): `deque(deque&&)` is `noexcept`.
 */
struct SessionRuleOfZeroWithStdQueue {
  std::uint64_t session_id = 0;
  std::uint32_t account_id = 0;
  std::uint32_t flags = 0;
  std::string endpoint_url;            // ~52B non-SSO heap string.
  std::vector<std::byte> read_buffer;  // 256B per-session socket read buffer.
  std::queue<std::uint64_t>
      pending_message_ids;  // Default container: std::deque.
};

#if defined(__GLIBCXX__)
static_assert(!std::is_nothrow_move_constructible_v<std::deque<std::uint64_t>>,
              "On libstdc++, std::deque move constructor allocates and is "
              "noexcept(false)");
static_assert(
    !std::is_nothrow_move_constructible_v<SessionRuleOfZeroWithStdQueue>,
    "Embedding std::queue disables Rule-of-Zero nothrow move on libstdc++");
#endif

/**
 * @struct SessionMissingNoexceptMove
 * @brief Session struct using `CompactRingDeque`, with a custom move
 *        constructor (e.g., resetting moved-from identifiers) that omits
 *        `noexcept`—isolating pure `std::move_if_noexcept` copy fallback.
 */
struct SessionMissingNoexceptMove {
  std::uint64_t session_id = 0;
  std::uint32_t account_id = 0;
  std::uint32_t flags = 0;
  std::string endpoint_url;
  std::vector<std::byte> read_buffer;
  CompactRingDeque<std::uint64_t> pending_message_ids;

  SessionMissingNoexceptMove() = default;
  SessionMissingNoexceptMove(const SessionMissingNoexceptMove&) = default;
  SessionMissingNoexceptMove& operator=(const SessionMissingNoexceptMove&) =
      default;

  /**
   * @brief Intentionally omits `noexcept` to isolate `move_if_noexcept`.
   * @param other Source session whose resources are moved into `*this`.
   */
  SessionMissingNoexceptMove(SessionMissingNoexceptMove&& other)
      : session_id(std::exchange(other.session_id, 0)),
        account_id(other.account_id),
        flags(std::exchange(other.flags, 0)),
        endpoint_url(std::move(other.endpoint_url)),
        read_buffer(std::move(other.read_buffer)),
        pending_message_ids(std::move(other.pending_message_ids)) {}

  SessionMissingNoexceptMove& operator=(SessionMissingNoexceptMove&&) = default;
};

static_assert(!std::is_nothrow_move_constructible_v<SessionMissingNoexceptMove>,
              "Move constructor without noexcept must be noexcept(false)");

/**
 * @struct SessionNoexceptRingQueue
 * @brief Clean Rule-of-Zero C++20 session struct using `CompactRingDeque`,
 *        guarded at compile time with `static_assert`.
 */
struct SessionNoexceptRingQueue {
  std::uint64_t session_id = 0;
  std::uint32_t account_id = 0;
  std::uint32_t flags = 0;
  std::string endpoint_url;
  std::vector<std::byte> read_buffer;
  CompactRingDeque<std::uint64_t> pending_message_ids;
};

static_assert(std::is_nothrow_move_constructible_v<SessionNoexceptRingQueue>,
              "Guardrail: SessionNoexceptRingQueue must be nothrow movable");

/**
 * @struct SessionRelocatable
 * @brief Bitwise-relocatable session struct safe for `folly::IsRelocatable`
 *        (`memcpy` relocation inside `folly::fbvector` with `1.5x` growth).
 *
 * Uses `std::array<char, 64>` rather than `libstdc++` `std::string`, because
 * `libstdc++` SSO strings store an internal self-pointer to `_M_local_buf`.
 */
struct SessionRelocatable {
  std::uint64_t session_id = 0;
  std::uint32_t account_id = 0;
  std::uint32_t flags = 0;
  std::uint32_t endpoint_len = 0;
  std::array<char, 64> endpoint_url{};
  folly::fbvector<std::byte> read_buffer;
  CompactRingDeque<std::uint64_t> pending_message_ids;
};

static_assert(std::is_nothrow_move_constructible_v<SessionRelocatable>,
              "SessionRelocatable must be nothrow move constructible");

/**
 * @brief Populates one client session object with non-SSO endpoint URL, 256B
 *        read buffer, and 4 pending message IDs.
 * @tparam SessionT One of the four `Session*` variants.
 * @param id Monotonic session slot identifier (`1..N`).
 * @return Constructed session object.
 */
template <typename SessionT>
[[nodiscard]] inline SessionT MakeClientSession(std::uint64_t id) {
  SessionT s;
  s.session_id = id;
  s.account_id = static_cast<std::uint32_t>((id * 31U) & 0xFFFFU);
  s.flags = 0x1U;
  constexpr std::string_view kPrefix =
      "https://gateway.service.internal/v2/sessions/slot-";
  const char tail_digit = static_cast<char>('0' + (id % 10U));
  if constexpr (std::is_same_v<SessionT, SessionRelocatable>) {
    std::memcpy(s.endpoint_url.data(), kPrefix.data(), kPrefix.size());
    s.endpoint_url[kPrefix.size()] = tail_digit;
    s.endpoint_url[kPrefix.size() + 1U] = '\0';
    s.endpoint_len = static_cast<std::uint32_t>(kPrefix.size() + 1U);
  } else {
    s.endpoint_url = absl::StrCat(kPrefix, std::string_view(&tail_digit, 1));
  }
  s.read_buffer.resize(256, static_cast<std::byte>(id & 0xFFU));
  for (std::uint64_t msg = 0; msg < 4; ++msg) {
    if constexpr (std::is_same_v<SessionT, SessionRuleOfZeroWithStdQueue>) {
      s.pending_message_ids.push(id * 16U + msg);
    } else {
      s.pending_message_ids.push_back(id * 16U + msg);
    }
  }
  return s;
}

/**
 * @brief Computes a full-field digest over a `ClientSession` so every member
 *        (`endpoint_url`, `read_buffer`, `pending_message_ids`, `account_id`,
 *        `flags`) is genuinely live and verified.
 * @tparam SessionT Session variant type.
 * @param s Populated session entry.
 * @return 64-bit session digest.
 */
template <typename SessionT>
[[nodiscard]] inline std::uint64_t DigestSession(const SessionT& s) noexcept {
  std::size_t url_len = 0;
  std::uint64_t url_tail = 0;
  if constexpr (std::is_same_v<SessionT, SessionRelocatable>) {
    url_len = s.endpoint_len;
    url_tail = static_cast<unsigned char>(s.endpoint_url[url_len - 1U]);
  } else {
    url_len = s.endpoint_url.size();
    url_tail = static_cast<unsigned char>(s.endpoint_url.back());
  }
  const std::uint64_t first_msg = s.pending_message_ids.front();
  const std::uint64_t buf_byte =
      std::to_integer<std::uint64_t>(s.read_buffer[0]);
  return s.session_id ^ (static_cast<std::uint64_t>(s.account_id) << 16) ^
         (static_cast<std::uint64_t>(s.flags) << 40) ^
         (static_cast<std::uint64_t>(url_len) << 32) ^ (url_tail << 8) ^
         (buf_byte << 24) ^ first_msg ^
         static_cast<std::uint64_t>(s.read_buffer.size() +
                                    s.pending_message_ids.size());
}

/**
 * @struct SessionGrowthMetrics
 * @brief Summary telemetry from registering `num_sessions` into a slot table.
 */
struct SessionGrowthMetrics {
  std::uint64_t checksum = 0;
  std::size_t final_size = 0;
  std::size_t final_capacity = 0;
};

/**
 * @brief Registers `num_sessions` reconnecting clients into a slot-indexed
 *        session vector (`sessions[slot_id]`) to measure end-to-end ingestion
 *        throughput, allocator churn, and post-growth capacity slack.
 * @tparam VectorType `std::vector<SessionT>` or `folly::fbvector<SessionT>`.
 * @tparam kPreReserve If true, invokes `.reserve(num_sessions)` upfront.
 * @param num_sessions Number of client sessions to register (`150,000`
 *        default).
 * @return Order-sensitive checksum, final size, and final capacity.
 */
template <typename VectorType, bool kPreReserve = false>
[[nodiscard]] inline SessionGrowthMetrics RunSessionTableGrowth(
    std::size_t num_sessions = 150'000) {
  using SessionT = typename VectorType::value_type;
  VectorType sessions;
  if constexpr (kPreReserve) {
    sessions.reserve(num_sessions);
  }
  for (std::size_t i = 0; i < num_sessions; ++i) {
    sessions.push_back(MakeClientSession<SessionT>(i + 1U));
  }
  std::uint64_t checksum = 0x5E5510ULL;
  for (const auto& s : sessions) {
    checksum = MixOrderHash(checksum, DigestSession(s));
  }
  return SessionGrowthMetrics{
      .checksum = checksum,
      .final_size = sessions.size(),
      .final_capacity = sessions.capacity(),
  };
}

/**
 * @brief Pointer-indirection C++ control: stores heap-allocated sessions inside
 *        `std::vector<std::unique_ptr<SessionNoexceptRingQueue>>` so vector
 *        growth shifts `8B` pointers without touching session objects (directly
 *        contrasting Java's `4B` Compressed OOP `ArrayList` + TLAB against C++
 *        `malloc` per-object overhead and inline value vectors).
 * @param num_sessions Number of client sessions to register (`150,000`
 * default).
 * @return Order-sensitive checksum, final size, and final capacity.
 */
[[nodiscard]] inline SessionGrowthMetrics RunSessionPointerVectorGrowth(
    std::size_t num_sessions = 150'000) {
  std::vector<std::unique_ptr<SessionNoexceptRingQueue>> sessions;
  for (std::size_t i = 0; i < num_sessions; ++i) {
    sessions.push_back(std::make_unique<SessionNoexceptRingQueue>(
        MakeClientSession<SessionNoexceptRingQueue>(i + 1U)));
  }
  std::uint64_t checksum = 0x5E5510ULL;
  for (const auto& s : sessions) {
    checksum = MixOrderHash(checksum, DigestSession(*s));
  }
  return SessionGrowthMetrics{
      .checksum = checksum,
      .final_size = sessions.size(),
      .final_capacity = sessions.capacity(),
  };
}

}  // namespace hotpath::episode17

namespace folly {
/**
 * @brief Declares `SessionRelocatable` safe for `memcpy` relocation inside
 *        `folly::fbvector` (all fields are bitwise relocatable).
 */
template <>
struct IsRelocatable<hotpath::episode17::SessionRelocatable> : std::true_type {
};
}  // namespace folly

#endif  // HOTPATH_COMMON_SEQUENCE_CONTAINERS_SEQUENCE_CONTAINERS_H_
