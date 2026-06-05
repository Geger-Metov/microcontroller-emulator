#pragma once

#include <cstdint>

namespace atmega328p::interrupts {
constexpr uint16_t TIMER0_OVF = 0x0010;  // word address
}

namespace atmega328p::gpio {
constexpr uint8_t REG_PIN = 0;
constexpr uint8_t REG_DDR = 1;
constexpr uint8_t REG_PORT = 2;
}