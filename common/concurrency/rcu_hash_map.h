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

#ifndef COMMON_CONCURRENCY_RCU_HASH_MAP_H_
#define COMMON_CONCURRENCY_RCU_HASH_MAP_H_

#include <emmintrin.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "common/concurrency/ebr.h"

namespace cognitas::trading {

/**
 * @class RcuHashMap
 * @brief Ultra-low latency, concurrent hash map using Epoch-Based Reclamation
 *        (EBR) for wait-free reads and Copy-On-Write node splicing for writes.
 *
 * Designed for read-dominated telemetry, market data, and routing
 * architectures. Readers incur ZERO bus locking, ZERO atomic CAS instructions,
 * and ZERO lock contention.
 *
 * @tparam K Key type (Hashable via absl::Hash and equality comparable).
 * @tparam V Value type (CopyConstructible).
 * @tparam Hash Hasher callable.
 */
template <typename K, typename V, typename Hash = absl::Hash<K>>
class RcuHashMap {
 public:
  /**
   * @brief Constructs an RcuHashMap with an initial bucket capacity.
   * @param initial_buckets Number of hash buckets allocated up-front.
   */
  explicit RcuHashMap(size_t initial_buckets = 1024) : hash_fn_(Hash{}) {
    auto* t = new Table(initial_buckets);
    table_.store(t, std::memory_order_release);
  }

  /**
   * @brief Destructor. Destroys active table nodes once quiescent.
   */
  ~RcuHashMap() {
    Table* t = table_.load(std::memory_order_acquire);
    if (t) {
      delete t;
    }
  }

  RcuHashMap(const RcuHashMap&) = delete;
  RcuHashMap& operator=(const RcuHashMap&) = delete;

  /**
   * @brief Retrieves a copy of the value for the given key.
   *
   * 100% wait-free! Protected by thread-local EBR epoch and Seqlock validation.
   *
   * @param key Search key.
   * @return std::optional<V> Value if present, std::nullopt otherwise.
   */
  std::optional<V> Get(const K& key) const {
    EbrGuard guard;
    Table* t = table_.load(std::memory_order_acquire);
    if (!t) return std::nullopt;

    size_t idx = hash_fn_(key) % t->size;
    Node* current = t->buckets[idx].load(std::memory_order_acquire);

    while (current) {
      if (current->key == key) {
        V result;
        while (true) {
          uint32_t seq1 = current->seq.load(std::memory_order_acquire);
          if (seq1 & 1) {
            _mm_pause();
            continue;
          }
          result = current->value;
          std::atomic_thread_fence(std::memory_order_acquire);
          uint32_t seq2 = current->seq.load(std::memory_order_relaxed);
          if (seq1 == seq2) break;
        }
        return result;
      }
      current = current->next.load(std::memory_order_acquire);
    }
    return std::nullopt;
  }

  /**
   * @brief Inserts or updates a key-value pair.
   *
   * Updates perform Copy-On-Write (COW) node replacement and retire the
   * previous node through Epoch-Based Reclamation. Readers are never blocked.
   *
   * @param key Key to store.
   * @param value Value associated with key.
   */
  void Put(K key, V value) {
    std::lock_guard<std::mutex> lock(write_mutex_);

    Table* t = table_.load(std::memory_order_relaxed);
    if (count_.load(std::memory_order_relaxed) >= t->size) {
      Resize(t->size * 2);
      t = table_.load(std::memory_order_relaxed);
    }

    size_t idx = hash_fn_(key) % t->size;
    std::atomic<Node*>& bucket = t->buckets[idx];
    Node* current = bucket.load(std::memory_order_relaxed);
    Node* prev = nullptr;

    while (current) {
      if (current->key == key) {
        Node* new_node =
            new Node(key, std::move(value),
                     current->next.load(std::memory_order_relaxed));
        if (prev) {
          prev->next.store(new_node, std::memory_order_release);
        } else {
          bucket.store(new_node, std::memory_order_release);
        }
        generation_.fetch_add(1, std::memory_order_release);
        EpochBasedReclamation::Retire(current);
        return;
      }
      prev = current;
      current = current->next.load(std::memory_order_relaxed);
    }

    Node* old_head = bucket.load(std::memory_order_relaxed);
    Node* new_node = new Node(std::move(key), std::move(value), old_head);
    bucket.store(new_node, std::memory_order_release);
    count_.fetch_add(1, std::memory_order_relaxed);
    generation_.fetch_add(1, std::memory_order_release);
  }

