#include "Timer0.h"

#include "../avr/ATmega328P_Constants.h"

Timer0::Timer0() {
  TCCR0B.on_write([this](uint8_t v) {
    TCCR0B.set(v & 0x07);
    prescaler_accum_ = 0;
  });

  TIMSK0.on_write([this](uint8_t v) { TIMSK0.set(v & 0x01); });

  TIFR0.on_write([this](uint8_t v) {
    uint8_t cur = TIFR0.value();

    if (v & 0x01) cur &= ~0x01;

    TIFR0.set(cur);
  });

  reset();
}

void Timer0::tick(uint64_t cycles) {
  uint8_t cs = TCCR0B.value() & 0x07;

  uint16_t divisor = prescaler_divisors[cs];
  if (divisor == 0) return;

  prescaler_accum_ += cycles;
  while (prescaler_accum_ >= divisor) {
    prescaler_accum_ -= divisor;

    uint8_t val = TCNT0.value() + 1;
    TCNT0.set(val);

    if (val == 0) {
      TIFR0.set(TIFR0.value() | 0x01);

      check_overflow();
    }
  }
}

void Timer0::check_overflow() {
  if ((TIMSK0.value() & 0x01) && (TIFR0.value() & 0x01)) {
    if (interrupt_sink_) {
      interrupt_sink_->raise_interrupt(atmega328p::interrupts::TIMER0_OVF);
    }
  }
}

uint8_t Timer0::read(uint32_t offset) const {
  switch (offset) {
    case 0x00:
      return TCCR0B.read(0);
    case 0x01:
      return TCNT0.read(0);
    case 0x02:
      return TIMSK0.read(0);
    case 0x03:
      return TIFR0.read(0);
    default:
      throw std::out_of_range("Timer0::read invalid offset");
  }
}

void Timer0::write(uint32_t offset, uint8_t value) {
  switch (offset) {
    case 0x00:
      TCCR0B.write(0, value);
      break;
    case 0x01:
      TCNT0.write(0, value);
      break;
    case 0x02:
      TIMSK0.write(0, value);
      break;
    case 0x03:
      TIFR0.write(0, value);
      break;
    default:
      throw std::out_of_range("Timer0::write invalid offset");
  }
}

uint32_t Timer0::size() const { return 4; }
