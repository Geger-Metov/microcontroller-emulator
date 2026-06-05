#include "avr_instructions.h"

#include "../decoder/avr_decode_helpers.h"
#include "AVRExecutionContext.h"

void avr::exec_NOP(AVRExecutionContext& ctx, const DecodedInstruction& instr) {}

void avr::exec_RJMP(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  ctx.set_pc(ctx.pc() + instr.rel + 1);
}

void avr::exec_JMP(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  ctx.set_pc(instr.imm);
}

void avr::exec_CALL(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint32_t return_pc = ctx.pc() + 2;
  ctx.push_pc(return_pc);
  ctx.set_pc(instr.imm);
}

void avr::exec_RCALL(AVRExecutionContext& ctx,
                     const DecodedInstruction& instr) {
  uint32_t return_pc = ctx.pc() + 1;
  ctx.push_pc(return_pc);
  ctx.set_pc(ctx.pc() + instr.rel + 1);
}

void avr::exec_RET(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  ctx.set_pc(ctx.pop_pc());
}

void avr::exec_RETI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  ctx.set_pc(ctx.pop_pc());
  ctx.set_flag(SREGFlag::I, true);
}

void avr::exec_MOV(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t val = ctx.read_reg(instr.Rr);
  ctx.write_reg(instr.Rd, val);
}

void avr::exec_LDI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  ctx.write_reg(instr.Rd, instr.imm);
}

void avr::exec_OUT(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t val = ctx.read_reg(instr.Rr);
  ctx.io_write(instr.imm, val);
}

void avr::exec_IN(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t val = ctx.io_read(instr.imm);
  ctx.write_reg(instr.Rd, val);
}

