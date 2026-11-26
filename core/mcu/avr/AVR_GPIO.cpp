#include "mcu/avr/AVR_GPIO.h"
#include "mcu/avr/ATmega328P_Constants.h"

AVR_GPIO::AVR_GPIO() { reset(); }

void AVR_GPIO::set_external_level(uint8_t bit, PinLevel level) {
  if (bit < external_.size())
    external_[bit] = level;
}

uint8_t AVR_GPIO::pin_levels() const {
  const uint8_t ddr = ddr_reg_.value();
  const uint8_t port = port_reg_.value();

  uint8_t result = 0;
  for (int i = 0; i < 8; ++i) {
    const bool port_bit = (port >> i) & 1;
    bool level;
    if ((ddr >> i) & 1) {
      level = port_bit; // output: the pin follows PORTx
    } else {
      switch (external_[i]) {
      case PinLevel::High:
        level = true;
        break;
      case PinLevel::Low:
        level = false;
        break;
      case PinLevel::Floating:
      default:
        level = port_bit; // pull-up enabled -> 1, otherwise reads as 0
        break;
      }
    }
    if (level)
      result |= static_cast<uint8_t>(1u << i);
  }
  return result;
}

uint8_t AVR_GPIO::read(uint32_t offset) const {
  switch (offset) {
  case atmega328p::gpio::REG_PIN:
    return pin_levels();
  case atmega328p::gpio::REG_DDR:
    return ddr_reg_.value();
  case atmega328p::gpio::REG_PORT:
    return port_reg_.value();
  default:
    return 0;
  }
}

void AVR_GPIO::write(uint32_t offset, uint8_t value) {
  switch (offset) {
  case atmega328p::gpio::REG_PIN:
    // Writing 1 to a PINx bit toggles the matching PORTx bit.
    port_reg_.set(static_cast<uint8_t>(port_reg_.value() ^ value));
    break;
  case atmega328p::gpio::REG_DDR:
    ddr_reg_.set(value);
    break;
  case atmega328p::gpio::REG_PORT:
    port_reg_.set(value);
    break;
  }
}

void AVR_GPIO::reset() {
  ddr_reg_.set(0);
  port_reg_.set(0);
}
