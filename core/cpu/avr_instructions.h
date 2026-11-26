#pragma once

#include <cstdint>

class AVRExecutionContext;

namespace avr {

/**
 * The result of decoding an instruction
 * Contains the already extracted operands
 */
struct DecodedInstruction {
  uint8_t Rd = 0;
  uint8_t Rr = 0;
  uint32_t imm = 0;
  int32_t rel = 0; // signed relative displacement
  uint8_t io = 0;
  uint8_t bit = 0;
  uint8_t sreg_bit = 0;
  /// The low register of a pointer pair (26=X, 28=Y, 30=Z) for LD/ST.
  uint8_t ptr = 0;
  /// LD/ST addressing mode: 0 — plain (+displacement), 1 — post-increment,
  /// 2 — pre-decrement.
  uint8_t mode = 0;
};

/**
 * Instruction description (an ISA entry)
 */
struct InstructionDesc {
  uint16_t mask;
  uint16_t pattern;
  /**
   * @brief instruction length (in words: 1 or 2)
   */
  uint8_t length;
  /**
   * @brief the base number of cycles (under special conditions extra cycles
   * are added in the exec_* methods)
   */
  uint8_t cycles;
  // operand decoding
  void (*decode)(uint32_t opcode, DecodedInstruction &out);
  // instruction execution
  void (*execute)(AVRExecutionContext &ctx, const DecodedInstruction &instr);
};

/**
 * The instruction table (defined in the .cpp)
 */
extern const InstructionDesc avre_plus_instruction_table[];
extern const std::size_t avre_plus_instruction_table_size;

void exec_ADD(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ADC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SUB(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CPC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CPI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SUBI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBCI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ANDI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ORI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_AND(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_OR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_EOR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_COM(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_NEG(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SWAP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_INC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_DEC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LSR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ASR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ROR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ADIW(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBIW(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MUL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MULS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MULSU(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_FMUL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_FMULS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_FMULSU(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MOV(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MOVW(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LDI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_IN(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_OUT(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_PUSH(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_POP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LD(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ST(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LDS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_STS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LPM(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LPM_R0(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RJMP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_JMP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_IJMP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RCALL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CALL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ICALL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RET(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RETI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRBS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRBC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CPSE(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBRC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBRS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBIC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBIS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CBI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BSET(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BCLR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BLD(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BST(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_NOP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SLEEP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SPM(AVRExecutionContext &ctx, const DecodedInstruction &instr);

} // namespace avr
