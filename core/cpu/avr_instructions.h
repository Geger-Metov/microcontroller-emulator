#pragma once

#include <cstdint>

class AVRExecutionContext;

namespace avr {

/**
 * Результат декодирования инструкции
 * Содержит уже извлечённые операнды
 */
struct DecodedInstruction {
  uint8_t Rd = 0;
  uint8_t Rr = 0;
  uint32_t imm = 0;
  int32_t rel = 0; // signed relative displacement
  uint8_t io = 0;
  uint8_t bit = 0;
  uint8_t sreg_bit = 0;
};

/**
 * Описание инструкции (ISA entry)
 */
struct InstructionDesc {
  uint16_t mask;
  uint16_t pattern;
  /**
   * @brief длина инструкции (в словах: 1 или 2)
   */
  uint8_t length;
  /**
   * @brief базовое число циклов (при особых условиях дополнительные циклы
   * добавляются в exec_* методах)
   */
  uint8_t cycles;
  // декодирование операндов
  void (*decode)(uint32_t opcode, DecodedInstruction &out);
  // выполнение инструкции
  void (*execute)(AVRExecutionContext &ctx, const DecodedInstruction &instr);
};

/**
 * Таблица инструкций (определяется в .cpp)
 */
extern const InstructionDesc avre_plus_instruction_table[];
extern const std::size_t avre_plus_instruction_table_size;

void exec_ADD(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ADC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SUB(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CPI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_INC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_DEC(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_LDI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MOV(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_OUT(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_IN(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_RET(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RETI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CALL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RCALL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_JMP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_RJMP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_NOP(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_AND(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_OR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_EOR(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_BRNE(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BREQ(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRBC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRCC(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_CPSE(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBIC(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBIS(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_PUSH(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_POP(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_SBI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CBI(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_SUBI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_CLR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SER(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBCI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ANDI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ORI(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LDS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_STS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRCS(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRSH(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_BRLO(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_LSR(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ROR(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_ADIW(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_SBIW(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_IJMP(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ICALL(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_MOVW(AVRExecutionContext &ctx, const DecodedInstruction &instr);

void exec_LPM_Zplus(AVRExecutionContext &ctx, const DecodedInstruction &instr);
void exec_ELPM_Zplus(AVRExecutionContext &ctx, const DecodedInstruction &instr);

/*
void exec_SLEEP(AVRExecutionContext& ctx, const DecodedInstruction& instr);
LD X/Y/Z
ST X/Y/Z
*/
} // namespace avr