void avr::exec_ADD(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);
  uint16_t sum = static_cast<uint16_t>(rd) + static_cast<uint16_t>(rr);
  uint8_t res = static_cast<uint8_t>(sum);
  ctx.write_reg(instr.Rd, res);

  bool rd3 = rd & (1 << 3);
  bool rr3 = rr & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool rr7 = rr & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (rd3 && rr3) || (rr3 && !r3) || (!r3 && rd3);
  bool V = (rd7 && rr7 && !r7) || (!rd7 && !rr7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0);
  bool C = (rd7 && rr7) || (rr7 && !r7) || (!r7 && rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

void avr::exec_ADC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint8_t carry = ctx.get_flag(SREGFlag::C) ? 1 : 0;
  uint16_t sum = static_cast<uint16_t>(rd) + static_cast<uint16_t>(rr) + carry;
  uint8_t res = static_cast<uint8_t>(sum);
  ctx.write_reg(instr.Rd, res);

  bool rd3 = rd & (1 << 3);
  bool rr3 = rr & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool rr7 = rr & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (rd3 && rr3) || (rr3 && !r3) || (!r3 && rd3);
  bool V = (rd7 && rr7 && !r7) || (!rd7 && !rr7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0) && ctx.get_flag(SREGFlag::Z);
  bool C = (rd7 && rr7) || (rr7 && !r7) || (!r7 && rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

void avr::exec_SUB(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint8_t res = rd - rr;
  ctx.write_reg(instr.Rd, res);

  bool rd3 = rd & (1 << 3);
  bool rr3 = rr & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool rr7 = rr & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (!rd3 && rr3) || (rr3 && r3) || (r3 && !rd3);
  bool V = (rd7 && !rr7 && !r7) || (!rd7 && rr7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0);
  bool C = (!rd7 && rr7) || (rr7 && r7) || (r7 && !rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

void avr::exec_SBC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint8_t carry = ctx.get_flag(SREGFlag::C) ? 1 : 0;
  uint8_t res = rd - rr - carry;
  ctx.write_reg(instr.Rd, res);

  bool rd3 = rd & (1 << 3);
  bool rr3 = rr & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool rr7 = rr & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (!rd3 && rr3) || (rr3 && r3) || (r3 && !rd3);
  bool V = (rd7 && !rr7 && !r7) || (!rd7 && rr7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0) && ctx.get_flag(SREGFlag::Z);
  bool C = (!rd7 && rr7) || (rr7 && r7) || (r7 && !rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

void avr::exec_CP(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint16_t diff = static_cast<uint16_t>(rd) - static_cast<uint16_t>(rr);
  uint8_t res = static_cast<uint8_t>(diff);

  bool rd3 = rd & (1 << 3);
  bool rr3 = rr & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool rr7 = rr & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (!rd3 && rr3) || (rr3 && r3) || (r3 && !rd3);
  bool V = (rd7 && !rr7 && !r7) || (!rd7 && rr7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0);
  bool C = (!rd7 && rr7) || (rr7 && r7) || (r7 && !rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

void avr::exec_CPI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t imm = static_cast<uint8_t>(instr.imm);

  uint16_t diff = static_cast<uint16_t>(rd) - static_cast<uint16_t>(imm);
  uint8_t res = static_cast<uint8_t>(diff);

  bool rd3 = rd & (1 << 3);
  bool rr3 = imm & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool rr7 = imm & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (!rd3 && rr3) || (rr3 && r3) || (r3 && !rd3);
  bool V = (rd7 && !rr7 && !r7) || (!rd7 && rr7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0);
  bool C = (!rd7 && rr7) || (rr7 && r7) || (r7 && !rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

void avr::exec_INC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t res = rd + 1;

  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);

  bool N = r7;
  bool Z = (res == 0);
  bool V = (rd == 0x7F);
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

void avr::exec_DEC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t res = rd - 1;

  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);

  bool N = r7;
  bool Z = (res == 0);
  bool V = (rd == 0x80);
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

void avr::exec_BRNE(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  if (!ctx.get_flag(SREGFlag::Z)) {
    ctx.set_pc(ctx.pc() + instr.rel + 1);
    ctx.cpu().add_cycles(1);
  }
}

void avr::exec_BREQ(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  if (ctx.get_flag(SREGFlag::Z)) {
    ctx.set_pc(ctx.pc() + instr.rel + 1);
    ctx.cpu().add_cycles(1);
  }
}

void avr::exec_PUSH(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t val = ctx.read_reg(instr.Rd);
  ctx.push8(val);
}
void avr::exec_POP(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  ctx.write_reg(instr.Rd, ctx.pop8());
}

void avr::exec_SBI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t value = ctx.io_read(instr.io);
  value |= (1 << instr.bit);
  ctx.io_write(instr.io, value);
}

void avr::exec_CBI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t value = ctx.io_read(instr.io);
  value &= ~(1 << instr.bit);
  ctx.io_write(instr.io, value);
}

void avr::exec_AND(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint8_t res = rd & rr;
  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);

  bool N = r7;
  bool Z = (res == 0);
  bool V = false;
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

void avr::exec_OR(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint8_t res = rd | rr;
  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);

  bool N = r7;
  bool Z = (res == 0);
  bool V = false;
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

void avr::exec_EOR(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  uint8_t res = rd ^ rr;
  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);

  bool N = r7;
  bool Z = (res == 0);
  bool V = false;
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

void avr::exec_ORI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t imm = static_cast<uint8_t>(instr.imm);
  uint8_t res = rd | imm;
  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);
  bool N = r7;
  bool Z = (res == 0);
  bool V = false;
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

// При написании следующих инструкций пришли к выводу, что инструкция должна
// изменять PC, нужно ли что-то менять в других функциях или методах?

void avr::exec_BRBC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  if (!ctx.get_flag(static_cast<SREGFlag>(instr.sreg_bit))) {
    ctx.set_pc(ctx.pc() + instr.rel + 1);
    ctx.cpu().add_cycles(1);
  }
}

void avr::exec_BRCC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  if (!ctx.get_flag(SREGFlag::C)) {
    ctx.set_pc(ctx.pc() + instr.rel + 1);
    ctx.cpu().add_cycles(1);
  }
}

void avr::exec_CPSE(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t rr = ctx.read_reg(instr.Rr);

  if (rd == rr) {
    uint8_t next_word = get_next_instruction_words(ctx);
    ctx.set_pc(ctx.pc() + 1 + next_word);
    ctx.cpu().add_cycles(next_word);
  } else {
    ctx.set_pc(ctx.pc() + 1);
  }
}

void avr::exec_SBIC(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t val = ctx.io_read(instr.io);
  bool bit_set = val & (1 << instr.bit);

  if (!bit_set) {
    uint8_t next_words = get_next_instruction_words(ctx);
    ctx.set_pc(ctx.pc() + 1 + next_words);
    ctx.cpu().add_cycles(next_words);
  } else {
    ctx.set_pc(ctx.pc() + 1);
  }
}

void avr::exec_SBIS(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t val = ctx.io_read(instr.io);
  bool bit_set = val & (1 << instr.bit);

  if (bit_set) {
    uint8_t next_words = get_next_instruction_words(ctx);
    ctx.set_pc(ctx.pc() + 1 + next_words);
    ctx.cpu().add_cycles(next_words);
  } else {
    ctx.set_pc(ctx.pc() + 1);
  }
}

void avr::exec_SUBI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t imm = static_cast<uint8_t>(instr.imm);
  uint8_t res = rd - imm;
  ctx.write_reg(instr.Rd, res);

  bool rd3 = rd & (1 << 3);
  bool imm3 = imm & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool imm7 = imm & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (!rd3 && imm3) || (imm3 && r3) || (r3 && !rd3);
  bool V = (rd7 && !imm7 && !r7) || (!rd7 && imm7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0);
  bool C = (!rd7 && imm7) || (imm7 && r7) || (r7 && !rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

// SBCI – вычитание с заёмом
void avr::exec_SBCI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t imm = static_cast<uint8_t>(instr.imm);
  uint8_t carry = ctx.get_flag(SREGFlag::C) ? 1 : 0;
  uint8_t res = rd - imm - carry;
  ctx.write_reg(instr.Rd, res);

  bool rd3 = rd & (1 << 3);
  bool imm3 = imm & (1 << 3);
  bool r3 = res & (1 << 3);
  bool rd7 = rd & (1 << 7);
  bool imm7 = imm & (1 << 7);
  bool r7 = res & (1 << 7);

  bool H = (!rd3 && imm3) || (imm3 && r3) || (r3 && !rd3);
  bool V = (rd7 && !imm7 && !r7) || (!rd7 && imm7 && r7);
  bool N = r7;
  bool S = N ^ V;
  bool Z = (res == 0) && ctx.get_flag(SREGFlag::Z);
  bool C = (!rd7 && imm7) || (imm7 && r7) || (r7 && !rd7);

  ctx.set_flag(SREGFlag::H, H);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::S, S);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::C, C);
}

