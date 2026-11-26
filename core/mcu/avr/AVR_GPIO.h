#pragma once

#include <array>

#include "interfaces/Interfaces.h"
#include "mcu/peripherals/MMIORegister.h"

/**
 * One 8-bit GPIO port (PINx, DDRx, PORTx).
 *
 * - PINx reads the level on each pin. An output pin shows its PORTx bit; an
 * input shows the externally applied level, or, if nothing drives it
 * (Floating), the pull-up: 1 when PORTx = 1, otherwise 0.
 * - Writing 1 to a PINx bit toggles the matching PORTx bit (as on the real
 * chip).
 * - Simplification: the synchronizer delay of PINx (about one cycle) is not
 * modeled.
 */
class AVR_GPIO : public IDevice, public IPinInput {
public:
  AVR_GPIO();

  // IPinInput. The external level survives reset(): the outside world
  // does not change when the MCU is reset.
  void set_external_level(uint8_t bit, PinLevel level) override;

  // IMemoryMapped
  uint8_t read(uint32_t offset) const override;
  void write(uint32_t offset, uint8_t value) override;
  uint32_t size() const override { return 3; } // num_regs

  // ITickable (does nothing)
  void tick(uint64_t) override {}

  // IDevice
  void reset() override;

private:
  MMIORegister ddr_reg_;
  MMIORegister port_reg_;

  std::array<PinLevel, 8> external_{}; // all Floating

  uint8_t pin_levels() const;
};
