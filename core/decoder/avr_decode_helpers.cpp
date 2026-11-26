#include "decoder/avr_decode_helpers.h"
#include "cpu/AVRExecutionContext.h"
#include "cpu/avr_instructions.h"

void avr::decode_none(uint32_t, avr::DecodedInstruction &) {}

void avr::decode_Rd_Rr(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = ((opcode >> 4) & 0x1F);
  out.Rr = (opcode & 0x0F) | ((opcode >> 5) & 0x10);
}

void avr::decode_Rd_imm(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = 16 + ((opcode >> 4) & 0x0F);
  out.imm = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
}

void avr::decode_rel7(uint32_t opcode, avr::DecodedInstruction &out) {
  // 7-bit two's complement: -64..63. Must be computed in a signed type,
  // otherwise `rel` ends up as 0..255 and backward branches go forward.
  int32_t k = (opcode >> 3) & 0x7F;
  if (k & 0x40)
    k -= 0x80;

  out.rel = k;
}

void avr::decode_rel7_s3(uint32_t opcode, avr::DecodedInstruction &out) {
  uint8_t s = opcode & 0x07;
  int32_t k = (opcode >> 3) & 0x7F;
  if (k & 0x40)
    k -= 0x80;

  out.sreg_bit = s;
  out.rel = k;
}

void avr::decode_rel12(uint32_t opcode, avr::DecodedInstruction &out) {
  // 12-bit two's complement: -2048..2047.
  int32_t k = opcode & 0x0FFF;
  if (k & 0x0800)
    k -= 0x1000;

  out.rel = k;
}

void avr::decode_abs22(uint32_t opcode, avr::DecodedInstruction &out) {
  // JMP/CALL: 1001 010k kkkk 11mk kkkk kkkk kkkk kkkk — k[21:17] in bits 8..4
  // of the first word, k[16] in bit 0, k[15:0] in the second word.
  uint32_t first = opcode & 0xFFFF;
  uint32_t second = opcode >> 16;

  out.imm = (((first >> 4) & 0x1F) << 17) | ((first & 1) << 16) | second;
}

void avr::decode_IO_Rr(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rr = ((opcode >> 4) & 0x1F);
  out.imm = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
}

void avr::decode_Rd_IO(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = ((opcode >> 4) & 0x1F);
  out.imm = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
}
void avr::decode_Rd(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = (opcode >> 4) & 0x1F;
}

void avr::decode_io_bit(uint32_t opcode, avr::DecodedInstruction &out) {
  out.io = (opcode >> 3) & 0x1F;
  out.bit = opcode & 0x07;
}

void avr::decode_Rd_pair_K(uint32_t opcode, DecodedInstruction &out) {
  uint8_t dd = (opcode >> 4) & 0x03;
  out.Rd = 24 + dd * 2; // the first register of the pair
  out.imm = ((opcode & 0x00C0) >> 2) | (opcode & 0x000F); // K[5:0]
}

void avr::decode_Rd_Rr_pair(uint32_t opcode, DecodedInstruction &out) {
  out.Rd = ((opcode >> 4) & 0x0F) *
           2;                   // Rd = the upper 4 bits of the pair number * 2
  out.Rr = (opcode & 0x0F) * 2; // Rr = the lower 4 bits of the pair number * 2
}

void avr::decode_Rd_bit(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = (opcode >> 4) & 0x1F;
  out.bit = opcode & 0x07;
}

void avr::decode_Rd_k16(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = (opcode >> 4) & 0x1F;
  out.imm = opcode >> 16; // the data address in the second word
}

void avr::decode_ld_st_ptr(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = (opcode >> 4) & 0x1F; // for ST this is Rr, but the field is the same
  out.imm = 0;
  switch (opcode & 0x0F) {
  case 0x1:
    out.ptr = 30;
    out.mode = 1;
    break; // Z+
  case 0x2:
    out.ptr = 30;
    out.mode = 2;
    break; // -Z
  case 0x9:
    out.ptr = 28;
    out.mode = 1;
    break; // Y+
  case 0xA:
    out.ptr = 28;
    out.mode = 2;
    break; // -Y
  case 0xC:
    out.ptr = 26;
    out.mode = 0;
    break; // X
  case 0xD:
    out.ptr = 26;
    out.mode = 1;
    break; // X+
  default:
    out.ptr = 26;
    out.mode = 2;
    break; // -X (0xE)
  }
}

void avr::decode_ldd_std(uint32_t opcode, avr::DecodedInstruction &out) {
  // 10q0 qq0d dddd yqqq: y=1 -> Y, y=0 -> Z; q = displacement 0..63
  out.Rd = (opcode >> 4) & 0x1F;
  out.ptr = (opcode & 0x08) ? 28 : 30;
  out.mode = 0;
  out.imm = ((opcode >> 8) & 0x20) | ((opcode >> 7) & 0x18) | (opcode & 0x07);
}

void avr::decode_lpm_rd_z(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = (opcode >> 4) & 0x1F;
  out.mode = (opcode & 1) ? 1 : 0; // 9005: Z+, 9004: Z
}

void avr::decode_Rd16_Rr16(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = 16 + ((opcode >> 4) & 0x0F);
  out.Rr = 16 + (opcode & 0x0F);
}

void avr::decode_Rd16_Rr16_3bit(uint32_t opcode, avr::DecodedInstruction &out) {
  out.Rd = 16 + ((opcode >> 4) & 0x07);
  out.Rr = 16 + (opcode & 0x07);
}

void avr::decode_sreg_bit(uint32_t opcode, avr::DecodedInstruction &out) {
  out.sreg_bit = (opcode >> 4) & 0x07;
}

uint8_t avr::get_next_instruction_words(AVRExecutionContext &ctx) {
  return ctx.cpu().instruction_length_at(ctx.pc() + 1);
}
