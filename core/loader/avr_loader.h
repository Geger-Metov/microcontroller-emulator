#pragma once

#include <cstdint>

#include "program_loader.h"

struct AVRMemoryLayout {
  uint32_t flashSize;   // в байтах
  uint32_t eepromSize;  // в байтах
  uint32_t flashBase;   // обычно 0x0000
  uint32_t eepromBase;  // зависит от модели (например, 0x0000 в отдельном
                        // пространстве)
};

class AVRProgramLoader : public IProgramLoader {
 public:
  explicit AVRProgramLoader(const AVRMemoryLayout& layout);

  ProgramImage load(const HexImage& hex) override;

 private:
  AVRMemoryLayout layout_;

  void writeFlash(ProgramImage& image, uint32_t address,
                  const std::vector<uint8_t>& data);
  void writeEEPROM(ProgramImage& image, uint32_t address,
                   const std::vector<uint8_t>& data);
};
