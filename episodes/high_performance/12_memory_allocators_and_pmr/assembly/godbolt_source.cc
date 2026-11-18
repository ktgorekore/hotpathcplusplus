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

struct alignas(8) OrderNode {
  std::uint64_t order_id;
  std::int64_t price_ticks;
  std::uint32_t quantity;
  std::uint32_t symbol_id;
  OrderNode* next;
};

struct BumpArenaView {
  std::byte* cur;
  std::byte* end;

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

struct SlabPoolView {
  union Slot {
    Slot* next_free;
    alignas(OrderNode) std::byte storage[sizeof(OrderNode)];
  };
  Slot* free_head;

  [[nodiscard]] OrderNode* PopSlot() noexcept {
    Slot* s = free_head;
    if (s == nullptr) [[unlikely]] {
      return nullptr;
    }
    free_head = s->next_free;
    return reinterpret_cast<OrderNode*>(s);
  }
};

// 1. Global heap new (emits `call _Znwm` + metadata/tcache work).
OrderNode* AllocStdNew(std::uint64_t id, std::int64_t px) {
  return new OrderNode{id, px, 100U, 1U, nullptr};
}

// 2. Monotonic BumpArena (compiles to ~4 instructions, 0 calls).
OrderNode* AllocBumpArena(BumpArenaView& arena, std::uint64_t id,
                          std::int64_t px) noexcept {
  void* mem = arena.Allocate32();
  if (mem == nullptr) [[unlikely]] {
    return nullptr;
  }
  return ::new (mem) OrderNode{id, px, 100U, 1U, nullptr};
}

// 3. Intrusive Zero-Header Slab Pool (compiles to 2 pointer loads, 0 calls).
OrderNode* AllocSlabPool(SlabPoolView& pool, std::uint64_t id,
                         std::int64_t px) noexcept {
  OrderNode* mem = pool.PopSlot();
  if (mem == nullptr) [[unlikely]] {
    return nullptr;
  }
  return ::new (mem) OrderNode{id, px, 100U, 1U, nullptr};
}

// 4. C++17 std::pmr::memory_resource (indirect vtable call into do_allocate).
void* AllocPmrResource(std::pmr::memory_resource* res) {
  return res->allocate(sizeof(OrderNode), alignof(OrderNode));
}

}  // namespace hotpath::godbolt_ep12
