#pragma once

#include <cstdint>

// Векторы прерываний ATmega328P (адреса в СЛОВАХ; шаг 2 слова, т.к. в
// каждом векторе лежит JMP). См. ATmega328P datasheet, Table 11-6.
namespace atmega328p::interrupts {
constexpr uint16_t RESET = 0x0000;
constexpr uint16_t INT0 = 0x0002;
constexpr uint16_t INT1 = 0x0004;
constexpr uint16_t PCINT0 = 0x0006;
constexpr uint16_t PCINT1 = 0x0008;
constexpr uint16_t PCINT2 = 0x000A;
constexpr uint16_t WDT = 0x000C;
constexpr uint16_t TIMER2_COMPA = 0x000E;
constexpr uint16_t TIMER2_COMPB = 0x0010;
constexpr uint16_t TIMER2_OVF = 0x0012;
constexpr uint16_t TIMER1_CAPT = 0x0014;
constexpr uint16_t TIMER1_COMPA = 0x0016;
constexpr uint16_t TIMER1_COMPB = 0x0018;
constexpr uint16_t TIMER1_OVF = 0x001A;
constexpr uint16_t TIMER0_COMPA = 0x001C;
constexpr uint16_t TIMER0_COMPB = 0x001E;
constexpr uint16_t TIMER0_OVF = 0x0020;
constexpr uint16_t SPI_STC = 0x0022;
constexpr uint16_t USART_RX = 0x0024;
constexpr uint16_t USART_UDRE = 0x0026;
constexpr uint16_t USART_TX = 0x0028;
constexpr uint16_t ADC = 0x002A;
constexpr uint16_t EE_READY = 0x002C;
constexpr uint16_t ANALOG_COMP = 0x002E;
constexpr uint16_t TWI = 0x0030;
constexpr uint16_t SPM_READY = 0x0032;
} // namespace atmega328p::interrupts

namespace atmega328p::gpio {
constexpr uint8_t REG_PIN = 0;
constexpr uint8_t REG_DDR = 1;
constexpr uint8_t REG_PORT = 2;
} // namespace atmega328p::gpio
