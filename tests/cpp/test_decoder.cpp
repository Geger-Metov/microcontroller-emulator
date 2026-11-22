// Тесты декодирования операндов и исполнения переходов.
// Регрессия: смещения переходов раньше декодировались без знака, из-за чего
// любой переход назад уходил вперёд (см. аудит, D-02).
#include <memory>
#include <vector>

#include "../../core/decoder/avr_decode_helpers.h"
#include "../../core/facade/EmulatorFacade.h"
#include "../../core/mcu/avr/ATmega328p.h"
#include "test_framework.h"

namespace {

int32_t rel7(uint16_t opcode) {
  avr::DecodedInstruction d;
  avr::decode_rel7(opcode, d);
  return d.rel;
}

int32_t rel12(uint16_t opcode) {
  avr::DecodedInstruction d;
  avr::decode_rel12(opcode, d);
  return d.rel;
}

// BRNE = 1111 01kk kkkk k001
uint16_t brne(int k) { return 0xF401 | ((k & 0x7F) << 3); }
// BRBC = 1111 01kk kkkk ksss
uint16_t brbc(int k, int s) { return 0xF400 | ((k & 0x7F) << 3) | s; }
// RJMP = 1100 kkkk kkkk kkkk
uint16_t rjmp(int k) { return 0xC000 | (k & 0x0FFF); }

} // namespace

TEST(decode_rel7_is_signed) {
  CHECK_EQ(rel7(brne(0)), 0);
  CHECK_EQ(rel7(brne(1)), 1);
  CHECK_EQ(rel7(brne(63)), 63);
  CHECK_EQ(rel7(brne(-64)), -64);
  CHECK_EQ(rel7(brne(-4)), -4);
  CHECK_EQ(rel7(brne(-1)), -1);
  // Реальный опкод из firmware.hex: BRNE .-4
  CHECK_EQ(rel7(0xF7E1), -4);
}

TEST(decode_rel7_s3_is_signed) {
  avr::DecodedInstruction d;
  avr::decode_rel7_s3(brbc(-2, 5), d);
  CHECK_EQ(d.rel, -2);
  CHECK_EQ(d.sreg_bit, 5);

  avr::decode_rel7_s3(brbc(10, 3), d);
  CHECK_EQ(d.rel, 10);
  CHECK_EQ(d.sreg_bit, 3);
}

TEST(decode_rel12_is_signed) {
  CHECK_EQ(rel12(rjmp(0)), 0);
  CHECK_EQ(rel12(rjmp(1)), 1);
  CHECK_EQ(rel12(rjmp(2047)), 2047);
  CHECK_EQ(rel12(rjmp(-1)), -1);
  CHECK_EQ(rel12(rjmp(-2048)), -2048);
  CHECK_EQ(rel12(0xCFFF), -1); // rjmp .-0 (бесконечный цикл на месте)
}

TEST(backward_branch_and_rjmp_execute_correctly) {
  auto emu = ATmega328P::create();
  // 0: ldi r16, 3
  // 1: dec r16
  // 2: brne .-2      (обратно на dec, пока r16 != 0)
  // 3: rjmp .-1      (бесконечный цикл на месте)
  emu->program_flash(0, {0xE003, 0x950A, brne(-2), rjmp(-1)});
  emu->reset();

  emu->step_instruction(); // ldi
  CHECK_EQ(emu->get_pc(), 1u);
  emu->step_instruction(); // dec -> r16 = 2
  emu->step_instruction(); // brne, взят назад
  CHECK_EQ(emu->get_pc(), 1u);
  emu->step_instruction(); // dec -> 1
  emu->step_instruction(); // brne, взят назад
  CHECK_EQ(emu->get_pc(), 1u);
  emu->step_instruction(); // dec -> 0
  emu->step_instruction(); // brne, не взят
  CHECK_EQ(emu->get_pc(), 3u);
  CHECK_EQ(emu->read_data(16, 1), 0u);

  for (int i = 0; i < 10; ++i) {
    emu->step_instruction(); // rjmp .-1
    CHECK_EQ(emu->get_pc(), 3u);
  }
}
