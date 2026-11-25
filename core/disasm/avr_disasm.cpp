#include <cstdarg>
#include <cstdio>

#include "disasm/avr_disasm.h"

namespace avr {
namespace {

constexpr uint32_t PC_WORD_MASK = 0x3FFF; // ATmega328P: 16 К слов flash

std::string fmt(const char *f, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return buf;
}

// Поля опкода
int rd5(uint16_t w) { return (w >> 4) & 0x1F; }
int rr5(uint16_t w) { return ((w >> 5) & 0x10) | (w & 0x0F); }
int rd4(uint16_t w) { return 16 + ((w >> 4) & 0x0F); } // r16..r31
int k8(uint16_t w) { return ((w >> 4) & 0xF0) | (w & 0x0F); }
int io6(uint16_t w) { return ((w >> 5) & 0x30) | (w & 0x0F); }

// Байтовый адрес цели относительного перехода: слово (PC+1+k)
uint32_t rel_target(uint32_t word_addr, int k) {
  return (((word_addr + 1 + static_cast<uint32_t>(k)) & PC_WORD_MASK)) * 2;
}

Disassembled one(std::string text) { return {1, true, std::move(text)}; }
Disassembled two(std::string text) { return {2, true, std::move(text)}; }
Disassembled unknown(uint16_t w) { return {1, false, fmt(".word 0x%04X", w)}; }

const char *const SET_NAMES[8] = {"sec", "sez", "sen", "sev",
                                  "ses", "seh", "set", "sei"};
const char *const CLR_NAMES[8] = {"clc", "clz", "cln", "clv",
                                  "cls", "clh", "clt", "cli"};
const char *const BRBS_NAMES[8] = {"brcs", "breq", "brmi", "brvs",
                                   "brlt", "brhs", "brts", "brie"};
const char *const BRBC_NAMES[8] = {"brcc", "brne", "brpl", "brvc",
                                   "brge", "brhc", "brtc", "brid"};

// Двухрегистровые команды 0000..0010
// (CPC/SBC/ADD/CPSE/CP/SUB/ADC/AND/EOR/OR/MOV)
Disassembled two_reg(uint16_t w) {
  const int d = rd5(w);
  const int r = rr5(w);
  switch (w & 0xFC00) {
  case 0x0400:
    return one(fmt("cpc r%d, r%d", d, r));
  case 0x0800:
    return one(fmt("sbc r%d, r%d", d, r));
  case 0x0C00:
    return one(d == r ? fmt("lsl r%d", d) : fmt("add r%d, r%d", d, r));
  case 0x1000:
    return one(fmt("cpse r%d, r%d", d, r));
  case 0x1400:
    return one(fmt("cp r%d, r%d", d, r));
  case 0x1800:
    return one(fmt("sub r%d, r%d", d, r));
  case 0x1C00:
    return one(d == r ? fmt("rol r%d", d) : fmt("adc r%d, r%d", d, r));
  case 0x2000:
    return one(d == r ? fmt("tst r%d", d) : fmt("and r%d, r%d", d, r));
  case 0x2400:
    return one(d == r ? fmt("clr r%d", d) : fmt("eor r%d, r%d", d, r));
  case 0x2800:
    return one(fmt("or r%d, r%d", d, r));
  default:
    return one(fmt("mov r%d, r%d", d, r)); // 0x2C00
  }
}

// 0000 0001..0011: MOVW, MULS, MULSU/FMUL*
Disassembled mul_group(uint16_t w) {
  switch (w & 0xFF00) {
  case 0x0100:
    return one(fmt("movw r%d, r%d", ((w >> 4) & 0x0F) * 2, (w & 0x0F) * 2));
  case 0x0200:
    return one(fmt("muls r%d, r%d", rd4(w), 16 + (w & 0x0F)));
  default: { // 0x0300
    const int d = 16 + ((w >> 4) & 0x07);
    const int r = 16 + (w & 0x07);
    switch (w & 0x88) {
    case 0x00:
      return one(fmt("mulsu r%d, r%d", d, r));
    case 0x08:
      return one(fmt("fmul r%d, r%d", d, r));
    case 0x80:
      return one(fmt("fmuls r%d, r%d", d, r));
    default:
      return one(fmt("fmulsu r%d, r%d", d, r));
    }
  }
  }
}

// 10q0 qqsd dddd yqqq: LDD/STD (и LD/ST Y/Z без смещения при q == 0)
Disassembled ldd_std(uint16_t w) {
  const int q = ((w >> 8) & 0x20) | ((w >> 7) & 0x18) | (w & 0x07);
  const char y = (w & 0x08) ? 'Y' : 'Z';
  const int d = rd5(w);
  const bool store = (w & 0x0200) != 0;
  std::string ptr = q == 0 ? fmt("%c", y) : fmt("%c+%d", y, q);
  return one(store ? fmt("st %s, r%d", ptr.c_str(), d)
                   : fmt("ld r%d, %s", d, ptr.c_str()));
}

// 1001 000d / 1001 001d: LDS/STS, LD/ST с постинкрементом, LPM, POP/PUSH
Disassembled load_store(uint16_t w, uint16_t w1) {
  const int d = rd5(w);
  const bool store = (w & 0x0200) != 0;
  const int op = w & 0x0F;
  const char *ptr = nullptr;
  switch (op) {
  case 0x0:
    return store ? two(fmt("sts 0x%04X, r%d", w1, d))
                 : two(fmt("lds r%d, 0x%04X", d, w1));
  case 0x1:
    ptr = "Z+";
    break;
  case 0x2:
    ptr = "-Z";
    break;
  case 0x9:
    ptr = "Y+";
    break;
  case 0xA:
    ptr = "-Y";
    break;
  case 0xC:
    ptr = "X";
    break;
  case 0xD:
    ptr = "X+";
    break;
  case 0xE:
    ptr = "-X";
    break;
  case 0xF:
    return one(store ? fmt("push r%d", d) : fmt("pop r%d", d));
  case 0x4: // LPM Rd,Z
  case 0x5: // LPM Rd,Z+
    if (!store)
      return one(fmt("lpm r%d, %s", d, op == 0x4 ? "Z" : "Z+"));
    return unknown(w);
  default:
    return unknown(w); // ELPM, XCH/LAS/LAC/LAT — нет на ATmega328P
  }
  return one(store ? fmt("st %s, r%d", ptr, d) : fmt("ld r%d, %s", d, ptr));
}

// 1001 010x xxxx xxxx: однооперандные, флаги, ret/reti/sleep/…, jmp/call
Disassembled misc(uint16_t w, uint16_t w1) {
  const int d = rd5(w);
  switch (w & 0xFE0F) {
  case 0x9400:
    return one(fmt("com r%d", d));
  case 0x9401:
    return one(fmt("neg r%d", d));
  case 0x9402:
    return one(fmt("swap r%d", d));
  case 0x9403:
    return one(fmt("inc r%d", d));
  case 0x9405:
    return one(fmt("asr r%d", d));
  case 0x9406:
    return one(fmt("lsr r%d", d));
  case 0x9407:
    return one(fmt("ror r%d", d));
  case 0x940A:
    return one(fmt("dec r%d", d));
  default:
    break;
  }
  if ((w & 0xFF8F) == 0x9408)
    return one(SET_NAMES[(w >> 4) & 7]);
  if ((w & 0xFF8F) == 0x9488)
    return one(CLR_NAMES[(w >> 4) & 7]);
  switch (w) {
  case 0x9409:
    return one("ijmp");
  case 0x9509:
    return one("icall");
  case 0x9508:
    return one("ret");
  case 0x9518:
    return one("reti");
  case 0x9588:
    return one("sleep");
  case 0x9598:
    return one("break");
  case 0x95A8:
    return one("wdr");
  case 0x95C8:
    return one("lpm");
  case 0x95E8:
    return one("spm");
  default:
    break;
  }
  if ((w & 0xFE0E) == 0x940C) { // JMP k (22 бита, адрес в словах)
    const uint32_t k = ((((w >> 4) & 0x1F) << 1 | (w & 1)) << 16) | w1;
    return two(fmt("jmp 0x%04X", ((k * 2) & (PC_WORD_MASK * 2 + 1))));
  }
  if ((w & 0xFE0E) == 0x940E) {
    const uint32_t k = ((((w >> 4) & 0x1F) << 1 | (w & 1)) << 16) | w1;
    return two(fmt("call 0x%04X", ((k * 2) & (PC_WORD_MASK * 2 + 1))));
  }
  return unknown(w); // EIJMP/EICALL/DES/ELPM…
}

} // namespace

Disassembled disassemble(uint16_t w, uint16_t w1, uint32_t word_addr) {
  if (w == 0x0000)
    return one("nop");

  switch (w >> 12) {
  case 0x0:
    if ((w & 0xFC00) == 0x0000) // 0000 00xx: MOVW/MULS/…; 0x00xx ≠ 0 — нет
      return (w & 0xFF00) == 0x0000 ? unknown(w) : mul_group(w);
    return two_reg(w);
  case 0x1:
  case 0x2:
    return two_reg(w);
  case 0x3:
    return one(fmt("cpi r%d, 0x%02X", rd4(w), k8(w)));
  case 0x4:
    return one(fmt("sbci r%d, 0x%02X", rd4(w), k8(w)));
  case 0x5:
    return one(fmt("subi r%d, 0x%02X", rd4(w), k8(w)));
  case 0x6:
    return one(fmt("ori r%d, 0x%02X", rd4(w), k8(w)));
  case 0x7:
    return one(fmt("andi r%d, 0x%02X", rd4(w), k8(w)));
  case 0x8:
  case 0xA:
    return ldd_std(w);
  case 0x9:
    switch ((w >> 8) & 0x0F) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
      return load_store(w, w1);
    case 0x4:
    case 0x5:
      return misc(w, w1);
    case 0x6:
    case 0x7: {
      const int d = 24 + 2 * ((w >> 4) & 3);
      const int k = ((w >> 2) & 0x30) | (w & 0x0F);
      return one(fmt("%s r%d, 0x%02X", (w & 0x0100) ? "sbiw" : "adiw", d, k));
    }
    case 0x8:
    case 0x9:
    case 0xA:
    case 0xB: {
      static const char *const names[4] = {"cbi", "sbic", "sbi", "sbis"};
      return one(
          fmt("%s 0x%02X, %d", names[(w >> 8) & 3], (w >> 3) & 0x1F, w & 7));
    }
    default:
      return one(fmt("mul r%d, r%d", rd5(w), rr5(w))); // 0x9C..0x9F
    }
  case 0xB: {
    const int a = io6(w);
    return (w & 0x0800) ? one(fmt("out 0x%02X, r%d", a, rd5(w)))
                        : one(fmt("in r%d, 0x%02X", rd5(w), a));
  }
  case 0xC:
  case 0xD: {
    int k = w & 0x0FFF;
    if (k & 0x0800)
      k -= 0x1000;
    return one(fmt("%s 0x%04X", (w >> 12) == 0xC ? "rjmp" : "rcall",
                   rel_target(word_addr, k)));
  }
  case 0xE:
    return k8(w) == 0xFF && rd4(w) >= 16
               ? one(fmt("ser r%d", rd4(w)))
               : one(fmt("ldi r%d, 0x%02X", rd4(w), k8(w)));
  default: {                 // 0xF
    if ((w & 0x0800) == 0) { // BRBS / BRBC
      int k = (w >> 3) & 0x7F;
      if (k & 0x40)
        k -= 0x80;
      const int s = w & 7;
      const char *name = (w & 0x0400) ? BRBC_NAMES[s] : BRBS_NAMES[s];
      return one(fmt("%s 0x%04X", name, rel_target(word_addr, k)));
    }
    if ((w & 0x0008) != 0)
      return unknown(w);
    static const char *const names[4] = {"bld", "bst", "sbrc", "sbrs"};
    return one(fmt("%s r%d, %d", names[(w >> 9) & 3], rd5(w), w & 7));
  }
  }
}

} // namespace avr
