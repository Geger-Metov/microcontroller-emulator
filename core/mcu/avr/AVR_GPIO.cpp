#include "AVR_GPIO.h"

#include "ATmega328P_Constants.h"

AVR_GPIO::AVR_GPIO() { reset(); }

void AVR_GPIO::connect_wire(uint8_t bit, Wire::Endpoint *wire) {
  wires_[bit] = wire;
  update_outputs();
}

uint8_t AVR_GPIO::read(uint32_t offset) const {
  switch (offset) {
  case atmega328p::gpio::REG_PIN: {
    uint8_t result = 0;

    for (int i = 0; i < 8; ++i) {
      auto *w = wires_[i];
      if (!w)
        continue;

      auto level = w->read();
      if (level == WireLevel::High)
        result |= (1 << i);
    }
    return result;
  }
  case atmega328p::gpio::REG_DDR:
    return ddr_reg_.value();
  case atmega328p::gpio::REG_PORT:;
    return port_reg_.value();
  default:
    return 0;
  }
}

void AVR_GPIO::write(uint32_t offset, uint8_t value) {
  switch (offset) {
  case 1:
    ddr_reg_.set(value);
    update_outputs();
    break;
  case 2:
    port_reg_.set(value);
    update_outputs();
    break;
  }
}

void AVR_GPIO::reset() {
  ddr_reg_.set(0);
  port_reg_.set(0);
  update_outputs();
}

void AVR_GPIO::update_outputs() {
  for (int i = 0; i < 8; ++i) {
    auto *w = wires_[i];

    if (!w)
      continue;

    bool output = (ddr_reg_.value() >> i) & 1;
    bool port = (port_reg_.value() >> i) & 1;

    if (output) {
      w->drive(port ? PinDriveMode::DriveHigh : PinDriveMode::DriveLow);
    } else {
      w->drive(port ? PinDriveMode::PullUp : PinDriveMode::HiZ);
    }
  }
}
