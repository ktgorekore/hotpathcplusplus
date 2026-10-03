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
  Stock(uint64_t id, double price, uint32_t volume)
      : id_(id), price_(price), volume_(volume) {}

  double Process() const override { return price_ * volume_; }

  uint64_t id() const noexcept { return id_; }
  double price() const noexcept { return price_; }
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
  Bond(uint64_t id, double face_value, double coupon_rate)
      : id_(id), face_value_(face_value), coupon_rate_(coupon_rate) {}

  double Process() const override { return face_value_ * (1.0 + coupon_rate_); }

  uint64_t id() const noexcept { return id_; }
  double face_value() const noexcept { return face_value_; }
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
  Option(uint64_t id, double strike, double premium, bool is_call)
      : id_(id), strike_(strike), premium_(premium), is_call_(is_call) {}

  double Process() const override { return strike_ + premium_; }

  uint64_t id() const noexcept { return id_; }
  double strike() const noexcept { return strike_; }
  double premium() const noexcept { return premium_; }
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

struct StockPOD {
  uint64_t id;
  double price;
  uint32_t volume;

  double Process() const noexcept { return price * volume; }
};

struct BondPOD {
  uint64_t id;
  double face_value;
  double coupon_rate;

  double Process() const noexcept { return face_value * (1.0 + coupon_rate); }
};

struct OptionPOD {
  uint64_t id;
  double strike;
  double premium;
  bool is_call;

  double Process() const noexcept { return strike + premium; }
};

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
