// Timer0: счёт, флаги, режимы, прерывания.
#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

namespace {
// Бесконечный цикл rjmp .-1 (2 такта на итерацию)
std::unique_ptr<IEmulatorFacade> idle_emu() {
  return make_emu({{0, {rjmp(-1)}}});
}

// Вектор -> обработчик (rjmp), чтобы не зависеть от длины векторов.
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
  emu->write_data(TCCR0B, 7, 1); // внешний источник — не моделируется
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
  emu->write_data(TIFR0, 0, 1); // запись нуля ничего не сбрасывает
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
  CHECK_EQ(emu->read_data(TCNT0, 1), c % 10);    // период 10: 0..9
  CHECK(emu->read_data(TIFR0, 1) & 0x02);        // OCF0A
  CHECK_EQ(emu->read_data(TIFR0, 1) & 0x01, 0u); // TOV0 в CTC не наступает
}

TEST(timer0_phase_correct_counts_up_then_down) {
  auto emu = idle_emu();
  emu->write_data(TCCR0A, 1, 1); // WGM00 = Phase Correct, TOP = 0xFF
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(254);
  CHECK_EQ(emu->read_data(TCNT0, 1), 254u);
  emu->run_for_cycles(2);
  CHECK_EQ(emu->read_data(TCNT0, 1), 255u - 1u); // 255 достигнут и пошёл вниз
  emu->run_for_cycles(254);
  CHECK_EQ(emu->read_data(TCNT0, 1), 0u);
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 1u); // TOV0 на BOTTOM
}

TEST(timer0_pwm_ocr_is_double_buffered) {
  auto emu = idle_emu();
  emu->write_data(TCCR0A, 3, 1); // Fast PWM, TOP = 0xFF
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(10); // OCR0A действующий = 0: совпадение на счёте 0
  emu->write_data(OCR0A, 100, 1);
  emu->write_data(TIFR0, 0x02, 1);          // сбросить OCF0A
  CHECK_EQ(emu->read_data(OCR0A, 1), 100u); // читается буфер
  // До BOTTOM действует старое значение 0: при счёте через 100 совпадения нет
  emu->run_for_cycles(190); // всего 200 тактов, счётчик = 200
  CHECK_EQ(emu->read_data(TIFR0, 1) & 0x02, 0u);
  // После BOTTOM новое значение вступило в силу: счётчик снова проходит 100
  emu->run_for_cycles(170); // всего 370 тактов, счётчик = 114
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
  CHECK_EQ(emu->read_data(20, 1), 5u); // ровно 5 переполнений = 5 вызовов ISR
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
  CHECK_EQ(emu->read_data(SREG, 1) & 0x80, 0u); // I сброшен при входе
}

TEST(interrupt_waits_for_global_enable_and_toie0) {
  auto emu = make_emu({
      {0, {rjmp(0x3F)}},
      vector_to(0x20, 0x50),
      {0x40, {rjmp(-1)}}, // I не включаем
      {0x50, {inc(20), RETI}},
  });
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->run_for_cycles(1000);
  CHECK_EQ(emu->read_data(20, 1), 0u);        // I = 0: ISR не вызывался
  CHECK_EQ(emu->read_data(TIFR0, 1) & 1, 1u); // но флаг висит
  emu->write_data(SREG, 0x80, 1);             // включили I
  emu->run_for_cycles(20);
  CHECK(emu->read_data(20, 1) >= 1u); // отложенное прерывание сработало
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
  emu->run_for_cycles(300); // TOV0 взведён, запрос ждёт I
  emu->write_data(TIMSK0, 0, 1);
  emu->write_data(SREG, 0x80, 1);
  emu->run_for_cycles(50);
  CHECK_EQ(emu->read_data(20, 1), 0u);
}

TEST(interrupt_priority_lower_vector_first) {
  // TCNT0=0xFE, OCR0A=0xFF: на одном такте выставляются OCF0A и TOV0.
  // COMPA (0x1C) приоритетнее OVF (0x20): обработчики пишут порядковый номер.
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
  CHECK_EQ(emu->read_data(21, 1), 1u); // COMPA первым
  CHECK_EQ(emu->read_data(22, 1), 2u); // затем OVF
}

TEST(one_instruction_runs_after_reti_before_next_interrupt) {
  // ISR завершается RETI; сразу висит ещё одно прерывание. После RETI обязана
  // выполниться хотя бы одна инструкция основной программы (счётчик r17).
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
  // идём до первого входа в COMPA, потом проверяем ход выполнения
  int guard = 0;
  while (emu->get_pc() != 0x1Cu && guard++ < 1000)
    emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 0x1Cu);
  emu->step_instruction(); // rjmp -> обработчик
  emu->step_instruction(); // RETI
  uint32_t after_reti = emu->get_pc();
  CHECK(after_reti >= 0x40u && after_reti < 0x45u); // вернулись в main,
                                                    // OVF не вклинился
  emu->step_instruction();        // ровно одна инструкция основной программы...
  CHECK_EQ(emu->get_pc(), 0x20u); // ...и только затем вход в OVF
}
