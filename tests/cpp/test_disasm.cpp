// Disassembler: known strings (in avr-objdump format) and consistency with
// the decoder/executor over all 65536 opcodes.
#include <string>

#include "cpu/avr_instructions.h"
#include "decoder/avr_decoder.h"
#include "disasm/avr_disasm.h"
#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

namespace {
std::string dis(uint16_t w, uint16_t w1 = 0, uint32_t addr = 0) {
  return avr::disassemble(w, w1, addr).text;
}
} // namespace

TEST(disasm_register_and_immediate_forms) {
  CHECK_EQ(dis(0x0000), "nop");
  CHECK_EQ(dis(ldi(16, 0x20)), "ldi r16, 0x20");
  CHECK_EQ(dis(ldi(31, 0xFF)), "ser r31");
  CHECK_EQ(dis(rd_rr(0x0C00, 1, 2)), "add r1, r2");
  CHECK_EQ(dis(rd_rr(0x0C00, 5, 5)), "lsl r5");
  CHECK_EQ(dis(rd_rr(0x1C00, 5, 5)), "rol r5");
  CHECK_EQ(dis(rd_rr(0x2400, 1, 1)), "clr r1");
  CHECK_EQ(dis(rd_rr(0x2000, 7, 7)), "tst r7");
  CHECK_EQ(dis(rd_rr(0x2C00, 17, 30)), "mov r17, r30");
  CHECK_EQ(dis(rd_k(0x3000, 24, 0x2A)), "cpi r24, 0x2A");
  CHECK_EQ(dis(rd_k(0x5000, 16, 1)), "subi r16, 0x01");
  CHECK_EQ(dis(unary(0x9403, 24)), "inc r24");
  CHECK_EQ(dis(0x0112), "movw r2, r4");
  CHECK_EQ(dis(0x9C01 | (1 << 4)), "mul r1, r1");
}

TEST(disasm_io_flags_and_misc) {
  CHECK_EQ(dis(out(0x05, 24)), "out 0x05, r24");
  CHECK_EQ(dis(0xB1B3), "in r27, 0x03");
  CHECK_EQ(dis(0x9478), "sei");
  CHECK_EQ(dis(0x94F8), "cli");
  CHECK_EQ(dis(0x9488), "clc");
  CHECK_EQ(dis(0x9508), "ret");
  CHECK_EQ(dis(0x9518), "reti");
  CHECK_EQ(dis(0x9588), "sleep");
  CHECK_EQ(dis(0x9A2D), "sbi 0x05, 5");
  CHECK_EQ(dis(0x982D), "cbi 0x05, 5");
  CHECK_EQ(dis(0x9601), "adiw r24, 0x01");
  CHECK_EQ(dis(0x9701), "sbiw r24, 0x01");
  CHECK_EQ(dis(0xF800), "bld r0, 0");
  CHECK_EQ(dis(0xFA07), "bst r0, 7");
  CHECK_EQ(dis(0xFC00), "sbrc r0, 0");
  CHECK_EQ(dis(0xFE00), "sbrs r0, 0");
}

TEST(disasm_load_store_forms) {
  CHECK_EQ(dis(0x9000 | (24 << 4) | 0xC), "ld r24, X");
  CHECK_EQ(dis(0x9000 | (24 << 4) | 0xD), "ld r24, X+");
  CHECK_EQ(dis(0x9000 | (24 << 4) | 0xE), "ld r24, -X");
  CHECK_EQ(dis(0x9200 | (3 << 4) | 0x1), "st Z+, r3");
  CHECK_EQ(dis(0x9200 | (3 << 4) | 0xA), "st -Y, r3");
  CHECK_EQ(dis(0x8000 | (5 << 4)), "ld r5, Z");
  CHECK_EQ(dis(0x8008 | (5 << 4)), "ld r5, Y");
  CHECK_EQ(dis(0x8009 | (5 << 4)), "ld r5, Y+1" /* LDD Rd, Y+q */);
  CHECK_EQ(dis(0x8200 | (5 << 4) | 0x0A), "st Y+2, r5");
  CHECK_EQ(dis(0x9004 | (7 << 4)), "lpm r7, Z");
  CHECK_EQ(dis(0x9005 | (7 << 4)), "lpm r7, Z+");
  CHECK_EQ(dis(0x900F | (2 << 4)), "pop r2");
  CHECK_EQ(dis(0x920F | (2 << 4)), "push r2");
}

TEST(disasm_two_word_instructions) {
  auto lds = avr::disassemble(0x9000 | (16 << 4), 0x0100, 0);
  CHECK_EQ(lds.words, 2u);
  CHECK_EQ(lds.text, "lds r16, 0x0100");
  CHECK_EQ(dis(0x9200 | (16 << 4), 0x0123), "sts 0x0123, r16");
  CHECK_EQ(dis(0x940C, 0x0034), "jmp 0x0068"); // word 0x34 -> byte 0x68
  CHECK_EQ(dis(0x940E, 0x0034), "call 0x0068");
  CHECK_EQ(avr::disassemble(0x940C, 0x0034, 0).words, 2u);
}

TEST(disasm_relative_targets_are_byte_addresses) {
  // RJMP .-1 at word 0x10: target = word 0x10 -> byte 0x20
  CHECK_EQ(dis(rjmp(-1), 0, 0x10), "rjmp 0x0020");
  CHECK_EQ(dis(rjmp(0), 0, 0x10), "rjmp 0x0022"); // to the next word
  CHECK_EQ(dis(0xD000 | 5, 0, 0), "rcall 0x000C");
  // BRNE .-4 at word 0x2B (byte 0x56): target word 0x28 -> 0x50
  CHECK_EQ(dis(0xF401 | ((-4 & 0x7F) << 3), 0, 0x2B), "brne 0x0050");
  CHECK_EQ(dis(0xF001, 0, 0), "breq 0x0002");
  // A "backward" jump through zero wraps around within flash.
  CHECK_EQ(dis(rjmp(-2), 0, 0), "rjmp 0x7FFE");
}

TEST(disasm_unknown_opcode_is_dot_word) {
  auto d = avr::disassemble(0x95D8, 0, 0); // ELPM — not on the ATmega328P
  CHECK(!d.known);
  CHECK_EQ(d.words, 1u);
  CHECK_EQ(d.text, ".word 0x95D8");
}

TEST(disasm_agrees_with_decoder_on_all_65536_opcodes) {
  AVRDecoder dec(avr::avre_plus_instruction_table,
                 avr::avre_plus_instruction_table_size);
  avr::DecodedInstruction d;
  for (uint32_t op = 0; op <= 0xFFFF; ++op) {
    const uint16_t w = static_cast<uint16_t>(op);
    const avr::InstructionDesc *desc = dec.decode(w, d);
    const avr::Disassembled dis = avr::disassemble(w, 0, 0);
    if ((desc != nullptr) != dis.known) {
      char buf[96];
      std::snprintf(buf, sizeof buf,
                    "opcode 0x%04X: decoder %s, disasm %s (%s)", w,
                    desc ? "knows" : "rejects", dis.known ? "knows" : "rejects",
                    dis.text.c_str());
      throw mctest::Failure{buf};
    }
    if (desc && desc->length != dis.words) {
      char buf[96];
      std::snprintf(buf, sizeof buf, "opcode 0x%04X: length %u vs %u", w,
                    desc->length, dis.words);
      throw mctest::Failure{buf};
    }
  }
}
