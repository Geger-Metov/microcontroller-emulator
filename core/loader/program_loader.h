#pragma once

#include "../hex_parser/hex_parser.h"

/**
 * Универсальное представление загруженной программы
 */
struct ProgramImage {
  std::vector<uint8_t> flash;
  std::vector<uint8_t> eeprom;

  // опционально:
  uint32_t entryPoint = 0;
};

/**
 * Интерфейс загрузчика программы
 */
class IProgramLoader {
 public:
  virtual ~IProgramLoader() = default;

  virtual ProgramImage load(const HexImage& hex) = 0;
};
