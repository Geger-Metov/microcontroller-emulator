#include "cpu/avr_instructions.h"
#include "cpu/AVRExecutionContext.h"
#include "decoder/avr_decode_helpers.h"

// Flag formulas are taken from the AVR Instruction Set Manual (DS40002198).
// Tests check them against an independent oracle on wide arithmetic.

namespace {

using avr::DecodedInstruction;

inline bool bit(unsigned v, unsigned n) { return (v >> n) & 1u; }

/// N, Z, V, S for a result (S = N xor V).
void set_nzvs(AVRExecutionContext &ctx, uint8_t res, bool v) {
  bool n = bit(res, 7);
  ctx.set_flag(SREGFlag::N, n);
  ctx.set_flag(SREGFlag::Z, res == 0);
  ctx.set_flag(SREGFlag::V, v);
  ctx.set_flag(SREGFlag::S, n != v);
}

uint8_t add8(AVRExecutionContext &ctx, uint8_t rd, uint8_t rr, uint8_t cin) {
  uint8_t res = static_cast<uint8_t>(rd + rr + cin);
  bool rd3 = bit(rd, 3), rr3 = bit(rr, 3), r3 = bit(res, 3);
  bool rd7 = bit(rd, 7), rr7 = bit(rr, 7), r7 = bit(res, 7);

  ctx.set_flag(SREGFlag::H, (rd3 && rr3) || (rr3 && !r3) || (!r3 && rd3));
  ctx.set_flag(SREGFlag::C, (rd7 && rr7) || (rr7 && !r7) || (!r7 && rd7));
  set_nzvs(ctx, res, (rd7 && rr7 && !r7) || (!rd7 && !rr7 && r7));
  return res;
}

/// Subtraction rd - rr - cin. keep_z: sticky Z (SBC/SBCI/CPC).
uint8_t sub8(AVRExecutionContext &ctx, uint8_t rd, uint8_t rr, uint8_t cin,
             bool keep_z) {
  bool old_z = ctx.get_flag(SREGFlag::Z);
  uint8_t res = static_cast<uint8_t>(rd - rr - cin);
  bool rd3 = bit(rd, 3), rr3 = bit(rr, 3), r3 = bit(res, 3);
  bool rd7 = bit(rd, 7), rr7 = bit(rr, 7), r7 = bit(res, 7);

  ctx.set_flag(SREGFlag::H, (!rd3 && rr3) || (rr3 && r3) || (r3 && !rd3));
  ctx.set_flag(SREGFlag::C, (!rd7 && rr7) || (rr7 && r7) || (r7 && !rd7));
  set_nzvs(ctx, res, (rd7 && !rr7 && !r7) || (!rd7 && rr7 && r7));
  if (keep_z)
    ctx.set_flag(SREGFlag::Z, res == 0 && old_z);
  return res;
}

uint8_t carry_in(const AVRExecutionContext &ctx) {
  return ctx.get_flag(SREGFlag::C) ? 1 : 0;
}

/// Logical operations: V = 0, C is unchanged.
void write_logic(AVRExecutionContext &ctx, uint8_t reg, uint8_t res) {
  ctx.write_reg(reg, res);
  set_nzvs(ctx, res, false);
}

/// Conditional skip of the next instruction (CPSE/SBRC/SBRS/SBIC/SBIS):
/// +1 cycle for a skipped word, +2 if a 2-word instruction is skipped.
void skip_next(AVRExecutionContext &ctx) {
  uint8_t words = avr::get_next_instruction_words(ctx);
  ctx.set_pc(ctx.pc() + 1 + words);
  ctx.cpu().add_cycles(words);
}

void take_branch(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.set_pc(ctx.pc() + instr.rel + 1);
  ctx.cpu().add_cycles(1);
}

uint8_t read_flash_byte(AVRExecutionContext &ctx, uint16_t byte_addr) {
  uint16_t word = ctx.fetch_program_word(byte_addr >> 1);
  return static_cast<uint8_t>((byte_addr & 1) ? (word >> 8) : (word & 0xFF));
}

/// Common part of right shifts: C = old bit0, N, Z, V = N xor C.
void finish_shift_right(AVRExecutionContext &ctx, uint8_t reg, uint8_t old,
                        uint8_t res) {
  ctx.write_reg(reg, res);
  bool c = bit(old, 0);
  ctx.set_flag(SREGFlag::C, c);
  set_nzvs(ctx, res, bit(res, 7) != c);
}

} // namespace

// ---------------------------------------------------------------- Arithmetic

void avr::exec_ADD(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd,
                add8(ctx, ctx.read_reg(instr.Rd), ctx.read_reg(instr.Rr), 0));
}

