#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "../interfaces/Interfaces.h"

// Чистое процессорное ядро
/**
 * @note
 * Interrupt entry timing is simplified: entry always costs 4 cycles
 * (2 for the PC push, 2 for the vector jump). The vector itself contains a
 * JMP/RJMP which is then executed as a normal instruction.
 *
 * Interrupts are accepted between instructions. After SEI and RETI one more
 * instruction is always executed before a pending interrupt is taken, as on
 * real AVR cores.
 */
class AVR_CPU : public ICPU, public IInterruptSink {
public:
  explicit AVR_CPU(std::unique_ptr<IDecoder> decoder);

  // IInterruptSink
  void raise_interrupt(uint16_t vector) override;
  void clear_interrupt(uint16_t vector) override;
  void set_interrupt_ack_handler(uint16_t vector,
                                 std::function<void()> on_ack) override;

  // ICPU
  void step() override;
  void reset() override;
  // может быть пустым, если у CPU нет внутренней задержки
  void tick(uint64_t cycles) override;

  void attach_data_bus(IDataBus *bus) { data_bus_ = bus; }
  void attach_code_bus(ICodeBus *bus) { code_bus_ = bus; }
  void set_register_file(IRegisterFile *rf) { regfile_ = rf; }
  void set_sreg_device(ISREG *sreg) { sreg_ = sreg; }

  // Отладка
  uint32_t get_pc() const override { return pc_; }
  uint32_t get_sp() const override { return sp_; }
  uint8_t get_sreg() const override { return sreg_->value(); }
  const uint8_t *get_registers() const override {
    return regfile_ ? &regfile_->ref(0) : nullptr;
  }

  // Очередной accessor
  IRegisterFile &registers() { return *regfile_; }
  const IRegisterFile &registers() const { return *regfile_; }
  ICodeBus &code_bus() { return *code_bus_; }
  const ICodeBus &code_bus() const { return *code_bus_; }

  // Специфичные для AVR методы, используемые обработчиками инструкций
  void set_flag(uint8_t flag, bool value);
  bool get_flag(uint8_t flag) const;
  void set_initial_sp(uint16_t sp) { initial_sp_ = sp; }

  /**
   * Добавить такты к ТЕКУЩЕЙ инструкции (взятый переход, пропуск и т.п.).
   * Итоговая стоимость инструкции = desc->cycles + добавленные такты;
   * именно она возвращается в get_last_instruction_cycles() и попадает и в
   * общий счётчик, и в тики периферии.
   */
  void add_cycles(uint16_t n) { extra_cycles_ += n; }
  uint64_t get_cycle_count() const override { return total_cycles_; }

  /// После SEI/RETI следующая инструкция выполняется до обработки прерывания.
  void inhibit_interrupts_for_next_instruction() { irq_inhibit_ = true; }

  /// SLEEP: остановить выполнение до прерывания (периферия продолжает тикать).
  void enter_sleep() { sleeping_ = true; }
  bool is_sleeping() const { return sleeping_; }

  void set_pc(uint32_t new_pc) {
    pc_ = new_pc;
    pc_modified_ = true;
  }
  void set_sp(uint16_t sp) { sp_ = sp; }

  uint16_t get_last_instruction_cycles() const override {
    return last_instruction_cycles_;
  }

  uint8_t instruction_length_at(uint32_t pc) const;

  void push8(uint8_t val);
  uint8_t pop8();

  /// Адрес возврата кладётся как на реальном AVR: сначала младший байт
  /// (по большему адресу), затем старший.
  void push_pc(uint32_t pc);
  uint32_t pop_pc();

  void service_interrupt(uint16_t vector, uint32_t return_pc);
  void set_pc_width_bytes(uint8_t n) { pc_width_bytes_ = n; }

  uint16_t read_program_word(uint32_t word_addr) const {
    return code_bus_->read_word(word_addr);
  }

private:
  IDataBus *data_bus_ = nullptr; // Подключается извне
  ICodeBus *code_bus_ = nullptr;
  IRegisterFile *regfile_ = nullptr;
  ISREG *sreg_ = nullptr;
  uint32_t pc_ = 0;
  uint8_t pc_width_bytes_ = 2;
  uint16_t sp_ = 0;
  uint64_t total_cycles_ = 0;
  uint16_t initial_sp_ = 0;
  bool pc_modified_ = false;
  uint16_t last_instruction_cycles_ = 0;
  uint16_t extra_cycles_ = 0;
  std::unique_ptr<IDecoder> decoder_;

  // Прерывания. Меньший адрес вектора = выше приоритет, поэтому упорядоченное
  // множество даёт нужную очерёдность «из коробки».
  std::set<uint16_t> pending_interrupts_;
  std::map<uint16_t, std::function<void()>> interrupt_ack_;
  bool irq_inhibit_ = false;
  bool sleeping_ = false;

  void commit_cycles(uint16_t cycles);
  void take_pending_interrupt();
};
