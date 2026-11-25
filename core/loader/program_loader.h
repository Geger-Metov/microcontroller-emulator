#pragma once

#include <string>

#include "hex_parser/hex_parser.h"

/**
 * Универсальное представление загруженной программы
 */
struct ProgramImage {
  std::vector<uint8_t> flash;
  std::vector<uint8_t> eeprom;

  // Что загрузчик не смог разместить (не ошибка, но пользователю стоит знать),
  // например данные fuse/lock-битов.
  std::vector<std::string> warnings;

  // опционально:
  uint32_t entryPoint = 0;
};

/**
 * Интерфейс загрузчика программы
 */
class IProgramLoader {
public:
  virtual ~IProgramLoader() = default;

  virtual ProgramImage load(const HexImage &hex) = 0;
};
