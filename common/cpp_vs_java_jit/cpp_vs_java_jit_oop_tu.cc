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
 * @file cpp_vs_java_jit_oop_tu.cc
 * @brief Separate translation unit defining concrete `Event` subclasses
 * (`LimitOrderEvent`, `CancelOrderEvent`, `ModifyOrderEvent`,
 * `PeggedOrderEvent`, `MarketHaltEvent`) and cross-TU factories
 * (`MakeEventByType`, `ConstructEventInPlace`).
 */

#include <cstdint>
#include <memory>
#include <new>

#include "common/cpp_vs_java_jit/cpp_vs_java_jit.h"

namespace hotpath::cpp_vs_java_jit {
namespace {

/**
 * @brief Limit Order event subclass (`type_tag == 0`, 99.9% of monomorphic
 * traffic).
 */
struct LimitOrderEvent final : Event {
  using Event::Event;

  [[nodiscard]] std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    const std::int64_t gross = price * qty;
    return std::make_unique<OrderResult>(OrderResult{gross, gross >> 10});
  }

  [[nodiscard]] std::int64_t ProcessNoAlloc() const noexcept override {
    const std::int64_t gross = price * qty;
    return gross - (gross >> 10);
  }
};

/**
 * @brief Cancel Order event subclass (`type_tag == 1`).
 */
struct CancelOrderEvent final : Event {
  using Event::Event;

  [[nodiscard]] std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    const std::int64_t gross = -(price * qty);
    return std::make_unique<OrderResult>(OrderResult{gross, 0});
  }

  [[nodiscard]] std::int64_t ProcessNoAlloc() const noexcept override {
    return -(price * qty);
  }
};

/**
 * @brief Modify Order event subclass (`type_tag == 2`).
 */
struct ModifyOrderEvent final : Event {
  using Event::Event;

  [[nodiscard]] std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    const std::int64_t gross = price * (qty + 1);
    return std::make_unique<OrderResult>(OrderResult{gross, gross >> 9});
  }

  [[nodiscard]] std::int64_t ProcessNoAlloc() const noexcept override {
    return (price * (qty + 1)) - ((price * qty) >> 9);
  }
};

/**
 * @brief Pegged Order event subclass (`type_tag == 3`).
 */
struct PeggedOrderEvent final : Event {
  using Event::Event;

  [[nodiscard]] std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    const std::int64_t gross = (price + 2) * qty;
    return std::make_unique<OrderResult>(OrderResult{gross, 1});
  }

  [[nodiscard]] std::int64_t ProcessNoAlloc() const noexcept override {
    return (price + 2) * qty;
  }
};

/**
 * @brief Market Halt emergency circuit-breaker event subclass (`type_tag ==
 * 4`).
 */
struct MarketHaltEvent final : Event {
  using Event::Event;

  [[nodiscard]] std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    const std::int64_t gross =
        (price * qty * 3) ^ static_cast<std::int64_t>(symbol_id);
    return std::make_unique<OrderResult>(OrderResult{gross, 0});
  }

  [[nodiscard]] std::int64_t ProcessNoAlloc() const noexcept override {
    return (price * qty * 3) ^ static_cast<std::int64_t>(symbol_id);
  }
};

}  // namespace

std::unique_ptr<Event> MakeEventByType(std::uint8_t type_tag,
                                       std::int64_t price, std::int64_t qty,
                                       std::uint32_t symbol_id) {
  switch (type_tag) {
    case 0:
      return std::make_unique<LimitOrderEvent>(price, qty, symbol_id, type_tag);
    case 1:
      return std::make_unique<CancelOrderEvent>(price, qty, symbol_id,
                                                type_tag);
    case 2:
      return std::make_unique<ModifyOrderEvent>(price, qty, symbol_id,
                                                type_tag);
    case 3:
      return std::make_unique<PeggedOrderEvent>(price, qty, symbol_id,
                                                type_tag);
    default:
      return std::make_unique<MarketHaltEvent>(price, qty, symbol_id, type_tag);
  }
}

void ConstructEventInPlace(void* arena_slot, std::uint8_t type_tag,
                           std::int64_t price, std::int64_t qty,
                           std::uint32_t symbol_id) {
  switch (type_tag) {
    case 0:
      new (arena_slot) LimitOrderEvent(price, qty, symbol_id, type_tag);
      break;
    case 1:
      new (arena_slot) CancelOrderEvent(price, qty, symbol_id, type_tag);
      break;
    case 2:
      new (arena_slot) ModifyOrderEvent(price, qty, symbol_id, type_tag);
      break;
    case 3:
      new (arena_slot) PeggedOrderEvent(price, qty, symbol_id, type_tag);
      break;
    default:
      new (arena_slot) MarketHaltEvent(price, qty, symbol_id, type_tag);
      break;
  }
}

}  // namespace hotpath::cpp_vs_java_jit
