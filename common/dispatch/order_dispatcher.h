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

#ifndef HOTPATH_COMMON_DISPATCH_ORDER_DISPATCHER_H_
#define HOTPATH_COMMON_DISPATCH_ORDER_DISPATCHER_H_

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <variant>

namespace hotpath::dispatch {

// ============================================================================
// Approach 1: Classical Object-Oriented Programming (Dynamic Polymorphism)
// ============================================================================

/**
 * @brief Abstract base order defining the classical polymorphic interface.
 *
 * Each instance incurs an 8-byte vptr pointing to the class virtual method
 * table (vtable). In array workloads, elements are stored via heap pointers
 * (std::unique_ptr<BaseOrder>), incurring a cache-line dereference penalty and
 * dynamic indirect branch resolution.
 */
class BaseOrder {
 public:
  /**
   * @brief Virtual destructor for safe polymorphic deletion.
   */
  virtual ~BaseOrder() = default;

  /**
   * @brief Executes the order matching or pricing logic.
   * @param current_price The prevailing market mid-price.
   * @return The executed cash-flow volume or synthetic fill price.
   */
  virtual double Execute(double current_price) const = 0;
};

/**
 * @brief Concrete Buy Limit order implementing dynamic virtual dispatch.
 */
class BuyLimitOrder final : public BaseOrder {
 public:
  /**
   * @brief Constructs a BuyLimitOrder.
   * @param id Unique order identifier.
   * @param limit_price Maximum execution limit price.
   * @param quantity Order share quantity.
   */
  BuyLimitOrder(uint64_t id, double limit_price, uint32_t quantity)
      : id_(id), limit_price_(limit_price), quantity_(quantity) {}

  /**
   * @brief Executes the buy limit order if market price is at or below limit.
   * @param current_price The prevailing market mid-price.
   * @return Executed notional value if filled, or 0.0 otherwise.
   */
  double Execute(double current_price) const override {
    if (current_price <= limit_price_) {
      return limit_price_ * quantity_;
    }
    return 0.0;
  }

  /**
   * @brief Returns the unique order identifier.
   * @return Order ID.
   */
  uint64_t id() const noexcept { return id_; }

  /**
   * @brief Returns the buy limit price.
   * @return Maximum limit price.
   */
  double limit_price() const noexcept { return limit_price_; }

  /**
   * @brief Returns the order share quantity.
   * @return Share quantity.
   */
  uint32_t quantity() const noexcept { return quantity_; }

 private:
  uint64_t id_;
  double limit_price_;
  uint32_t quantity_;
};

/**
 * @brief Concrete Sell Market order implementing dynamic virtual dispatch.
 */
class SellMarketOrder final : public BaseOrder {
 public:
  /**
   * @brief Constructs a SellMarketOrder.
   * @param id Unique order identifier.
   * @param quantity Order share quantity.
   */
  SellMarketOrder(uint64_t id, uint32_t quantity)
      : id_(id), quantity_(quantity) {}

  /**
   * @brief Executes the sell market order at the prevailing mid-price.
   * @param current_price The prevailing market mid-price.
   * @return Executed cash-flow proceeds (`current_price * quantity`).
   */
  double Execute(double current_price) const override {
    return current_price * quantity_;
  }

  /**
   * @brief Returns the unique order identifier.
   * @return Order ID.
   */
  uint64_t id() const noexcept { return id_; }

  /**
   * @brief Returns the order share quantity.
   * @return Share quantity.
   */
  uint32_t quantity() const noexcept { return quantity_; }

 private:
  uint64_t id_;
  uint32_t quantity_;
};

/**
 * @brief Concrete Cancel order implementing dynamic virtual dispatch.
 */
class CancelOrder final : public BaseOrder {
 public:
  /**
   * @brief Constructs a CancelOrder for the target order ID.
   * @param id Identifier of the order to cancel.
   */
  explicit CancelOrder(uint64_t id) : id_(id) {}

  /**
   * @brief Processes a cancel request (zero notional cash flow).
   * @param current_price Prevailing market mid-price (unused).
   * @return Always 0.0.
   */
  double Execute([[maybe_unused]] double current_price) const override {
    return 0.0;
  }

  /**
   * @brief Returns the canceled order identifier.
   * @return Target order ID.
   */
  uint64_t id() const noexcept { return id_; }

 private:
  uint64_t id_;
};

// ============================================================================
// Approach 2: Modern Data-Oriented Design (std::variant & std::visit)
// ============================================================================

/**
 * @brief Plain Old Data (POD) representation of a Buy Limit order.
 *
 * Zero vptr overhead. Stored inline within the variant buffer, aligned
 * contiguously inside standard sequential memory cache lines.
 */
struct BuyLimit {
  uint64_t id;
  double limit_price;
  uint32_t quantity;

  /**
   * @brief Executes the buy limit order if market price is at or below limit.
   * @param current_price The prevailing market mid-price.
   * @return Executed notional value if filled, or 0.0 otherwise.
   */
  double Execute(double current_price) const noexcept {
    if (current_price <= limit_price) {
      return limit_price * quantity;
    }
    return 0.0;
  }
};

/**
 * @brief POD representation of a Sell Market order.
 */
struct SellMarket {
  uint64_t id;
  uint32_t quantity;

  /**
   * @brief Executes the sell market order at the prevailing mid-price.
   * @param current_price The prevailing market mid-price.
   * @return Executed cash-flow proceeds (`current_price * quantity`).
   */
  double Execute(double current_price) const noexcept {
    return current_price * quantity;
  }
};

/**
 * @brief POD representation of a Cancel request.
 */
struct Cancel {
  uint64_t id;

  /**
   * @brief Processes a cancel request (zero notional cash flow).
   * @param current_price Prevailing market mid-price (unused).
   * @return Always 0.0.
   */
  double Execute([[maybe_unused]] double current_price) const noexcept {
    return 0.0;
  }
};

/**
 * @brief Discriminated union type representing any valid order in contiguous
 * memory.
 */
using OrderVariant = std::variant<BuyLimit, SellMarket, Cancel>;

/**
 * @brief Generic overloaded callable helper for std::visit pattern matching.
 * @tparam Ts Callable types combined via variadic inheritance.
 */
template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

/**
 * @brief Inlined execution helper evaluating an OrderVariant via std::visit.
 * @param order The variant order to execute.
 * @param current_price The prevailing market mid-price.
 * @return The cash-flow execution result.
 */
inline double ExecuteVariant(const OrderVariant& order,
                             double current_price) noexcept {
  return std::visit(
      [current_price](const auto& item) noexcept {
        return item.Execute(current_price);
      },
      order);
}

/**
 * @brief Alias for ExecuteVariant providing explicit dispatcher naming.
 * @param order The variant order instance.
 * @param current_price The prevailing market mid-price.
 * @return The executed cash-flow volume or fill price.
 */
inline double DispatchOrder(const OrderVariant& order,
                            double current_price) noexcept {
  return ExecuteVariant(order, current_price);
}

}  // namespace hotpath::dispatch

#endif  // HOTPATH_COMMON_DISPATCH_ORDER_DISPATCHER_H_
