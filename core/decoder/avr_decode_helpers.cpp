#include "avr_decode_helpers.h"

#include "../cpu/AVRExecutionContext.h"
#include "../cpu/avr_instructions.h"

void avr::decode_none(uint32_t, avr::DecodedInstruction&) {}

void avr::decode_Rd_Rr(uint32_t opcode, avr::DecodedInstruction& out) {
  out.Rd = ((opcode >> 4) & 0x1F);
  out.Rr = (opcode & 0x0F) | ((opcode >> 5) & 0x10);
}

void avr::decode_Rd_imm(uint32_t opcode, avr::DecodedInstruction& out) {
  out.Rd = 16 + ((opcode >> 4) & 0x0F);
  out.imm = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
}

void avr::decode_rel7(uint32_t opcode, avr::DecodedInstruction& out) {
  uint8_t k = (opcode >> 3) & 0x7F;

  // sign extend 7-bit
  if (k & 0x40) k |= 0x80;

  out.rel = static_cast<uint8_t>(k);
}

void avr::decode_rel7_s3(uint32_t opcode, avr::DecodedInstruction& out) {
  uint8_t s = opcode & 0x07;
  uint8_t k = static_cast<uint8_t>((opcode >> 3) & 0x7F);

  // sign extend 7-bit
  if (k & 0x40) k |= 0x80;

  out.sreg_bit = s;
  out.rel = k;
}

void avr::decode_rel12(uint32_t opcode, avr::DecodedInstruction& out) {
  uint16_t k = opcode & 0x0FFF;

  // sign extend 12-bit
  if (k & 0x0800) k |= 0xF000;

  out.rel = k;
}

void avr::decode_abs22(uint32_t opcode, avr::DecodedInstruction& out) {
  uint16_t first = opcode & 0xFFFF;
  uint16_t second = opcode >> 16;

  uint32_t k = 0;
  k |= second;
  k |= ((first >> 3) & 0x3E0000);
  k |= ((first >> 4) & 0x010000);

  out.imm = k;
}

void avr::decode_IO_Rr(uint32_t opcode, avr::DecodedInstruction& out) {
  out.Rr = ((opcode >> 4) & 0x1F);
  out.imm = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
}

void avr::decode_Rd_IO(uint32_t opcode, avr::DecodedInstruction& out) {
  out.Rd = ((opcode >> 4) & 0x1F);
  out.imm = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
}
void avr::decode_Rd(uint32_t opcode, avr::DecodedInstruction& out) {
  out.Rd = (opcode >> 4) & 0x1F;
}

void avr::decode_io_bit(uint32_t opcode, avr::DecodedInstruction& out) {
  out.io = (opcode >> 3) & 0x1F;
  out.bit = opcode & 0x07;
}

void avr::decode_Rd_pair_K(uint32_t opcode, DecodedInstruction& out) {
  uint8_t dd = (opcode >> 4) & 0x03;
  out.Rd = 24 + dd * 2;  // начальный регистр пары
  out.imm = ((opcode & 0x00C0) >> 2) | (opcode & 0x000F);  // K[5:0]
}

void avr::decode_Rd_Rr_pair(uint32_t opcode, DecodedInstruction& out) {
  out.Rd = ((opcode >> 4) & 0x0F) * 2;  // Rd = старшие 4 бита номера пары * 2
  out.Rr = (opcode & 0x0F) * 2;         // Rr = младшие 4 бита номера пары * 2
}

const avr::InstructionDesc* avr::find_instruction_desc(uint16_t opcode) {
  for (std::size_t i = 0; i < avre_plus_instruction_table_size; ++i) {
    const auto& desc = avre_plus_instruction_table[i];

    if ((opcode & desc.mask) == desc.pattern) {
      return &desc;
    }
  }
  return nullptr;
}

uint8_t avr::get_next_instruction_words(AVRExecutionContext& ctx) {
  uint32_t next_pc = ctx.pc() + 1;
  uint16_t opcode = ctx.fetch_program_word(next_pc);
  const InstructionDesc* desc = find_instruction_desc(opcode);

  if (!desc) return 1;
  return desc->length;
}
