#pragma once

#include "../interfaces/Interfaces.h"
#include "MMIORegister.h"

class Timer0 : public IMemoryMapped, public ITickable, public IResettable {
 public:
  Timer0();

  uint8_t read(uint32_t offset) const override;
  void write(uint32_t offset, uint8_t value) override;
  uint32_t size() const override;

  void tick(uint64_t cycles) override;

  void reset() override {
    TCCR0B.set(0);
    TCNT0.set(0);
    TIMSK0.set(0);
    TIFR0.set(0);

    prescaler_accum_ = 0;
  };

  void attach_interrupt_sink(IInterruptSink* sink) { interrupt_sink_ = sink; }

  MMIORegister TCCR0B;
  MMIORegister TCNT0;
  MMIORegister TIMSK0;
  MMIORegister TIFR0;

 private:
  // Внутренний предделитель
  uint64_t prescaler_accum_ = 0;
  static constexpr uint16_t prescaler_divisors[8] = {
      0,     // 000 - таймер остановлен
      1,     // 001 - clk/1
      8,     // 010 - clk/8
      64,    // 011 - clk/64
      256,   // 100 - clk/256
      1024,  // 101 - clk/1024
      0,     // 110 - внешний источник (не поддерживаем) - почему?
      0,     // 111 - внешний источник (не поддерживаем) - почему?
  };

  IInterruptSink* interrupt_sink_ = nullptr;

  void check_overflow();
};