void avr::exec_ADC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  // Z for ADC is plain (not sticky): Z = (result == 0).
  ctx.write_reg(instr.Rd, add8(ctx, ctx.read_reg(instr.Rd),
                               ctx.read_reg(instr.Rr), carry_in(ctx)));
}

void avr::exec_SUB(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, sub8(ctx, ctx.read_reg(instr.Rd),
                               ctx.read_reg(instr.Rr), 0, false));
}

void avr::exec_SBC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, sub8(ctx, ctx.read_reg(instr.Rd),
                               ctx.read_reg(instr.Rr), carry_in(ctx), true));
}

void avr::exec_CP(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  sub8(ctx, ctx.read_reg(instr.Rd), ctx.read_reg(instr.Rr), 0, false);
}

void avr::exec_CPC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  sub8(ctx, ctx.read_reg(instr.Rd), ctx.read_reg(instr.Rr), carry_in(ctx),
       true);
}

void avr::exec_CPI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  sub8(ctx, ctx.read_reg(instr.Rd), static_cast<uint8_t>(instr.imm), 0, false);
}

void avr::exec_SUBI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, sub8(ctx, ctx.read_reg(instr.Rd),
                               static_cast<uint8_t>(instr.imm), 0, false));
}

void avr::exec_SBCI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd,
                sub8(ctx, ctx.read_reg(instr.Rd),
                     static_cast<uint8_t>(instr.imm), carry_in(ctx), true));
}

void avr::exec_INC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t res = rd + 1;
  ctx.write_reg(instr.Rd, res);
  set_nzvs(ctx, res, rd == 0x7F);
}

void avr::exec_DEC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t res = rd - 1;
  ctx.write_reg(instr.Rd, res);
  set_nzvs(ctx, res, rd == 0x80);
}

void avr::exec_COM(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t res = ~ctx.read_reg(instr.Rd);
  write_logic(ctx, instr.Rd, res);
  ctx.set_flag(SREGFlag::C, true);
}

void avr::exec_NEG(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t res = static_cast<uint8_t>(0 - rd);
  ctx.write_reg(instr.Rd, res);
  ctx.set_flag(SREGFlag::H, bit(res, 3) || bit(rd, 3));
  ctx.set_flag(SREGFlag::C, res != 0);
  set_nzvs(ctx, res, res == 0x80);
}

void avr::exec_SWAP(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  ctx.write_reg(instr.Rd, static_cast<uint8_t>((rd << 4) | (rd >> 4)));
}

void avr::exec_LSR(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  finish_shift_right(ctx, instr.Rd, rd, static_cast<uint8_t>(rd >> 1));
}

void avr::exec_ASR(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  finish_shift_right(ctx, instr.Rd, rd,
                     static_cast<uint8_t>((rd >> 1) | (rd & 0x80)));
}

void avr::exec_ROR(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t res = static_cast<uint8_t>((rd >> 1) | (carry_in(ctx) << 7));
  finish_shift_right(ctx, instr.Rd, rd, res);
}

void avr::exec_AND(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  write_logic(ctx, instr.Rd, ctx.read_reg(instr.Rd) & ctx.read_reg(instr.Rr));
}

void avr::exec_OR(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  write_logic(ctx, instr.Rd, ctx.read_reg(instr.Rd) | ctx.read_reg(instr.Rr));
}

void avr::exec_EOR(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  write_logic(ctx, instr.Rd, ctx.read_reg(instr.Rd) ^ ctx.read_reg(instr.Rr));
}

void avr::exec_ANDI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  write_logic(ctx, instr.Rd,
              ctx.read_reg(instr.Rd) & static_cast<uint8_t>(instr.imm));
}

void avr::exec_ORI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  write_logic(ctx, instr.Rd,
              ctx.read_reg(instr.Rd) | static_cast<uint8_t>(instr.imm));
}

void avr::exec_ADIW(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint16_t val = ctx.read_reg16(instr.Rd);
  uint16_t res = static_cast<uint16_t>(val + instr.imm);
  ctx.write_reg16(instr.Rd, res);

  bool rdh7 = bit(val, 15), r15 = bit(res, 15);
  bool v = !rdh7 && r15;
  bool n = r15;
  ctx.set_flag(SREGFlag::C, !r15 && rdh7);
  ctx.set_flag(SREGFlag::V, v);
  ctx.set_flag(SREGFlag::N, n);
  ctx.set_flag(SREGFlag::Z, res == 0);
  ctx.set_flag(SREGFlag::S, n != v);
}

void avr::exec_SBIW(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint16_t val = ctx.read_reg16(instr.Rd);
  uint16_t res = static_cast<uint16_t>(val - instr.imm);
  ctx.write_reg16(instr.Rd, res);

  bool rdh7 = bit(val, 15), r15 = bit(res, 15);
  bool v = rdh7 && !r15;
  bool n = r15;
  ctx.set_flag(SREGFlag::C, r15 && !rdh7);
  ctx.set_flag(SREGFlag::V, v);
  ctx.set_flag(SREGFlag::N, n);
  ctx.set_flag(SREGFlag::Z, res == 0);
  ctx.set_flag(SREGFlag::S, n != v);
}

