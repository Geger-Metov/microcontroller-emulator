#pragma once

#include "Interfaces.h"

class SREGDevice : public IMemoryMapped, public ISREG, public IResettable {
public:
  uint8_t read(uint32_t) const override { return value_; }
  void write(uint32_t, uint8_t value) override { value_ = value; }
  uint32_t size() const override { return 1; }

  uint8_t value() const override { return value_; }
  void set(uint8_t v) override { value_ = v; }
  bool get_flag(uint8_t bit) const override { return (value_ >> bit) & 1; }
  void set_flag(uint8_t bit, bool val) override {
    if (val)
      value_ |= (1 << bit);
    else
      value_ &= ~(1 << bit);
  }

  void reset() override { value_ = 0; }

private:
  uint8_t value_ = 0;
};
/**
 * 0 - C
 * 1 - Z
 * 2 - N
 * 3 - V
 * 4 - S
 * 5 - H
 * 6 - T
 * 7 - I
 */
