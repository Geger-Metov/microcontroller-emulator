// Timer0: counting, flags, modes, interrupts.
#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

namespace {
// An infinite loop rjmp .-1 (2 cycles per iteration)
std::unique_ptr<IEmulatorFacade> idle_emu() {
  return make_emu({{0, {rjmp(-1)}}});
}

// Vector -> handler (rjmp), so as not to depend on the vector length.
Chunk vector_to(uint32_t vec, uint32_t handler) {
  return {vec, {rjmp(static_cast<int>(handler) - static_cast<int>(vec) - 1)}};
}
} // namespace

TEST(timer0_counts_at_cpu_clock) {
  auto emu = idle_emu();
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(1000);
  CHECK_EQ(emu->get_cycle_count(), 1000u);
  CHECK_EQ(emu->read_data(TCNT0, 1), 1000u % 256);
}

TEST(timer0_prescaler_divides_clock) {
  auto emu = idle_emu();
  emu->write_data(TCCR0B, 2, 1); // clk/8
  emu->run_for_cycles(2000);
  CHECK_EQ(emu->read_data(TCNT0, 1), 250u);
}

TEST(timer0_stopped_when_cs_is_zero_or_external) {
  auto emu = idle_emu();
  emu->run_for_cycles(1000);
  CHECK_EQ(emu->read_data(TCNT0, 1), 0u);
  emu->write_data(TCCR0B, 7, 1); // external clock source — not modeled
  emu->run_for_cycles(1000);
  CHECK_EQ(emu->read_data(TCNT0, 1), 0u);
}

TEST(tov0_set_on_overflow_and_cleared_by_writing_one) {
  auto emu = idle_emu();
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(254);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 0u);
  emu->run_for_cycles(4);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 1u);
  emu->write_data(TIFR0, 0, 1); // writing zero clears nothing
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 1u);
  emu->write_data(TIFR0, 1, 1);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 0u);
}

TEST(timer0_ctc_mode_wraps_at_ocr0a) {
  auto emu = idle_emu();
  emu->write_data(TCCR0A, 2, 1); // WGM01 = CTC
  emu->write_data(OCR0A, 9, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(26);
  uint64_t c = emu->get_cycle_count();
  CHECK_EQ(emu->read_data(TCNT0, 1), c % 10);    // period 10: 0..9
  CHECK(emu->read_data(TIFR0, 1) & 0x02);        // OCF0A
  CHECK_EQ(emu->read_data(TIFR0, 1) & 0x01, 0u); // TOV0 does not occur in CTC
}

TEST(timer0_phase_correct_counts_up_then_down) {
  auto emu = idle_emu();
  emu->write_data(TCCR0A, 1, 1); // WGM00 = Phase Correct, TOP = 0xFF
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(254);
  CHECK_EQ(emu->read_data(TCNT0, 1), 254u);
  emu->run_for_cycles(2);
  CHECK_EQ(emu->read_data(TCNT0, 1),
           255u - 1u); // 255 reached and counting down
  emu->run_for_cycles(254);
  CHECK_EQ(emu->read_data(TCNT0, 1), 0u);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 1u); // TOV0 at BOTTOM
}

TEST(timer0_pwm_ocr_is_double_buffered) {
  auto emu = idle_emu();
  emu->write_data(TCCR0A, 3, 1); // Fast PWM, TOP = 0xFF
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(10); // effective OCR0A = 0: a match at count 0
  emu->write_data(OCR0A, 100, 1);
  emu->write_data(TIFR0, 0x02, 1);          // clear OCF0A
  CHECK_EQ(emu->read_data(OCR0A, 1), 100u); // the buffer is read
  // Until BOTTOM the old value 0 is in effect: no match when counting through
  // 100
  emu->run_for_cycles(190); // 200 cycles in total, the counter = 200
  CHECK_EQ(emu->read_data(TIFR0, 1) & 0x02, 0u);
  // After BOTTOM the new value takes effect: the counter passes 100 again
  emu->run_for_cycles(170); // 370 cycles in total, the counter = 114
  CHECK(emu->read_data(TIFR0, 1) & 0x02);
}

TEST(timer0_compare_match_flags) {
  auto emu = idle_emu();
  emu->write_data(OCR0A, 20, 1);
  emu->write_data(OCR0B, 30, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(22);
  CHECK(emu->read_data(TIFR0, 1) & 0x02);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 0x04, 0u);
  emu->run_for_cycles(10);
  CHECK(emu->read_data(TIFR0, 1) & 0x04);
}

