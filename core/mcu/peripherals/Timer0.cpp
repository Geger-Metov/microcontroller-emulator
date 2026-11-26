#include "mcu/peripherals/Timer0.h"
#include "mcu/avr/ATmega328P_Constants.h"

namespace {
// Flag bit index → interrupt vector
constexpr uint8_t kFlagBits[3] = {1u << 0, 1u << 1, 1u << 2};
constexpr uint16_t kVectors[3] = {atmega328p::interrupts::TIMER0_OVF,
                                  atmega328p::interrupts::TIMER0_COMPA,
                                  atmega328p::interrupts::TIMER0_COMPB};
} // namespace

Timer0::Timer0() {
  // WGM01:00 and COM0xx; bits 3:2 are reserved
  TCCR0A.on_write([this](uint8_t v) {
    TCCR0A.set(v & 0xF3);
    if (!ocr_double_buffered())
      load_ocr_from_buffer();
  });

  // WGM02 and CS02:0; FOC0A/FOC0B (strobe) read as 0 and are not modeled
  TCCR0B.on_write([this](uint8_t v) {
    uint8_t old_cs = TCCR0B.value() & 0x07;
    TCCR0B.set(v & 0x0F);
    if ((v & 0x07) != old_cs)
      prescaler_accum_ = 0;
    if (!ocr_double_buffered())
      load_ocr_from_buffer();
  });

  TCNT0.on_write([this](uint8_t v) {
    TCNT0.set(v);
    skip_compare_ = true;
  });

  OCR0A.on_write([this](uint8_t v) {
    OCR0A.set(v);
    if (!ocr_double_buffered())
      ocr_a_active_ = v;
  });
  OCR0B.on_write([this](uint8_t v) {
    OCR0B.set(v);
    if (!ocr_double_buffered())
      ocr_b_active_ = v;
  });

  TIMSK0.on_write([this](uint8_t v) {
    TIMSK0.set(v & 0x07);
    update_irq();
  });

  // Flags are cleared by writing a one
  TIFR0.on_write([this](uint8_t v) {
    TIFR0.set(TIFR0.value() & ~(v & 0x07));
    update_irq();
  });

  reset();
}

void Timer0::reset() {
  TCCR0A.set(0);
  TCCR0B.set(0);
  TCNT0.set(0);
  OCR0A.set(0);
  OCR0B.set(0);
  TIMSK0.set(0);
  TIFR0.set(0);

  ocr_a_active_ = 0;
  ocr_b_active_ = 0;
  prescaler_accum_ = 0;
  counting_up_ = true;
  skip_compare_ = false;
  update_irq();
}

void Timer0::attach_interrupt_sink(IInterruptSink *sink) {
  interrupt_sink_ = sink;
  if (!sink)
    return;
  // The hardware clears the interrupt flag on vector entry
  for (int i = 0; i < 3; ++i) {
    sink->set_interrupt_ack_handler(kVectors[i], [this, i] {
      TIFR0.set(TIFR0.value() & static_cast<uint8_t>(~kFlagBits[i]));
    });
  }
}

uint8_t Timer0::wgm() const {
  return static_cast<uint8_t>(((TCCR0B.value() >> 3) & 1) << 2 |
                              (TCCR0A.value() & 3));
}

// PWM modes: 1 (PC, TOP=FF), 3 (Fast, TOP=FF), 5 (PC, TOP=OCRA),
// 7 (Fast, TOP=OCRA)
bool Timer0::ocr_double_buffered() const {
  uint8_t m = wgm();
  return m == 1 || m == 3 || m == 5 || m == 7;
}

void Timer0::load_ocr_from_buffer() {
  ocr_a_active_ = OCR0A.value();
  ocr_b_active_ = OCR0B.value();
}

void Timer0::tick(uint64_t cycles) {
  uint16_t divisor = prescaler_divisors[TCCR0B.value() & 0x07];
  if (divisor == 0)
    return;

  prescaler_accum_ += cycles;
  while (prescaler_accum_ >= divisor) {
    prescaler_accum_ -= divisor;
    clock_timer();
  }
  update_irq();
}

// One timer tick (after the prescaler).
void Timer0::clock_timer() {
  const uint8_t mode = wgm();
  const uint8_t cnt = TCNT0.value();
  uint8_t flags = TIFR0.value();

  // Compare match: the flag is set on the cycle AFTER TCNT0 matches OCR0x
  // (datasheet, "Setting of OCF0x"); a write to TCNT0 blocks the compare.
  if (!skip_compare_) {
    if (cnt == ocr_a_active_)
      flags |= OCF0A;
    if (cnt == ocr_b_active_)
      flags |= OCF0B;
  }
  skip_compare_ = false;

  const bool top_is_ocra = (mode == 2 || mode == 5 || mode == 7);
  const uint8_t top = top_is_ocra ? ocr_a_active_ : 0xFF;
  uint8_t next;

  if (mode == 1 || mode == 5) {
    // Phase Correct PWM: up to TOP, then down to BOTTOM
    if (counting_up_) {
      next = static_cast<uint8_t>(cnt + 1);
      if (next >= top) {
        next = top;
        counting_up_ = false;
        load_ocr_from_buffer(); // OCR is updated at TOP
      }
    } else {
      next = cnt == 0 ? 0 : static_cast<uint8_t>(cnt - 1);
      if (next == 0) {
        counting_up_ = true;
        flags |= TOV0; // TOV0 at BOTTOM
      }
    }
  } else {
    // Normal / CTC / Fast PWM: count up
    next = (top_is_ocra && cnt == top) ? 0 : static_cast<uint8_t>(cnt + 1);

    if (mode == 7) {
      if (next == top)
        flags |= TOV0; // Fast PWM with TOP=OCR0A: TOV0 at TOP
    } else if (cnt == 0xFF && next == 0) {
      flags |= TOV0; // TOV0 at MAX
    }

    if (ocr_double_buffered() && next == 0)
      load_ocr_from_buffer(); // Fast PWM: OCR is updated at BOTTOM
  }

  TCNT0.set(next);
  TIFR0.set(flags);
}

// The interrupt line is active while the flag is set AND the mask is enabled (a
// level, not a pulse): if TOIE0 is enabled while TOV0 is already set, the
// interrupt fires; if the flag is cleared or the mask removed, the request is
// withdrawn.
void Timer0::update_irq() {
  if (!interrupt_sink_)
    return;
  const uint8_t active = TIFR0.value() & TIMSK0.value();
  for (int i = 0; i < 3; ++i) {
    if (active & kFlagBits[i])
      interrupt_sink_->raise_interrupt(kVectors[i]);
    else
      interrupt_sink_->clear_interrupt(kVectors[i]);
  }
}
