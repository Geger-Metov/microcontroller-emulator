#pragma once

#include <cstdint>

#include "program_loader.h"

// Адреса областей в HEX-файле, который делает avr-objcopy (линейная адресация).
constexpr uint32_t AVR_HEX_FLASH_BASE = 0x000000;
constexpr uint32_t AVR_HEX_EEPROM_BASE = 0x810000;

struct AVRMemoryLayout {
  uint32_t flashSize;  // в байтах
  uint32_t eepromSize; // в байтах
  uint32_t flashBase;  // адрес flash в HEX-файле (AVR_HEX_FLASH_BASE)
  uint32_t eepromBase; // адрес EEPROM в HEX-файле (AVR_HEX_EEPROM_BASE)
};

class AVRProgramLoader : public IProgramLoader {
public:
  explicit AVRProgramLoader(const AVRMemoryLayout &layout);

  // Размещает данные HEX во flash и EEPROM. Запись, начинающаяся в области, но
  // выходящая за её конец, — std::runtime_error. Данные вне обеих областей
  // (fuse, lock bits, signature) не ошибка: они пропускаются и попадают в
  // ProgramImage::warnings.
  ProgramImage load(const HexImage &hex) override;

private:
  AVRMemoryLayout layout_;
};