TEST(timer0_overflow_interrupt_runs_isr) {
  // 0: rjmp main ; 0x20 (TIMER0_OVF): rjmp isr ; main: sreg=0x80, loop
  auto emu = make_emu({
      {0, {rjmp(0x3F)}}, // -> 0x40
      vector_to(0x20, 0x50),
      {0x40, {ldi(16, 0x80), out(0x3F, 16), rjmp(-1)}},
      {0x50, {inc(20), RETI}},
  });
  emu->write_data(TIMSK0, 1, 1); // TOIE0
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(256 * 5 + 100);
  CHECK_EQ(emu->read_data(20, 1), 5u); // exactly 5 overflows = 5 ISR calls
}

TEST(tov0_is_cleared_by_hardware_on_interrupt_entry) {
  auto emu = make_emu({
      {0, {rjmp(0x3F)}},
      vector_to(0x20, 0x50),
      {0x40, {ldi(16, 0x80), out(0x3F, 16), rjmp(-1)}},
      {0x50, {inc(20), RETI}},
  });
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCCR0B, 1, 1);
  int guard = 0;
  while (emu->get_pc() != 0x20u && guard++ < 10000)
    emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 0x20u);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 0u);
  CHECK_EQ(emu->read_data(SREG, 1) & 0x80, 0u); // I is cleared on entry
}

TEST(interrupt_waits_for_global_enable_and_toie0) {
  auto emu = make_emu({
      {0, {rjmp(0x3F)}},
      vector_to(0x20, 0x50),
      {0x40, {rjmp(-1)}}, // we do not enable I
      {0x50, {inc(20), RETI}},
  });
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(1000);
  CHECK_EQ(emu->read_data(20, 1), 0u);        // I = 0: the ISR was not called
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 1u); // but the flag is pending
  emu->write_data(SREG, 0x80, 1);             // enabled I
  emu->run_for_cycles(20);
  CHECK(emu->read_data(20, 1) >= 1u); // the deferred interrupt fired
}

TEST(clearing_toie0_withdraws_pending_interrupt) {
  auto emu = make_emu({
      {0, {rjmp(0x3F)}},
      vector_to(0x20, 0x50),
      {0x40, {rjmp(-1)}},
      {0x50, {inc(20), RETI}},
  });
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(300); // TOV0 is set, the request waits for I
  emu->write_data(TIMSK0, 0, 1);
  emu->write_data(SREG, 0x80, 1);
  emu->run_for_cycles(50);
  CHECK_EQ(emu->read_data(20, 1), 0u);
}

TEST(interrupt_priority_lower_vector_first) {
  // TCNT0=0xFE, OCR0A=0xFF: OCF0A and TOV0 are set on the same cycle.
  // COMPA (0x1C) has priority over OVF (0x20): the handlers write a sequence
  // number.
  auto emu = make_emu({
      {0, {rjmp(0x3F)}},
      vector_to(0x1C, 0x50),
      vector_to(0x20, 0x60),
      {0x40, {ldi(16, 0x80), out(0x3F, 16), rjmp(-1)}},
      {0x50, {inc(20), mov(21, 20), RETI}}, // COMPA
      {0x60, {inc(20), mov(22, 20), RETI}}, // OVF
  });
  emu->write_data(TIMSK0, 0x03, 1);
  emu->write_data(OCR0A, 0xFF, 1);
  emu->write_data(TCNT0, 0xFE, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(200);
  CHECK_EQ(emu->read_data(21, 1), 1u); // COMPA first
  CHECK_EQ(emu->read_data(22, 1), 2u); // then OVF
}

TEST(one_instruction_runs_after_reti_before_next_interrupt) {
  // The ISR ends with RETI; another interrupt is pending right away. After RETI
  // at least one main-program instruction must execute (the r17 counter).
  auto emu = make_emu({
      {0, {rjmp(0x3F)}},
      vector_to(0x1C, 0x50),
      vector_to(0x20, 0x60),
      {0x40, {ldi(16, 0x80), out(0x3F, 16), inc(17), rjmp(-2)}},
      {0x50, {RETI}},
      {0x60, {RETI}},
  });
  emu->write_data(TIMSK0, 0x03, 1);
  emu->write_data(OCR0A, 0xFF, 1);
  emu->write_data(TCNT0, 0xFE, 1);
  emu->write_data(TCCR0B, 1, 1);
  // run to the first entry into COMPA, then check the execution flow
  int guard = 0;
  while (emu->get_pc() != 0x1Cu && guard++ < 1000)
    emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 0x1Cu);
  emu->step_instruction(); // rjmp -> handler
  emu->step_instruction(); // RETI
  uint32_t after_reti = emu->get_pc();
  CHECK(after_reti >= 0x40u && after_reti < 0x45u); // returned to main,
                                                    // OVF did not cut in
  emu->step_instruction();        // exactly one main-program instruction...
  CHECK_EQ(emu->get_pc(), 0x20u); // ...and only then the entry into OVF
}
