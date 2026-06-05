#pragma once

#include <memory>

#include "../interfaces/Interfaces.h"

// Чистое процессорное ядро
/**
 * @note
 * Interrupt entry timing is simplified.
 * Real AVR cores incur additional cycles for:
 * - PC push
 * - pipeline flush
 * - vector fetch
 */
class AVR_CPU : public ICPU, public IInterruptSink {
 public:
  explicit AVR_CPU(std::unique_ptr<IDecoder> decoder);

  // IInterruptSink
  void raise_interrupt(uint16_t vector) override;

  // ICPU
  void step() override;
  void reset() override;
  // может быть пустым, если у CPU нет внутренней задержки
  void tick(uint64_t cycles) override;

  void attach_data_bus(IDataBus* bus) { data_bus_ = bus; }
  void attach_code_bus(ICodeBus* bus) { code_bus_ = bus; }
  void set_register_file(IRegisterFile* rf) { regfile_ = rf; }
  void set_sreg_device(ISREG* sreg) { sreg_ = sreg; }

  // Отладка
  uint32_t get_pc() const override { return pc_; }
  uint32_t get_sp() const override { return sp_; }
  uint8_t get_sreg() const override { return sreg_->value(); }
  const uint8_t* get_registers() const override {
    return regfile_ ? &regfile_->ref(0) : nullptr;
  }

  // Очередной accessor
  IRegisterFile& registers() { return *regfile_; }
  const IRegisterFile& registers() const { return *regfile_; }
  ICodeBus& code_bus() { return *code_bus_; }
  const ICodeBus& code_bus() const { return *code_bus_; }

  // Специфичные для AVR методы, используемые обработчиками инструкций
  void set_flag(uint8_t flag, bool value);
  bool get_flag(uint8_t flag) const;
  void set_initial_sp(uint16_t sp) { initial_sp_ = sp; }
  void add_cycles(uint16_t n) { total_cycles_ += n; }
  uint64_t get_cycle_count() const { return total_cycles_; }
  // Для skip-инструкций
  void set_skip_next(bool skip) { skip_next_ = skip; }
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
  void push16(uint16_t val);
  uint16_t pop16();
  void push32(uint32_t val);
  uint32_t pop32();

  void push_pc(uint32_t pc);
  uint32_t pop_pc();

  void service_interrupt(uint32_t return_pc);
  void set_pc_width_bytes(uint8_t n) { pc_width_bytes_ = n; }

  uint16_t read_program_word(uint16_t word_addr) const {
    return code_bus_->read_word(word_addr);
  }

 private:
  IDataBus* data_bus_ = nullptr;  // Подключается извне
  ICodeBus* code_bus_ = nullptr;
  IRegisterFile* regfile_ = nullptr;
  ISREG* sreg_ = nullptr;
  uint32_t pc_ = 0;
  uint8_t pc_width_bytes_ = 2;
  uint16_t sp_ = 0;
  uint64_t total_cycles_ = 0;
  uint16_t initial_sp_ = 0;
  bool skip_next_ = false;
  bool pc_modified_ = false;
  uint16_t last_instruction_cycles_ = 0;
  std::unique_ptr<IDecoder> decoder_;
  bool interrupt_pending_ = false;
  uint16_t pending_interrupt_vector_ = 0;

  uint8_t io_read(uint8_t io_addr) const {
    return data_bus_->read(0x20 + io_addr);
  }
  void io_write(uint8_t io_addr, uint8_t value) {
    data_bus_->write(0x20 + io_addr, value);
  }
};
