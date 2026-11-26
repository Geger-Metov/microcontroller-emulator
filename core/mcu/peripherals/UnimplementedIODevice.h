#pragma once

#include <set>

#include "interfaces/Interfaces.h"

/**
 * A stub for I/O registers that are not implemented yet.
 *
 * A read returns 0 and a write is ignored (RAZ/WI) instead of throwing an
 * exception: real firmware (for example Arduino init()) touches dozens of
 * UART/ADC/SPI/TWI/... registers at startup, and the emulator must not crash
 * because of that. All touched addresses are remembered and available through
 * touched() for diagnostics
 * ("what the firmware expects from a peripheral that does not exist yet").
 */
class UnimplementedIODevice : public IMemoryMapped {
public:
  explicit UnimplementedIODevice(uint32_t size) : size_(size) {}

  uint8_t read(uint32_t offset) const override {
    touched_.insert(offset);
    return 0;
  }
  void write(uint32_t offset, uint8_t) override { touched_.insert(offset); }
  uint32_t size() const override { return size_; }

  /// Offsets (from the start of the range) that the program accessed.
  const std::set<uint32_t> &touched() const { return touched_; }

private:
  uint32_t size_;
  mutable std::set<uint32_t> touched_;
};
