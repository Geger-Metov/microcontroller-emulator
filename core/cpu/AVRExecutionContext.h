#pragma once

#include <cstdint>

#include "../interfaces/Interfaces.h"
#include "AVR_CPU.h"

enum class SREGFlag : uint8_t { C, Z, N, V, S, H, T, I };

class AVRExecutionContext {
 public:
  AVRExecutionContext(AVR_CPU& cpu, IRegisterFile& regs, ISREG& sreg,
                      IDataBus& data_bus)
      : cpu_(cpu), regs_(regs), sreg_(sreg), data_bus_(data_bus) {}

  AVR_CPU& cpu() { return cpu_; }
  const AVR_CPU& cpu() const { return cpu_; }

  IRegisterFile& regs() { return regs_; }
  const IRegisterFile& regs() const { return regs_; }

  uint8_t read_reg(uint8_t index) const { return regs_.read_reg(index); }
  void write_reg(uint8_t index, uint8_t value) {
    regs_.write_reg(index, value);
  }
  uint16_t read_reg16(uint8_t low) const {
    uint16_t lo = regs_.read_reg(low);
    uint16_t hi = regs_.read_reg(low + 1);

    return lo | (hi << 8);
  }
  void write_reg16(uint8_t low, uint16_t value) {
    regs_.write_reg(low, value & 0xFF);
    regs_.write_reg(low + 1, value >> 8);
  }

  ISREG& sreg() { return sreg_; }
  const ISREG& sreg() const { return sreg_; }

  void set_flag(SREGFlag flag, bool value) {
    sreg_.set_flag(static_cast<uint8_t>(flag), value);
  }
  bool get_flag(SREGFlag flag) const {
    return sreg_.get_flag(static_cast<uint8_t>(flag));
  }

  uint8_t io_read(uint8_t addr) const { return data_bus_.read(0x20 + addr); }
  void io_write(uint8_t addr, uint8_t val) {
    data_bus_.write(0x20 + addr, val);
  }

  uint8_t data_read(uint16_t addr) const { return data_bus_.read(addr); }
  void data_write(uint16_t addr, uint8_t val) { data_bus_.write(addr, val); }

  void push8(uint8_t val) { cpu_.push8(val); }
  uint8_t pop8() { return cpu_.pop8(); }
  void push_pc(uint32_t pc) { cpu_.push_pc(pc); }
  uint32_t pop_pc() { return cpu_.pop_pc(); }
  uint32_t pc() const { return cpu_.get_pc(); }
  void set_pc(uint32_t pc) { cpu_.set_pc(pc); }

  uint16_t fetch_program_word(uint32_t word_addr) const {
    return cpu().code_bus().read_word(word_addr);
  }

 private:
  AVR_CPU& cpu_;
  IRegisterFile& regs_;
  ISREG& sreg_;
  IDataBus& data_bus_;
};
