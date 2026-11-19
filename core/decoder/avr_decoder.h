#pragma once

#include "../cpu/avr_instructions.h"
#include "../interfaces/Interfaces.h"

/**
 * Универсальный декодер инструкций AVR
 */
class AVRDecoder : public IDecoder {
public:
  AVRDecoder(const avr::InstructionDesc *table, std::size_t count)
      : table_(table), count_(count) {}

  const avr::InstructionDesc *
  decode(uint32_t opcode, avr::DecodedInstruction &out) const override {
    for (std::size_t i = 0; i < count_; ++i) {
      const auto &desc = table_[i];
      if ((opcode & desc.mask) == desc.pattern) {
        desc.decode(opcode, out);
        return &desc;
      }
    }
    // отладка
    fprintf(stderr, "[Decoder] Unknown opcode: 0x%04X (PC=0x%X)\n",
            (uint16_t)opcode,
            /* сюда желательно передать PC, но его здесь нет */ 0);
    return nullptr;
  }

private:
  const avr::InstructionDesc *table_;
  std::size_t count_;
};
