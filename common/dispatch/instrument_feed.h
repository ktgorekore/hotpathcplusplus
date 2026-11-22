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

#ifndef HOTPATH_COMMON_DISPATCH_INSTRUMENT_FEED_H_
#define HOTPATH_COMMON_DISPATCH_INSTRUMENT_FEED_H_

#include <cstdint>
#include <string_view>
#include <variant>

namespace hotpath::dispatch {

// ============================================================================
// Approach 1: Classical OOP Polymorphism (Instrument Hierarchy)
// ============================================================================

/**
 * @brief Base instrument class defining the classical virtual interface.
 */
class Instrument {
 public:
  /**
   * @brief Virtual destructor for safe polymorphic deletion.
   */
  virtual ~Instrument() = default;

  /**
   * @brief Processes the current tick or price update for this financial
   * instrument.
   * @return The computed valuation or volume metric.
   */
  virtual double Process() const = 0;
};

/**
 * @brief Concrete stock instrument with price and volume.
 */
class Stock final : public Instrument {
 public:
  /**
   * @brief Constructs a Stock instrument.
   * @param id Unique instrument identifier.
   * @param price Current share price.
   * @param volume Traded share volume.
   */
  Stock(uint64_t id, double price, uint32_t volume)
      : id_(id), price_(price), volume_(volume) {}

  /**
   * @brief Computes notional stock volume (`price * volume`).
   * @return Notional traded dollar volume.
   */
  double Process() const override { return price_ * volume_; }

  /**
   * @brief Returns the stock identifier.
   * @return Unique 64-bit instrument ID.
   */
  uint64_t id() const noexcept { return id_; }

  /**
   * @brief Returns the current share price.
   * @return Share price.
   */
  double price() const noexcept { return price_; }

  /**
   * @brief Returns the traded share volume.
   * @return Traded share count.
   */
  uint32_t volume() const noexcept { return volume_; }

 private:
  uint64_t id_;
  double price_;
  uint32_t volume_;
};

/**
 * @brief Concrete bond instrument with face value and coupon yield.
 */
class Bond final : public Instrument {
 public:
  /**
   * @brief Constructs a Bond instrument.
   * @param id Unique instrument identifier.
   * @param face_value Par face value of the bond.
   * @param coupon_rate Fractional coupon yield rate.
   */
  Bond(uint64_t id, double face_value, double coupon_rate)
      : id_(id), face_value_(face_value), coupon_rate_(coupon_rate) {}

  /**
   * @brief Computes coupon-adjusted bond payout (`face_value * (1 + rate)`).
   * @return Coupon-adjusted bond valuation.
   */
  double Process() const override { return face_value_ * (1.0 + coupon_rate_); }

  /**
   * @brief Returns the bond identifier.
   * @return Unique 64-bit instrument ID.
   */
  uint64_t id() const noexcept { return id_; }

  /**
   * @brief Returns the bond par face value.
   * @return Bond face value.
   */
  double face_value() const noexcept { return face_value_; }

  /**
   * @brief Returns the bond coupon rate.
   * @return Fractional coupon yield rate.
   */
  double coupon_rate() const noexcept { return coupon_rate_; }

 private:
  uint64_t id_;
  double face_value_;
  double coupon_rate_;
};

/**
 * @brief Concrete derivative option instrument.
 */
class Option final : public Instrument {
 public:
  /**
   * @brief Constructs an Option derivative instrument.
   * @param id Unique instrument identifier.
   * @param strike Option strike price.
   * @param premium Option contract premium.
   * @param is_call True for call options, false for put options.
   */
  Option(uint64_t id, double strike, double premium, bool is_call)
      : id_(id), strike_(strike), premium_(premium), is_call_(is_call) {}

  /**
   * @brief Computes combined strike plus premium reference level.
   * @return Sum of strike price and option premium.
   */
  double Process() const override { return strike_ + premium_; }

  /**
   * @brief Returns the option identifier.
   * @return Unique 64-bit instrument ID.
   */
  uint64_t id() const noexcept { return id_; }

  /**
   * @brief Returns the option strike price.
   * @return Strike price.
   */
  double strike() const noexcept { return strike_; }

  /**
   * @brief Returns the option contract premium.
   * @return Contract premium.
   */
  double premium() const noexcept { return premium_; }

  /**
   * @brief Reports whether this option is a call contract.
   * @return True if call option, false if put option.
   */
  bool is_call() const noexcept { return is_call_; }

 private:
  uint64_t id_;
  double strike_;
  double premium_;
  bool is_call_;
};

// ============================================================================
// Approach 2: Modern Data-Oriented Design (Flat std::variant)
// ============================================================================

/**
 * @brief Plain Old Data (POD) representation of a stock tick for contiguous
 *        `std::variant` storage.
 */
struct StockPOD {
  uint64_t id;
  double price;
  uint32_t volume;

  /**
   * @brief Computes notional stock volume (`price * volume`).
   * @return Notional traded dollar volume.
   */
  double Process() const noexcept { return price * volume; }
};

/**
 * @brief Plain Old Data (POD) representation of a bond tick for contiguous
 *        `std::variant` storage.
 */
struct BondPOD {
  uint64_t id;
  double face_value;
  double coupon_rate;

  /**
   * @brief Computes coupon-adjusted bond payout (`face_value * (1 + rate)`).
   * @return Coupon-adjusted bond valuation.
   */
  double Process() const noexcept { return face_value * (1.0 + coupon_rate); }
};

/**
 * @brief Plain Old Data (POD) representation of an option tick for contiguous
 *        `std::variant` storage.
 */
struct OptionPOD {
  uint64_t id;
  double strike;
  double premium;
  bool is_call;

  /**
   * @brief Computes combined strike plus premium reference level.
   * @return Sum of strike price and option premium.
   */
  double Process() const noexcept { return strike + premium; }
};

/**
 * @brief Discriminated union storing any instrument tick inline without heap
 *        indirection.
 */
using InstrumentVariant = std::variant<StockPOD, BondPOD, OptionPOD>;

/**
 * @brief Processes an InstrumentVariant using std::visit.
 * @param instrument The instrument variant to process.
 * @return The computed valuation metric.
 */
inline double ProcessInstrument(const InstrumentVariant& instrument) noexcept {
  return std::visit([](const auto& item) noexcept { return item.Process(); },
                    instrument);
}

}  // namespace hotpath::dispatch

#endif  // HOTPATH_COMMON_DISPATCH_INSTRUMENT_FEED_H_
