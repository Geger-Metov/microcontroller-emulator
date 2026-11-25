#pragma once

#include <cstdint>

#include "cpu/avr_instructions.h"

namespace avr {
void decode_none(uint32_t, avr::DecodedInstruction &);
void decode_Rd_Rr(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd_imm(uint32_t opcode, avr::DecodedInstruction &out);
void decode_rel7(uint32_t opcode, avr::DecodedInstruction &out);
void decode_rel7_s3(uint32_t opcode, avr::DecodedInstruction &out);
void decode_rel12(uint32_t opcode, avr::DecodedInstruction &out);
void decode_abs22(uint32_t opcode, avr::DecodedInstruction &out);
void decode_IO_Rr(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd_IO(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd(uint32_t opcode, avr::DecodedInstruction &out);
void decode_io_bit(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd_pair_K(uint32_t opcode, DecodedInstruction &out);
void decode_Rd_Rr_pair(uint32_t opcode, DecodedInstruction &out);

void decode_Rd_bit(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd_k16(uint32_t opcode, avr::DecodedInstruction &out);
void decode_ld_st_ptr(uint32_t opcode, avr::DecodedInstruction &out);
void decode_ldd_std(uint32_t opcode, avr::DecodedInstruction &out);
void decode_lpm_rd_z(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd16_Rr16(uint32_t opcode, avr::DecodedInstruction &out);
void decode_Rd16_Rr16_3bit(uint32_t opcode, avr::DecodedInstruction &out);
void decode_sreg_bit(uint32_t opcode, avr::DecodedInstruction &out);

uint8_t get_next_instruction_words(AVRExecutionContext &ctx);
} // namespace avr
