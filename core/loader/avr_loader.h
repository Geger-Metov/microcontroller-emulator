#pragma once

#include <cstdint>

#include "loader/program_loader.h"

// Region addresses in the HEX file produced by avr-objcopy (linear addressing).
constexpr uint32_t AVR_HEX_FLASH_BASE = 0x000000;
constexpr uint32_t AVR_HEX_EEPROM_BASE = 0x810000;

struct AVRMemoryLayout {
  uint32_t flashSize;  // in bytes
  uint32_t eepromSize; // in bytes
  uint32_t flashBase;  // flash address in the HEX file (AVR_HEX_FLASH_BASE)
  uint32_t eepromBase; // EEPROM address in the HEX file (AVR_HEX_EEPROM_BASE)
};

class AVRProgramLoader : public IProgramLoader {
public:
  explicit AVRProgramLoader(const AVRMemoryLayout &layout);

  // Places HEX data into flash and EEPROM. A record that starts inside a region
  // but runs past its end is a HexFormatError. Data outside both regions (fuse,
  // lock bits, signature) is not an error: it is skipped and goes into
  // ProgramImage::warnings.
  ProgramImage load(const HexImage &hex) override;

private:
  AVRMemoryLayout layout_;
};
