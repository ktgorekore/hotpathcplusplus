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
 * @file live_uaf_repro.cc
 * @brief Episode 14 Live Session 1: Temporal Use-After-Free & Reference
 * Invalidation Reproducer.
 *
 * 1. Compiles cleanly with ZERO warnings under:
 *      clang++ -std=c++20 -O3 -Wall -Wextra -Wdangling live_uaf_repro.cc
 *    and exits with status 0 while silently reading an `AuditEntry`'s
 *    `sequence_number` (`999,999,900` ticks) as `best_bid->price_ticks`!
 * 2. Caught immediately at runtime via 1-to-8 shadow memory under:
 *      clang++ -std=c++20 -O1 -g -fsanitize=address live_uaf_repro.cc
 * 3. Demonstrates local GSL dangling-view detection when built with:
 *      clang++ -std=c++20 -O3 -Wall -Wextra -Wdangling -DDEMO_DANGLING_VIEW \
 *        live_uaf_repro.cc
 * 4. Demonstrates O(1) temporal safety via 64-bit generational slab handles
 *    when executed with `--safe-slab`.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/memory_safety/memory_safety.h"

namespace hotpath::memory_safety {

#if defined(DEMO_DANGLING_VIEW)
/**
 * @brief Local lifetime bug that `clang++ -Wdangling` DOES catch at compile
 * time (`-Wdangling-gsl`): binding a non-owning `std::string_view` to a
 * temporary `std::string` destroyed at the end of the full-expression.
 * @return Dangling `std::string_view`.
 */
std::string_view MakeDanglingOrderSymbolView() {
  std::string_view sv = std::string("AAPL_TEMP_ORDER_TAG");
  return sv;
}
#endif

/**
 * @brief Demonstrates how holding `const Order* best_bid = &book[0]` across
 * `std::vector<Order>::push_back()` escapes `-Wall -Wextra -Wdangling` and
 * causes deterministic same-size-class (64-byte tcache bin) heap aliasing with
 * `std::vector<AuditEntry>`.
 */
[[gnu::noinline]] void RunUnsafeVectorReallocationRepro() {
  std::printf(
      "======================================================================\n"
      "LIVE SESSION 1A: C++20 TEMPORAL UAF & SAME-SIZE-CLASS TCACHE REUSE\n"
      "======================================================================"
      "\n");

  std::vector<Order> book;
  book.reserve(2);  // Allocates 2 * 32B = 64-byte heap chunk.

  book.push_back(Order{
      .order_id = 1001ULL,
      .price_ticks = 105'250LL,  // $1,052.50 resting limit bid
      .quantity = 50U,
      .symbol_id = 7U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });
  book.push_back(Order{
      .order_id = 1002ULL,
      .price_ticks = 105'200LL,
      .quantity = 25U,
      .symbol_id = 7U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });

  // Risk/matching caller caches a raw pointer to the top-of-book order.
  const Order* best_bid = &book[0];
  std::printf(
      "[1] Held pointer best_bid      = %p\n"
      "    best_bid->order_id         = %llu\n"
      "    best_bid->price_ticks      = %lld ($%.2f)\n",
      static_cast<const void*>(best_bid),
      static_cast<unsigned long long>(best_bid->order_id),
      static_cast<long long>(best_bid->price_ticks),
      static_cast<double>(best_bid->price_ticks) / 100.0);

  // Market volatility burst appends a 3rd order -> book reallocates from
  // capacity 2 (64B) to capacity 4 (128B) and frees the old 64-byte chunk.
  book.push_back(Order{
      .order_id = 1003ULL,
      .price_ticks = 105'150LL,
      .quantity = 100U,
      .symbol_id = 7U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });

  // Immediately afterward, the audit logger allocates 2 * 32B = 64 bytes,
  // reclaiming the exact 64-byte tcache block that `best_bid` still points to!
  std::vector<AuditEntry> audit_log;
  audit_log.reserve(2);
  audit_log.push_back(AuditEntry{
      .event_tag = 0xDEAD'BEEF'CAFE'BABEULL,
      .sequence_number = 999'999'900LL,  // Overwrites Order::price_ticks (+8)!
      .subsystem_id = 999'999U,          // Overwrites Order::quantity (+16)!
      .severity_code = 777U,
      .status_bits = 0xBAD0'C0DEU,
      .checksum = 0x1337U,
  });

  std::printf("[2] Newbook[0] address         = %p (reallocated 128B block)\n",
              static_cast<const void*>(&book[0]));
  std::printf(
      "[3] AuditEntry[0] address      = %p (reclaimed 64B tcache bin)\n",
      static_cast<const void*>(&audit_log[0]));

  // Temporal Use-After-Free read: under -O3 this silently reads AuditEntry[0];
  // under -fsanitize=address this triggers an immediate shadow-memory abort!
  const std::int64_t read_price = best_bid->price_ticks;
  const std::uint32_t read_qty = best_bid->quantity;
  const std::uint64_t read_id = best_bid->order_id;

  std::printf(
      "\n[4] DEREFERENCING DANGLING best_bid (%p):\n"
      "    read order_id              = 0x%llx (corrupted by event_tag!)\n"
      "    read price_ticks           = %lld ($%.2f -- CORRUPTED BY "
      "sequence_number!)\n"
      "    read quantity              = %u (corrupted by subsystem_id!)\n",
      static_cast<const void*>(best_bid),
      static_cast<unsigned long long>(read_id),
      static_cast<long long>(read_price),
      static_cast<double>(read_price) / 100.0, read_qty);
}

/**
 * @brief Demonstrates how `OrderSlabPool` + 64-bit `OrderHandle` prevents
 * temporal Use-After-Free and ABA slot aliasing in O(1).
 */
[[gnu::noinline]] void RunSafeGenerationalSlabDemo() {
  std::printf(
      "\n======================================================================"
      "\n"
      "LIVE SESSION 1B: SAFE 64-BIT GENERATIONAL SLAB HANDLE (OrderSlabPool)\n"
      "======================================================================"
      "\n");
  OrderSlabPool pool(1024);

  const std::optional<OrderHandle> h_bid = pool.Allocate(Order{
      .order_id = 1001ULL,
      .price_ticks = 105'250LL,
      .quantity = 50U,
      .symbol_id = 7U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });

  std::printf("[1] Allocated OrderHandle      = {index=%u, generation=%u}\n",
              h_bid->index, h_bid->generation);

  // Cancel order 1001 and immediately recycle slot 0 with a new order.
  pool.Release(*h_bid);
  const std::optional<OrderHandle> h_replacement = pool.Allocate(Order{
      .order_id = 2002ULL,
      .price_ticks = 999'999'900LL,
      .quantity = 999'999U,
      .symbol_id = 7U,
      .flags = kOrderFlagActive,
      .reserved_pad = 0U,
  });

  std::printf("[2] Recycled Slot 0 Handle     = {index=%u, generation=%u}\n",
              h_replacement->index, h_replacement->generation);

  const Order* stale_ptr = pool.Get(*h_bid);
  const Order* live_ptr = pool.Get(*h_replacement);

  std::printf(
      "[3] Lookup stale {idx=%u, gen=%u} = %s (ABA corruption prevented!)\n",
      h_bid->index, h_bid->generation,
      (stale_ptr == nullptr) ? "nullptr [SAFE REJECT]" : "UNEXPECTED");
  if (live_ptr != nullptr) {
    std::printf(
        "[4] Lookup live  {idx=%u, gen=%u} = order_id=%llu, price_ticks=%lld\n",
        h_replacement->index, h_replacement->generation,
        static_cast<unsigned long long>(live_ptr->order_id),
        static_cast<long long>(live_ptr->price_ticks));
  }
}

}  // namespace hotpath::memory_safety

int main(int argc, char** argv) {
  bool safe_only = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--safe-slab") == 0) {
      safe_only = true;
    }
  }

  if (safe_only) {
    hotpath::memory_safety::RunSafeGenerationalSlabDemo();
    return 0;
  }

  hotpath::memory_safety::RunUnsafeVectorReallocationRepro();
  hotpath::memory_safety::RunSafeGenerationalSlabDemo();
  return 0;
}
