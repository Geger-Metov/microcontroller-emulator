#pragma once

#include <array>

#include "interfaces/Interfaces.h"
#include "mcu/peripherals/MMIORegister.h"
#include "mcu/peripherals/Wire.h"

class AVR_GPIO : public IDevice {
public:
  AVR_GPIO();

  void connect_wire(uint8_t bit, Wire::Endpoint *wire);

  // IMemoryMapped
  uint8_t read(uint32_t offset) const override;
  void write(uint32_t offset, uint8_t value) override;
  uint32_t size() const override { return 3; } // num_regs

  // ITickable (ничего не делает)
  void tick(uint64_t) override {}

  // IDevice
  void reset() override;

private:
  MMIORegister pin_reg_;
  MMIORegister ddr_reg_;
  MMIORegister port_reg_;

  std::array<Wire::Endpoint *, 8> wires_ = {nullptr};

  void update_outputs();
};
