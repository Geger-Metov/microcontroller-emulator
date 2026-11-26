#pragma once

#include "interfaces/Interfaces.h"
#include "mcu/peripherals/MMIORegister.h"

/**
 * 8-bit Timer/Counter0 (ATmega328P).
 *
 * Implemented: all counting modes (Normal, CTC, Fast PWM, Phase Correct PWM,
 * including TOP = OCR0A), prescaler clk/1..clk/1024, the TOV0/OCF0A/OCF0B
 * flags, the TIMSK0 mask and three interrupt vectors, double buffering of OCR0x
 * in PWM.
 *
 * Not implemented: the OC0A/OC0B outputs (COM0xx bits are only stored), the
 * FOC0x strobe, an external clock source on pin T0 (CS02:0 = 110/111 — the
 * timer is considered stopped).
 *
 * The timer is NOT mapped into memory as a whole: each register sits at its
 * own address (see ATmega328P::build_memory_map), and the bus delivers time to
 * the timer through MemoryBus::add_tickable().
 */
class Timer0 : public ITickable, public IResettable {
public:
  Timer0();

  void tick(uint64_t cycles) override;
  void reset() override;

  /// Attach the interrupt controller (CPU) and register flag clearing
  /// on vector entry.
  void attach_interrupt_sink(IInterruptSink *sink);

  MMIORegister TCCR0A;
  MMIORegister TCCR0B;
  MMIORegister TCNT0;
  MMIORegister OCR0A; // in PWM modes holds the buffer value
  MMIORegister OCR0B;
  MMIORegister TIMSK0;
  MMIORegister TIFR0;

private:
  static constexpr uint8_t TOV0 = 1u << 0;
  static constexpr uint8_t OCF0A = 1u << 1;
  static constexpr uint8_t OCF0B = 1u << 2;

  // Internal prescaler
  uint64_t prescaler_accum_ = 0;
  static constexpr uint16_t prescaler_divisors[8] = {
      0,    // 000 - timer stopped
      1,    // 001 - clk/1
      8,    // 010 - clk/8
      64,   // 011 - clk/64
      256,  // 100 - clk/256
      1024, // 101 - clk/1024
      0,    // 110 - external source on T0 (the pin is not modeled)
      0,    // 111 - external source on T0 (the pin is not modeled)
  };

  // Effective OCR0x values (in PWM modes they are updated from the buffer at a
  // specific point of the count, not immediately on write).
  uint8_t ocr_a_active_ = 0;
  uint8_t ocr_b_active_ = 0;
  bool counting_up_ = true;   // for Phase Correct PWM
  bool skip_compare_ = false; // a write to TCNT0 blocks the compare for 1 cycle

  IInterruptSink *interrupt_sink_ = nullptr;

  uint8_t wgm() const;
  bool ocr_double_buffered() const;
  void clock_timer();
  void load_ocr_from_buffer();
  void update_irq();
};
