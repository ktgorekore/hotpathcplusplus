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
 * @file godbolt_source.cc
 * @brief Minimal self-contained Compiler Explorer (Godbolt) comparison file
 * contrasting x86-64 code generation under GCC 13/14 `-std=c++20 -O3` for:
 *   1. Standard heap `new OrderNode` (`call _Znwm@PLT`),
 *   2. Monotonic `BumpArena::Allocate()` (inline pointer add + bounds guard),
 *   3. Zero-header intrusive `FixedSlabPool::Allocate()` (2-load freelist pop),
 *   4. C++17 `std::pmr::memory_resource::allocate()` (indirect vtable call).
 */

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>

namespace hotpath::godbolt_ep12 {

/**
 * @struct OrderNode
 * @brief 32-byte linked order node used for allocator codegen comparisons.
 */
struct alignas(8) OrderNode {
  std::uint64_t order_id;
  std::int64_t price_ticks;
  std::uint32_t quantity;
  std::uint32_t symbol_id;
  OrderNode* next;
};

/**
 * @struct BumpArenaView
 * @brief Minimal pointer-bump allocator view for Godbolt disassembly.
 */
struct BumpArenaView {
  std::byte* cur;
  std::byte* end;

  /**
   * @brief Allocates a 32-byte `OrderNode` slot via pointer addition.
   * @return Pointer to allocated slot, or `nullptr` if exhausted.
   */
  [[nodiscard]] void* Allocate32() noexcept {
    std::byte* p = cur;
    std::byte* next = p + sizeof(OrderNode);
    if (next > end) [[unlikely]] {
      return nullptr;
    }
    cur = next;
    return p;
  }
};

/**
 * @struct SlabPoolView
 * @brief Minimal intrusive freelist slab view for Godbolt disassembly.
 */
struct SlabPoolView {
  /** @brief Intrusive storage union overlaying freelist link and payload. */
  union Slot {
    Slot* next_free;
    alignas(OrderNode) std::byte storage[sizeof(OrderNode)];
  };
  Slot* free_head;

  /**
   * @brief Pops one pre-allocated slot from the intrusive freelist head.
   * @return Pointer to `OrderNode` storage, or `nullptr` if empty.
   */
  [[nodiscard]] OrderNode* PopSlot() noexcept {
    Slot* s = free_head;
    if (s == nullptr) [[unlikely]] {
      return nullptr;
    }
    free_head = s->next_free;
    return reinterpret_cast<OrderNode*>(s);
  }
};

/**
 * @brief Allocates an `OrderNode` via global `operator new` (`call _Znwm`).
 * @param id Order identifier.
 * @param px Limit price in ticks.
 * @return Heap-allocated `OrderNode*`.
 */
OrderNode* AllocStdNew(std::uint64_t id, std::int64_t px) {
  return new OrderNode{id, px, 100U, 1U, nullptr};
}

/**
 * @brief Allocates and initializes an `OrderNode` from a monotonic bump arena.
 * @param arena Backing `BumpArenaView`.
 * @param id Order identifier.
 * @param px Limit price in ticks.
 * @return Pointer to constructed `OrderNode`, or `nullptr` if exhausted.
 */
OrderNode* AllocBumpArena(BumpArenaView& arena, std::uint64_t id,
                          std::int64_t px) noexcept {
  void* mem = arena.Allocate32();
  if (mem == nullptr) [[unlikely]] {
    return nullptr;
  }
  return ::new (mem) OrderNode{id, px, 100U, 1U, nullptr};
}

/**
 * @brief Allocates and initializes an `OrderNode` from a zero-header slab pool.
 * @param pool Backing `SlabPoolView`.
 * @param id Order identifier.
 * @param px Limit price in ticks.
 * @return Pointer to constructed `OrderNode`, or `nullptr` if empty.
 */
OrderNode* AllocSlabPool(SlabPoolView& pool, std::uint64_t id,
                         std::int64_t px) noexcept {
  OrderNode* mem = pool.PopSlot();
  if (mem == nullptr) [[unlikely]] {
    return nullptr;
  }
  return ::new (mem) OrderNode{id, px, 100U, 1U, nullptr};
}

/**
 * @brief Allocates `sizeof(OrderNode)` via C++17 `std::pmr::memory_resource`.
 * @param res Polymorphic memory resource pointer.
 * @return Pointer to raw allocated storage.
 */
void* AllocPmrResource(std::pmr::memory_resource* res) {
  return res->allocate(sizeof(OrderNode), alignof(OrderNode));
}

}  // namespace hotpath::godbolt_ep12