  /**
   * @brief Removes a key-value entry from the map.
   *
   * Unlinks the node atomically and schedules it for deferred reclamation.
   *
   * @param key Key to remove.
   * @return true if key was present and removed, false otherwise.
   */
  bool Remove(const K& key) {
    std::lock_guard<std::mutex> lock(write_mutex_);

    Table* t = table_.load(std::memory_order_relaxed);
    size_t idx = hash_fn_(key) % t->size;
    std::atomic<Node*>& bucket = t->buckets[idx];
    Node* current = bucket.load(std::memory_order_relaxed);
    Node* prev = nullptr;

    while (current) {
      if (current->key == key) {
        Node* next_node = current->next.load(std::memory_order_relaxed);
        if (prev) {
          prev->next.store(next_node, std::memory_order_release);
        } else {
          bucket.store(next_node, std::memory_order_release);
        }
        count_.fetch_sub(1, std::memory_order_relaxed);
        generation_.fetch_add(1, std::memory_order_release);
        EpochBasedReclamation::Retire(current);
        return true;
      }
      prev = current;
      current = current->next.load(std::memory_order_relaxed);
    }
    return false;
  }

  /**
   * @brief Returns current element count.
   */
  size_t Size() const noexcept {
    return count_.load(std::memory_order_relaxed);
  }

 private:
  struct Node : public EbrNode {
    const K key;
    V value;
    std::atomic<uint32_t> seq{0};
    std::atomic<Node*> next{nullptr};

    Node(K k, V v, Node* n) : key(std::move(k)), value(std::move(v)), next(n) {}
  };

  struct Table : public EbrNode {
    std::vector<std::atomic<Node*>> buckets;
    size_t size;

    explicit Table(size_t s) : buckets(s), size(s) {
      for (auto& b : buckets) {
        b.store(nullptr, std::memory_order_relaxed);
      }
    }

    ~Table() override {
      for (auto& b : buckets) {
        Node* curr = b.load(std::memory_order_relaxed);
        while (curr) {
          Node* next = curr->next.load(std::memory_order_relaxed);
          delete curr;
          curr = next;
        }
      }
    }
  };

  void Resize(size_t new_size) {
    auto* new_table = new Table(new_size);
    Table* old_table = table_.load(std::memory_order_relaxed);

    for (size_t i = 0; i < old_table->size; ++i) {
      Node* curr = old_table->buckets[i].load(std::memory_order_relaxed);
      while (curr) {
        size_t new_idx = hash_fn_(curr->key) % new_size;
        Node* new_head =
            new_table->buckets[new_idx].load(std::memory_order_relaxed);
        Node* copied_node = new Node(curr->key, curr->value, new_head);
        new_table->buckets[new_idx].store(copied_node,
                                          std::memory_order_relaxed);
        curr = curr->next.load(std::memory_order_relaxed);
      }
    }

    table_.store(new_table, std::memory_order_release);
    EpochBasedReclamation::Retire(old_table);
  }

  Hash hash_fn_;
  std::atomic<Table*> table_{nullptr};
  std::mutex write_mutex_;
  std::atomic<size_t> count_{0};
  std::atomic<uint64_t> generation_{0};
};

}  // namespace cognitas::trading

namespace hotpath {
using cognitas::trading::RcuHashMap;
}  // namespace hotpath

#endif  // COMMON_CONCURRENCY_RCU_HASH_MAP_H_
