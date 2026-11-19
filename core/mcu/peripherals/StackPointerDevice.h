#pragma once

#include "../../cpu/AVR_CPU.h"
#include "../../interfaces/Interfaces.h"

class StackPointerDevice : public IMemoryMapped {
public:
  explicit StackPointerDevice(AVR_CPU *cpu) : cpu_(cpu) {}

  uint8_t read(uint32_t offset) const override {
    uint16_t sp = cpu_->get_sp();
    if (offset == 0)
      return sp & 0xFF;
    else
      return (sp >> 8) & 0xFF;
  }

  void write(uint32_t offset, uint8_t value) override {
    uint16_t sp = cpu_->get_sp();
    if (offset == 0) {
      sp = (sp & 0xFF00) | value;
    } else {
      sp = (sp & 0x00FF) | (static_cast<uint16_t>(value) << 8);
    }
    cpu_->set_sp(sp);
  }

  uint32_t size() const override { return 2; }

private:
  AVR_CPU *cpu_;
};
