#pragma once

#include <cstdint>
#include <string>

namespace avr {

struct Disassembled {
  uint8_t words; // instruction length in words (1 or 2)
  bool known; // false — the opcode does not exist on the ATmega328P (printed as
              // `.word`)
  std::string text; // for example "ldi r16, 0x20" or "rjmp 0x0054"
};

/**
 * Disassembles one ATmega328P instruction.
 *
 * @param w0        the first word of the instruction
 * @param w1        the next word (needed only for the 2-word
 * LDS/STS/JMP/CALL)
 * @param word_addr the address of the first word in **words** — needed for the
 * addresses of RJMP/RCALL/BRxx jumps
 *
 * The format is close to `avr-objdump`: aliases (`clr`, `lsl`, `sei`, `breq`,
 * …), jump addresses are **byte** addresses, as in objdump and the HEX file. An
 * unknown opcode yields `.word 0xXXXX` with a length of 1 word. The instruction
 * set matches what the emulator executes (checked by a test over all 65536
 * opcodes).
 */
Disassembled disassemble(uint16_t w0, uint16_t w1, uint32_t word_addr);

} // namespace avr
