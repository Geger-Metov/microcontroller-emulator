#pragma once

#include "interfaces/Interfaces.h"
#include "mcu/peripherals/MMIORegister.h"

/**
 * 8-bit Timer/Counter0 (ATmega328P).
 *
 * Реализовано: все режимы счёта (Normal, CTC, Fast PWM, Phase Correct PWM,
 * включая TOP = OCR0A), предделитель clk/1..clk/1024, флаги TOV0/OCF0A/OCF0B,
 * маска TIMSK0 и три вектора прерываний, двойная буферизация OCR0x в PWM.
 *
 * Не реализовано: выходы OC0A/OC0B (биты COM0xx только сохраняются), строб
 * FOC0x, внешний источник тактов на пине T0 (CS02:0 = 110/111 — таймер
 * считается остановленным).
 *
 * Таймер НЕ отображается в память целиком: каждый регистр лежит по своему
 * адресу (см. ATmega328P::build_memory_map), а время таймеру сообщает шина
 * через MemoryBus::add_tickable().
 */
class Timer0 : public ITickable, public IResettable {
public:
  Timer0();

  void tick(uint64_t cycles) override;
  void reset() override;

  /// Подключить контроллер прерываний (CPU) и зарегистрировать сброс флагов
  /// при входе в вектор.
  void attach_interrupt_sink(IInterruptSink *sink);

  MMIORegister TCCR0A;
  MMIORegister TCCR0B;
  MMIORegister TCNT0;
  MMIORegister OCR0A; // для PWM-режимов хранит значение буфера
  MMIORegister OCR0B;
  MMIORegister TIMSK0;
  MMIORegister TIFR0;

private:
  static constexpr uint8_t TOV0 = 1u << 0;
  static constexpr uint8_t OCF0A = 1u << 1;
  static constexpr uint8_t OCF0B = 1u << 2;

  // Внутренний предделитель
  uint64_t prescaler_accum_ = 0;
  static constexpr uint16_t prescaler_divisors[8] = {
      0,    // 000 - таймер остановлен
      1,    // 001 - clk/1
      8,    // 010 - clk/8
      64,   // 011 - clk/64
      256,  // 100 - clk/256
      1024, // 101 - clk/1024
      0,    // 110 - внешний источник на T0 (пин не моделируется)
      0,    // 111 - внешний источник на T0 (пин не моделируется)
  };

  // Действующие значения OCR0x (в PWM-режимах обновляются из буфера в
  // определённый момент счёта, а не сразу при записи).
  uint8_t ocr_a_active_ = 0;
  uint8_t ocr_b_active_ = 0;
  bool counting_up_ = true;   // для Phase Correct PWM
  bool skip_compare_ = false; // запись в TCNT0 блокирует сравнение на 1 такт

  IInterruptSink *interrupt_sink_ = nullptr;

  uint8_t wgm() const;
  bool ocr_double_buffered() const;
  void clock_timer();
  void load_ocr_from_buffer();
  void update_irq();
};
