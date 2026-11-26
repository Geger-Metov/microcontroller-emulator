#pragma once

#include <string>

#include "hex_parser/hex_parser.h"

/**
 * A universal representation of a loaded program
 */
struct ProgramImage {
  std::vector<uint8_t> flash;
  std::vector<uint8_t> eeprom;

  // What the loader could not place (not an error, but the user should know),
  // for example fuse/lock-bit data.
  std::vector<std::string> warnings;

  // optional:
  uint32_t entryPoint = 0;
};

/**
 * The program loader interface
 */
class IProgramLoader {
public:
  virtual ~IProgramLoader() = default;

  virtual ProgramImage load(const HexImage &hex) = 0;
};