// ------------------------------------------------------------ Multiplication

namespace {
/// Write R1:R0 and the flags C (product bit 15) and Z.
void finish_mul(AVRExecutionContext &ctx, uint16_t product, bool fractional) {
  uint16_t res = fractional ? static_cast<uint16_t>(product << 1) : product;
  ctx.write_reg16(0, res);
  ctx.set_flag(SREGFlag::C, bit(product, 15));
  ctx.set_flag(SREGFlag::Z, res == 0);
}

int8_t s8(uint8_t v) { return static_cast<int8_t>(v); }
} // namespace

void avr::exec_MUL(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  finish_mul(
      ctx,
      static_cast<uint16_t>(ctx.read_reg(instr.Rd) * ctx.read_reg(instr.Rr)),
      false);
}

void avr::exec_MULS(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  int p = s8(ctx.read_reg(instr.Rd)) * s8(ctx.read_reg(instr.Rr));
  finish_mul(ctx, static_cast<uint16_t>(p), false);
}

void avr::exec_MULSU(AVRExecutionContext &ctx,
                     const DecodedInstruction &instr) {
  int p = s8(ctx.read_reg(instr.Rd)) * ctx.read_reg(instr.Rr);
  finish_mul(ctx, static_cast<uint16_t>(p), false);
}

void avr::exec_FMUL(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  finish_mul(
      ctx,
      static_cast<uint16_t>(ctx.read_reg(instr.Rd) * ctx.read_reg(instr.Rr)),
      true);
}

void avr::exec_FMULS(AVRExecutionContext &ctx,
                     const DecodedInstruction &instr) {
  int p = s8(ctx.read_reg(instr.Rd)) * s8(ctx.read_reg(instr.Rr));
  finish_mul(ctx, static_cast<uint16_t>(p), true);
}

void avr::exec_FMULSU(AVRExecutionContext &ctx,
                      const DecodedInstruction &instr) {
  int p = s8(ctx.read_reg(instr.Rd)) * ctx.read_reg(instr.Rr);
  finish_mul(ctx, static_cast<uint16_t>(p), true);
}

// -------------------------------------------------------------------- Moves

void avr::exec_MOV(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, ctx.read_reg(instr.Rr));
}

void avr::exec_MOVW(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg16(instr.Rd, ctx.read_reg16(instr.Rr));
}

void avr::exec_LDI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, static_cast<uint8_t>(instr.imm));
}

void avr::exec_OUT(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.io_write(static_cast<uint8_t>(instr.imm), ctx.read_reg(instr.Rr));
}

void avr::exec_IN(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, ctx.io_read(static_cast<uint8_t>(instr.imm)));
}

void avr::exec_PUSH(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.push8(ctx.read_reg(instr.Rd));
}

void avr::exec_POP(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, ctx.pop8());
}

void avr::exec_LD(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint16_t p = ctx.read_reg16(instr.ptr);
  if (instr.mode == 2) { // -X / -Y / -Z
    --p;
    ctx.write_reg16(instr.ptr, p);
  }
  uint16_t addr = static_cast<uint16_t>(instr.mode == 0 ? p + instr.imm : p);
  uint8_t val = ctx.data_read(addr);
  if (instr.mode == 1) // X+ / Y+ / Z+
    ctx.write_reg16(instr.ptr, static_cast<uint16_t>(p + 1));
  ctx.write_reg(instr.Rd, val);
}

void avr::exec_ST(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint16_t p = ctx.read_reg16(instr.ptr);
  if (instr.mode == 2) {
    --p;
    ctx.write_reg16(instr.ptr, p);
  }
  uint16_t addr = static_cast<uint16_t>(instr.mode == 0 ? p + instr.imm : p);
  // The value is read after the pointer may have changed (ST X+, r26 — UB).
  ctx.data_write(addr, ctx.read_reg(instr.Rd));
  if (instr.mode == 1)
    ctx.write_reg16(instr.ptr, static_cast<uint16_t>(p + 1));
}

void avr::exec_LDS(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.write_reg(instr.Rd, ctx.data_read(static_cast<uint16_t>(instr.imm)));
}

void avr::exec_STS(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.data_write(static_cast<uint16_t>(instr.imm), ctx.read_reg(instr.Rd));
}

// LPM Rd, Z / LPM Rd, Z+ (the mode comes from the decoder: 0 or 1).
void avr::exec_LPM(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint16_t z = ctx.read_reg16(30);
  uint8_t byte = read_flash_byte(ctx, z);
  if (instr.mode == 1)
    ctx.write_reg16(30, static_cast<uint16_t>(z + 1));
  ctx.write_reg(instr.Rd, byte);
}