// ANDI – логическое И с константой
void avr::exec_ANDI(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t imm = static_cast<uint8_t>(instr.imm);
  uint8_t res = rd & imm;
  ctx.write_reg(instr.Rd, res);

  bool r7 = res & (1 << 7);
  bool N = r7;
  bool Z = (res == 0);
  bool V = false;
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::S, S);
}

// ADIW – Add Immediate to Word (работает с парами R24, R26, R28, R30)
void avr::exec_ADIW(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t lo = instr.Rd;
  uint16_t val = ctx.read_reg16(lo);  // читаем 16 бит из пары (Rd:Rd+1)
  uint16_t imm = static_cast<uint16_t>(instr.imm);
  uint16_t res = val + imm;
  ctx.write_reg16(lo, res);

  // Флаги
  bool V = ((val ^ res) & (val ^ imm) & 0x8000) != 0;  // переполнение знакового
  bool N = (res & 0x8000) != 0;
  bool Z = (res == 0);
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::S, S);
}

// SBIW – Subtract Immediate from Word
void avr::exec_SBIW(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint8_t lo = instr.Rd;
  uint16_t val = ctx.read_reg16(lo);
  uint16_t imm = static_cast<uint16_t>(instr.imm);
  uint16_t res = val - imm;
  ctx.write_reg16(lo, res);

  // Флаги (формула для вычитания)
  bool V = ((val ^ imm) & (val ^ res) & 0x8000) != 0;
  bool N = (res & 0x8000) != 0;
  bool Z = (res == 0);
  bool S = N ^ V;

  ctx.set_flag(SREGFlag::V, V);
  ctx.set_flag(SREGFlag::N, N);
  ctx.set_flag(SREGFlag::Z, Z);
  ctx.set_flag(SREGFlag::S, S);
}

void avr::exec_IJMP(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint16_t z = ctx.read_reg16(30);  // Z = R31:R30
  ctx.set_pc(z);                    // прямой word‑адрес
}

void avr::exec_ICALL(AVRExecutionContext& ctx,
                     const DecodedInstruction& instr) {
  uint16_t z = ctx.read_reg16(30);
  uint32_t return_pc = ctx.pc() + 1;  // для ICALL длина инструкции 1 слово
  ctx.push_pc(return_pc);
  ctx.set_pc(z);
}

void avr::exec_MOVW(AVRExecutionContext& ctx, const DecodedInstruction& instr) {
  uint16_t val = ctx.read_reg16(instr.Rr);  // читаем пару Rr+1:Rr
  ctx.write_reg16(instr.Rd, val);           // записываем в Rd+1:Rd
}

// LPM Rd, Z+ : Rd <- (RAMPZ:Z), Z++
void avr::exec_LPM_Zplus(AVRExecutionContext& ctx,
                         const DecodedInstruction& instr) {
  uint16_t z = ctx.read_reg16(30);  // R31:R30
  uint16_t word_addr = z >> 1;      // Z — байтовый адрес, преобразуем в word
  uint16_t word = ctx.cpu().read_program_word(word_addr);
  uint8_t byte;
  if (z & 1)  // младший байт адреса определяет, какой полубайт выбираем
    byte = static_cast<uint8_t>(word >> 8);
  else
    byte = static_cast<uint8_t>(word & 0xFF);
  ctx.write_reg(instr.Rd, byte);
  // Постинкремент Z
  ctx.write_reg16(30, z + 1);
}

// ELPM Rd, Z+ : аналогично, но с учетом RAMPZ (пока RAMPZ = 0)
void avr::exec_ELPM_Zplus(AVRExecutionContext& ctx,
                          const DecodedInstruction& instr) {
  // Для ATmega328P RAMPZ всегда 0, поэтому ELPM = LPM
  exec_LPM_Zplus(ctx, instr);
}
