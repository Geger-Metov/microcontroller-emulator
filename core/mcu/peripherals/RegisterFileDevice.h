#pragma once

#include <array>
#include <stdexcept>

#include "../interfaces/Interfaces.h"

class RegisterFileDevice : public IMemoryMapped, public IRegisterFile {
 public:
  RegisterFileDevice() { regs_.fill(0); }

  uint8_t read(uint32_t offset) const override {
    if (offset >= 32)
      throw std::out_of_range("RegisterFileDevice:: read offset out of range");
    return regs_[offset];
  }

  void write(uint32_t offset, uint8_t value) override {
    if (offset >= 32)
      throw std::out_of_range("RegisterFileDevice:: write offset out of range");
    regs_[offset] = value;
  }

  uint32_t size() const override { return 32; }

  // Прямой доступ для CPU
  uint8_t& operator[](uint8_t idx) { return regs_[idx]; }
  const uint8_t& operator[](uint8_t idx) const { return regs_[idx]; }

  void write_reg(uint8_t index, uint8_t val) override {
    if (index >= 32)
      throw std::out_of_range("RegisterFileDevice:: write offset out of range");
    regs_[index] = val;
  }
  uint8_t read_reg(uint8_t index) const override {
    if (index >= 32)
      throw std::out_of_range("RegisterFileDevice:: read offset out of range");
    return regs_[index];
  }

  void reset() override { regs_.fill(0); }

  uint8_t& ref(uint8_t index) override {
    if (index >= 32)
      throw std::out_of_range("RegisterFileDevice::ref out of range");

    return regs_[index];
  }
  const uint8_t& ref(uint8_t index) const override {
    if (index >= 32)
      throw std::out_of_range("RegisterFileDevice::ref const out of range");
    return regs_[index];
  }

 private:
  std::array<uint8_t, 32> regs_;
};