// LPM (no operands): R0 <- (Z)
void avr::exec_LPM_R0(AVRExecutionContext &ctx, const DecodedInstruction &) {
  ctx.write_reg(0, read_flash_byte(ctx, ctx.read_reg16(30)));
}

// ------------------------------------------------------------- Jumps and
// calls

void avr::exec_RJMP(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.set_pc(ctx.pc() + instr.rel + 1);
}

void avr::exec_JMP(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.set_pc(instr.imm);
}

void avr::exec_IJMP(AVRExecutionContext &ctx, const DecodedInstruction &) {
  ctx.set_pc(ctx.read_reg16(30));
}

void avr::exec_RCALL(AVRExecutionContext &ctx,
                     const DecodedInstruction &instr) {
  ctx.push_pc(ctx.pc() + 1);
  ctx.set_pc(ctx.pc() + instr.rel + 1);
}

void avr::exec_CALL(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.push_pc(ctx.pc() + 2);
  ctx.set_pc(instr.imm);
}

void avr::exec_ICALL(AVRExecutionContext &ctx, const DecodedInstruction &) {
  uint16_t z = ctx.read_reg16(30);
  ctx.push_pc(ctx.pc() + 1);
  ctx.set_pc(z);
}

void avr::exec_RET(AVRExecutionContext &ctx, const DecodedInstruction &) {
  ctx.set_pc(ctx.pop_pc());
}

void avr::exec_RETI(AVRExecutionContext &ctx, const DecodedInstruction &) {
  ctx.set_pc(ctx.pop_pc());
  ctx.set_flag(SREGFlag::I, true);
  // One instruction after RETI executes before the next interrupt.
  ctx.cpu().inhibit_interrupts_for_next_instruction();
}

void avr::exec_BRBS(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (ctx.get_flag(static_cast<SREGFlag>(instr.sreg_bit)))
    take_branch(ctx, instr);
}

void avr::exec_BRBC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (!ctx.get_flag(static_cast<SREGFlag>(instr.sreg_bit)))
    take_branch(ctx, instr);
}

// ------------------------------------------------------------ Conditional
// skips

void avr::exec_CPSE(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (ctx.read_reg(instr.Rd) == ctx.read_reg(instr.Rr))
    skip_next(ctx);
}

void avr::exec_SBRC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (!bit(ctx.read_reg(instr.Rd), instr.bit))
    skip_next(ctx);
}

void avr::exec_SBRS(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (bit(ctx.read_reg(instr.Rd), instr.bit))
    skip_next(ctx);
}

void avr::exec_SBIC(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (!bit(ctx.io_read(instr.io), instr.bit))
    skip_next(ctx);
}

void avr::exec_SBIS(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  if (bit(ctx.io_read(instr.io), instr.bit))
    skip_next(ctx);
}

// ---------------------------------------------------------- Bits and system

void avr::exec_SBI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.io_write(instr.io, ctx.io_read(instr.io) | (1u << instr.bit));
}

void avr::exec_CBI(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.io_write(instr.io, ctx.io_read(instr.io) & ~(1u << instr.bit));
}

// BSET s (SEC, SEZ, ..., SEI) and BCLR s (CLC, ..., CLI): mode 1 = set.
void avr::exec_BSET(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.set_flag(static_cast<SREGFlag>(instr.sreg_bit), true);
  if (instr.sreg_bit == 7) // SEI: the next instruction manages to execute
    ctx.cpu().inhibit_interrupts_for_next_instruction();
}

void avr::exec_BCLR(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.set_flag(static_cast<SREGFlag>(instr.sreg_bit), false);
}

void avr::exec_BLD(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  uint8_t rd = ctx.read_reg(instr.Rd);
  uint8_t mask = static_cast<uint8_t>(1u << instr.bit);
  rd = ctx.get_flag(SREGFlag::T) ? (rd | mask) : (rd & ~mask);
  ctx.write_reg(instr.Rd, rd);
}

void avr::exec_BST(AVRExecutionContext &ctx, const DecodedInstruction &instr) {
  ctx.set_flag(SREGFlag::T, bit(ctx.read_reg(instr.Rd), instr.bit));
}

void avr::exec_NOP(AVRExecutionContext &, const DecodedInstruction &) {}

// SLEEP: the core halts until the nearest interrupt (see AVR_CPU::step).
void avr::exec_SLEEP(AVRExecutionContext &ctx, const DecodedInstruction &) {
  ctx.cpu().enter_sleep();
}

// SPM (writing to flash) is not modeled: executes as a NOP.
void avr::exec_SPM(AVRExecutionContext &, const DecodedInstruction &) {}
