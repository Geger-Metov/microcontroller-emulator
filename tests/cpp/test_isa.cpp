// Направленные тесты набора инструкций: адресация LD/ST, LPM, стек, биты SREG,
// ветвления, пропуски, SLEEP, такты, таблица декодера.
#include "avr_decoder.h"
#include "avr_instructions.h"
#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

namespace {

void steps(IEmulatorFacade &e, int n) {
  while (n-- > 0)
    e.step_instruction();
}

uint16_t ldst(uint16_t base, int rd) { return base | (rd << 4); }
uint16_t ldd_std(bool store, bool y, int rd, int q) {
  return (store ? 0x8200 : 0x8000) | (y ? 0x0008 : 0) | ((q & 0x20) << 8) |
         ((q & 0x18) << 7) | (q & 7) | (rd << 4);
}
uint16_t brbs(int s, int k) { return 0xF000 | ((k & 0x7F) << 3) | s; }
uint16_t brbc(int s, int k) { return 0xF400 | ((k & 0x7F) << 3) | s; }

constexpr uint32_t SRAM = 0x100;

} // namespace

TEST(decoder_table_entries_never_overlap) {
  for (uint32_t op = 0; op <= 0xFFFF; ++op) {
    int matches = 0;
    for (std::size_t i = 0; i < avr::avre_plus_instruction_table_size; ++i) {
      const auto &d = avr::avre_plus_instruction_table[i];
      if ((op & d.mask) == d.pattern)
        ++matches;
    }
    if (matches > 1) {
      char buf[64];
      std::snprintf(buf, sizeof buf, "opcode 0x%04X matches %d entries", op,
                    matches);
      throw mctest::Failure{buf};
    }
  }
}

TEST(decoder_recognises_exactly_the_328p_opcode_space) {
  // Ориентир: у ATmega328P нет ELPM/EIJMP/EICALL/DES/XCH/LAS/LAC/LAT.
  AVRDecoder dec(avr::avre_plus_instruction_table,
                 avr::avre_plus_instruction_table_size);
  avr::DecodedInstruction d;
  for (uint16_t absent :
       {0x95D8 /*ELPM*/, 0x9419 /*EIJMP*/, 0x9519 /*EICALL*/, 0x940B /*DES*/,
        0x9204 /*XCH*/, 0x9205 /*LAS*/, 0x9206 /*LAC*/, 0x9207 /*LAT*/})
    CHECK(dec.decode(absent, d) == nullptr);
  for (uint16_t present : {0x0000, 0x9508, 0x9518, 0x9588, 0x95C8, 0x9409})
    CHECK(dec.decode(present, d) != nullptr);
}

TEST(illegal_opcode_throws) {
  auto emu = make_emu({{0, {0x95D8}}}); // ELPM: нет на ATmega328P
  CHECK_THROWS(emu->step_instruction());
}

TEST(jmp_and_call_decode_all_22_address_bits) {
  AVRDecoder dec(avr::avre_plus_instruction_table,
                 avr::avre_plus_instruction_table_size);
  avr::DecodedInstruction d;
  // JMP k=0x3FFFFF: первое слово 0x95FD, второе 0xFFFF
  CHECK(dec.decode(0xFFFF95FDu, d) != nullptr);
  CHECK_EQ(d.imm, 0x3FFFFFu);
  // CALL k=0x20000 (бит 17): k[21:17] лежат в битах 8..4 первого слова
  CHECK(dec.decode(0x0000941Eu, d) != nullptr);
  CHECK_EQ(d.imm, 0x20000u);
  // CALL k=0x10000 (бит 16): бит 0 первого слова
  CHECK(dec.decode(0x0000940Fu, d) != nullptr);
  CHECK_EQ(d.imm, 0x10000u);
  CHECK(dec.decode(0x0010940Cu, d) != nullptr);
  CHECK_EQ(d.imm, 0x10u);
}

TEST(ld_st_indirect_addressing_modes) {
  // Значения в соседних ячейках разные, чтобы ошибка на 1 в адресе была видна.
  auto emu =
      make_emu({{0,
                 {ldi(26, 0x00), ldi(27, 0x01), // X = 0x0100
                  ldi(16, 0x11), ldi(17, 0x22),
                  ldst(0x920D, 16),     // st X+, r16  -> [0x100]=11, X=0x101
                  ldst(0x920D, 17),     // st X+, r17  -> [0x101]=22, X=0x102
                  ldst(0x900E, 19),     // ld r19, -X  -> X=0x101, r19=22
                  ldst(0x900C, 18),     // ld r18, X   -> r18=22
                  ldst(0x900D, 20),     // ld r20, X+  -> r20=22, X=0x102
                  ldst(0x920E, 16)}}}); // st -X, r16 -> X=0x101, [0x101]=11
  steps(*emu, 10);
  CHECK_EQ(emu->read_data(SRAM, 1), 0x11u);
  CHECK_EQ(emu->read_data(SRAM + 1, 1), 0x11u);
  CHECK_EQ(emu->read_data(SRAM + 2, 1), 0x00u); // за пределы не писали
  CHECK_EQ(emu->read_data(19, 1), 0x22u);
  CHECK_EQ(emu->read_data(18, 1), 0x22u);
  CHECK_EQ(emu->read_data(20, 1), 0x22u);
  CHECK_EQ(emu->read_data(26, 1), 0x01u);
  CHECK_EQ(emu->read_data(27, 1), 0x01u);
}

TEST(ld_st_y_and_z_with_displacement) {
  // Y = 0x0120, Z = 0x0140; q-смещения 0, 5, 63 покрывают все биты q.
  std::vector<uint16_t> prog = {ldi(28, 0x20), ldi(29, 0x01), ldi(30, 0x40),
                                ldi(31, 0x01), ldi(16, 0xA0), ldi(17, 0xB1),
                                ldi(18, 0xC2)};
  prog.push_back(ldd_std(true, true, 16, 0));   // std Y+0, r16
  prog.push_back(ldd_std(true, true, 17, 5));   // std Y+5, r17
  prog.push_back(ldd_std(true, true, 18, 63));  // std Y+63, r18
  prog.push_back(ldd_std(true, false, 16, 9));  // std Z+9, r16
  prog.push_back(ldd_std(false, true, 20, 5));  // ldd r20, Y+5
  prog.push_back(ldd_std(false, false, 21, 9)); // ldd r21, Z+9
  prog.push_back(ldd_std(false, true, 22, 63)); // ldd r22, Y+63
  prog.push_back(ldd_std(false, true, 23, 0));  // ld r23, Y
  auto emu = make_emu({{0, prog}});
  steps(*emu, static_cast<int>(prog.size()));
  CHECK_EQ(emu->read_data(0x120, 1), 0xA0u);
  CHECK_EQ(emu->read_data(0x125, 1), 0xB1u);
  CHECK_EQ(emu->read_data(0x120 + 63, 1), 0xC2u);
  CHECK_EQ(emu->read_data(0x149, 1), 0xA0u);
  CHECK_EQ(emu->read_data(20, 1), 0xB1u);
  CHECK_EQ(emu->read_data(21, 1), 0xA0u);
  CHECK_EQ(emu->read_data(22, 1), 0xC2u);
  CHECK_EQ(emu->read_data(23, 1), 0xA0u);
  // указатели при смещении не меняются
  CHECK_EQ(emu->read_data(28, 1), 0x20u);
  CHECK_EQ(emu->read_data(30, 1), 0x40u);
}

TEST(ld_st_post_increment_wraps_pointer_at_64k_and_y_z_modes) {
  auto emu = make_emu({{0,
                        {ldi(30, 0x00), ldi(31, 0x01),    // Z=0x100
                         ldi(16, 0x5A), ldst(0x9201, 16), // st Z+, r16
                         ldst(0x9202, 16), // st -Z, r16 (Z: 0x101 -> 0x100)
                         ldi(28, 0x01), ldi(29, 0x01), // Y=0x101
                         ldst(0x9209, 16),             // st Y+, r16
                         ldst(0x920A, 16),             // st -Y, r16
                         ldst(0x9001, 17),             // ld r17, Z+
                         ldst(0x9002, 18)}}});         // ld r18, -Z
  steps(*emu, 11);
  CHECK_EQ(emu->read_data(SRAM, 1), 0x5Au);
  CHECK_EQ(emu->read_data(SRAM + 1, 1), 0x5Au);
  CHECK_EQ(emu->read_data(17, 1), 0x5Au);
  CHECK_EQ(emu->read_data(18, 1), 0x5Au);
  CHECK_EQ(emu->read_data(30, 1), 0x00u);
  CHECK_EQ(emu->read_data(31, 1), 0x01u);
  CHECK_EQ(emu->read_data(28, 1), 0x01u);
}

TEST(lds_sts_use_second_word_as_address) {
  auto emu = make_emu({{0,
                        {ldi(16, 0x77), 0x9200 | (16 << 4), 0x0123, // sts 0x123
                         0x9000 | (17 << 4), 0x0123,                // lds r17
                         NOP}}});
  steps(*emu, 3);
  CHECK_EQ(emu->read_data(0x123, 1), 0x77u);
  CHECK_EQ(emu->read_data(17, 1), 0x77u);
  CHECK_EQ(emu->get_pc(), 5u); // обе 2-словные инструкции продвинули PC на 2
}

TEST(lpm_reads_low_and_high_flash_bytes) {
  auto emu = make_emu({{0,
                        {ldi(30, 0x40), ldi(31, 0x00), // Z = 0x0040
                         0x95C8,                       // lpm        -> r0
                         ldst(0x9004, 5),              // lpm r5, Z
                         ldst(0x9005, 6),              // lpm r6, Z+
                         ldst(0x9005, 7)}},            // lpm r7, Z+
                       {0x20, {0x3412, 0x7856}}});
  steps(*emu, 6);
  CHECK_EQ(emu->read_data(0, 1), 0x12u);
  CHECK_EQ(emu->read_data(5, 1), 0x12u);
  CHECK_EQ(emu->read_data(6, 1), 0x12u);
  CHECK_EQ(emu->read_data(7, 1), 0x34u);
  CHECK_EQ(emu->read_data(30, 1), 0x42u);
}

TEST(push_pop_roundtrip_and_stack_pointer) {
  auto emu = make_emu({{0,
                        {ldi(16, 0x11), ldi(17, 0x22), ldst(0x920F, 16),
                         ldst(0x920F, 17), // push r16, r17
                         ldst(0x900F, 18), ldst(0x900F, 19)}}}); // pop r18, r19
  uint32_t sp0 = emu->get_sp();
  steps(*emu, 4);
  CHECK_EQ(emu->get_sp(), sp0 - 2);
  CHECK_EQ(emu->read_data(sp0, 1), 0x11u); // первый push по старшему адресу
  CHECK_EQ(emu->read_data(sp0 - 1, 1), 0x22u);
  steps(*emu, 2);
  CHECK_EQ(emu->get_sp(), sp0);
  CHECK_EQ(emu->read_data(18, 1), 0x22u);
  CHECK_EQ(emu->read_data(19, 1), 0x11u);
}

TEST(bset_bclr_cover_all_eight_sreg_flags) {
  for (int s = 0; s < 8; ++s) {
    auto emu = make_emu({{0,
                          {static_cast<uint16_t>(0x9408 | (s << 4)),
                           static_cast<uint16_t>(0x9488 | (s << 4))}}});
    emu->step_instruction();
    CHECK_EQ(emu->get_sreg(), 1u << s);
    emu->step_instruction();
    CHECK_EQ(emu->get_sreg(), 0u);
  }
}

TEST(bst_bld_transfer_bits_through_t_flag) {
  auto emu = make_emu({{0,
                        {ldi(16, 0x08), ldi(17, 0xFF), ldi(18, 0x00),
                         0xFA00 | (16 << 4) | 3,     // bst r16,3 -> T=1
                         0xF800 | (18 << 4) | 6,     // bld r18,6 -> r18=0x40
                         0xFA00 | (16 << 4) | 0,     // bst r16,0 -> T=0
                         0xF800 | (17 << 4) | 7}}}); // bld r17,7 -> r17=0x7F
  steps(*emu, 4);                                    // 3 x ldi + bst
  CHECK((emu->get_sreg() >> 6) & 1);
  emu->step_instruction(); // bld
  CHECK_EQ(emu->read_data(18, 1), 0x40u);
  steps(*emu, 2); // bst r16,0 ; bld r17,7
  CHECK(((emu->get_sreg() >> 6) & 1) == 0);
  CHECK_EQ(emu->read_data(17, 1), 0x7Fu);
}

TEST(branches_on_every_sreg_bit_forward_and_backward) {
  for (int s = 0; s < 8; ++s) {
    for (int flag = 0; flag < 2; ++flag) {
      // BRBS: переход при flag==1, BRBC: при flag==0
      for (int is_brbs = 0; is_brbs < 2; ++is_brbs) {
        bool taken = is_brbs ? flag == 1 : flag == 0;
        uint16_t fwd = is_brbs ? brbs(s, 2) : brbc(s, 2);
        auto emu = make_emu({{0, {fwd, NOP, NOP, NOP}}});
        emu->write_data(SREG, flag << s, 1);
        emu->step_instruction();
        CHECK_EQ(emu->get_pc(), taken ? 3u : 1u);
        CHECK_EQ(emu->get_cycle_count(), taken ? 2u : 1u);

        // назад: rjmp на слово 4, там ветвление на -3 -> слово 2
        uint16_t bwd = is_brbs ? brbs(s, -3) : brbc(s, -3);
        auto emu2 = make_emu({{0, {rjmp(3), NOP, NOP, NOP, bwd}}});
        emu2->write_data(SREG, flag << s, 1);
        emu2->step_instruction();
        emu2->step_instruction();
        CHECK_EQ(emu2->get_pc(), taken ? 2u : 5u);
      }
    }
  }
}

TEST(skip_on_register_and_io_bits_costs_one_or_two_extra_cycles) {
  const uint16_t two_word = 0x940C; // JMP k (второе слово — адрес)
  struct Case {
    uint16_t skip_op;
    uint8_t reg_val; // значение r16 / PORTB
    bool skipped;
  };
  const Case cases[] = {
      {static_cast<uint16_t>(0xFC00 | (16 << 4) | 3), 0x08,
       false}, // sbrc, бит=1
      {static_cast<uint16_t>(0xFC00 | (16 << 4) | 3), 0x00,
       true}, // sbrc, бит=0
      {static_cast<uint16_t>(0xFE00 | (16 << 4) | 3), 0x08,
       true}, // sbrs, бит=1
      {static_cast<uint16_t>(0xFE00 | (16 << 4) | 3), 0x00,
       false},                             // sbrs, бит=0
      {0x9900 | (5 << 3) | 3, 0x00, true}, // sbic PORTB,3
      {0x9900 | (5 << 3) | 3, 0x08, false},
      {0x9B00 | (5 << 3) | 3, 0x08, true}, // sbis PORTB,3
      {0x9B00 | (5 << 3) | 3, 0x00, false},
  };
  for (const Case &c : cases) {
    for (int words = 1; words <= 2; ++words) {
      std::vector<uint16_t> prog{c.skip_op};
      if (words == 2) {
        prog.push_back(two_word);
        prog.push_back(0x0000);
      } else {
        prog.push_back(NOP);
      }
      prog.push_back(NOP);
      auto emu = make_emu({{0, prog}});
      emu->write_data(16, c.reg_val, 1);
      emu->write_data(0x25, c.reg_val, 1); // PORTB
      uint64_t c0 = emu->get_cycle_count();
      emu->step_instruction();
      uint64_t cyc = emu->get_cycle_count() - c0;
      if (c.skipped) {
        CHECK_EQ(emu->get_pc(), 1u + words);
        CHECK_EQ(cyc, 1u + words);
      } else {
        CHECK_EQ(emu->get_pc(), 1u);
        CHECK_EQ(cyc, 1u);
      }
    }
  }
}

TEST(sei_lets_one_instruction_run_before_pending_interrupt) {
  // TOV0 успевает выставиться до SEI; прерывание должно ждать одну инструкцию.
  std::vector<Chunk> chunks = {
      {0, {NOP, NOP, NOP, 0x9478 /*sei*/, NOP, NOP, rjmp(-1)}}, {0x20, {RETI}}};
  auto emu = make_emu(chunks);
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCNT0, 0xFF, 1);
  emu->write_data(TCCR0B, 1, 1);
  steps(*emu, 3); // три NOP: таймер переполнился, но I=0
  CHECK_EQ(emu->get_pc(), 3u);
  emu->step_instruction(); // SEI
  CHECK_EQ(emu->get_pc(), 4u);
  emu->step_instruction(); // NOP после SEI — выполняется, затем вход в ISR
  CHECK_EQ(emu->get_pc(), 0x20u);
}

TEST(sleep_halts_core_but_peripherals_keep_running) {
  auto emu = make_emu({{0, {0x9588 /*sleep*/, NOP, rjmp(-1)}}});
  emu->write_data(TCCR0B, 1, 1); // clk/1
  emu->step_instruction();       // SLEEP: 1 такт
  CHECK_EQ(emu->get_pc(), 1u);
  uint64_t c0 = emu->get_cycle_count();
  for (int i = 0; i < 100; ++i)
    emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 1u); // ядро стоит
  CHECK_EQ(emu->get_cycle_count() - c0, 100u);
  CHECK_EQ(emu->read_data(TCNT0, 1), emu->get_cycle_count() % 256);
}

TEST(sleep_wakes_on_enabled_interrupt_and_returns_after_sleep) {
  std::vector<Chunk> chunks = {
      {0, {0x9478 /*sei*/, 0x9588 /*sleep*/, ldi(16, 7), rjmp(-1)}},
      {0x20, {RETI}}};
  auto emu = make_emu(chunks);
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCNT0, 250, 1);
  emu->write_data(TCCR0B, 1, 1);
  uint32_t sp0 = emu->get_sp();
  int guard = 0;
  while (emu->get_pc() != 0x20 && guard++ < 100)
    emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 0x20u);
  CHECK_EQ(emu->get_sp(), sp0 - 2);
  // адрес возврата — слово 2, т.е. инструкция сразу ПОСЛЕ SLEEP
  CHECK_EQ(emu->read_data(sp0, 1), 2u);
  CHECK_EQ(emu->read_data(sp0 - 1, 1), 0u);
  emu->step_instruction(); // RETI
  CHECK_EQ(emu->get_pc(), 2u);
}

TEST(sleep_wakes_without_vector_when_global_interrupts_disabled) {
  auto emu = make_emu({{0, {0x9588, ldi(16, 7), rjmp(-1)}}, {0x20, {RETI}}});
  emu->write_data(TIMSK0, 1, 1);
  emu->write_data(TCNT0, 250, 1);
  emu->write_data(TCCR0B, 1, 1);
  int guard = 0;
  while (emu->read_data(16, 1) != 7 && guard++ < 100)
    emu->step_instruction();
  CHECK_EQ(emu->read_data(16, 1), 7u); // выполнилась инструкция после SLEEP
  CHECK(emu->get_pc() != 0x20u);       // в вектор не уходили (I=0)
}

TEST(sleep_does_not_wake_on_disabled_interrupt_source) {
  auto emu = make_emu({{0, {0x9588, ldi(16, 7), rjmp(-1)}}});
  emu->write_data(TIMSK0, 0, 1); // TOIE0 выключен
  emu->write_data(TCNT0, 250, 1);
  emu->write_data(TCCR0B, 1, 1);
  steps(*emu, 2000);
  CHECK_EQ(emu->read_data(16, 1), 0u);
  CHECK_EQ(emu->get_pc(), 1u);
}

TEST(reset_clears_sleep_state) {
  auto emu = make_emu({{0, {ldi(16, 7), 0x9588, rjmp(-1)}}});
  steps(*emu, 2); // ldi, sleep
  steps(*emu, 10);
  CHECK_EQ(emu->get_pc(), 2u); // ядро спит
  emu->reset();
  CHECK_EQ(emu->read_data(16, 1), 0u);
  emu->step_instruction(); // после reset выполняется ldi, а не «спим дальше»
  CHECK_EQ(emu->read_data(16, 1), 7u);
}

TEST(documented_instruction_cycle_counts) {
  struct Case {
    const char *name;
    std::vector<uint16_t> insn; // тестируемая инструкция (1–2 слова)
    unsigned cycles;
  };
  const std::vector<Case> cases = {
      {"ld X", {ldst(0x900C, 18)}, 2},
      {"ld X+", {ldst(0x900D, 18)}, 2},
      {"ld -X", {ldst(0x900E, 18)}, 2},
      {"st X+", {ldst(0x920D, 16)}, 2},
      {"ldd Y+3", {ldd_std(false, true, 18, 3)}, 2},
      {"std Z+5", {ldd_std(true, false, 16, 5)}, 2},
      {"lds", {0x9000 | (18 << 4), 0x0120}, 2},
      {"sts", {0x9200 | (16 << 4), 0x0120}, 2},
      {"lpm r0", {0x95C8}, 3},
      {"lpm Rd,Z+", {ldst(0x9005, 18)}, 3},
      {"adiw", {sbiw_adiw(0x9600, 24, 1)}, 2},
      {"sbiw", {sbiw_adiw(0x9700, 24, 1)}, 2},
      {"push", {ldst(0x920F, 16)}, 2},
      {"pop", {ldst(0x900F, 18)}, 2},
      {"sbi", {0x9A00 | (5 << 3) | 1}, 2},
      {"cbi", {0x9800 | (5 << 3) | 1}, 2},
      {"movw", {0x0100 | (2 << 4) | 4}, 1},
      {"rjmp", {rjmp(0)}, 2},
      {"jmp", {0x940C, 0x0010}, 3},
      {"ijmp", {0x9409}, 2},
      {"rcall", {0xD000}, 3},
      {"call", {0x940E, 0x0010}, 4},
      {"icall", {0x9509}, 3},
      {"mul", {rd_rr(0x9C00, 16, 17)}, 2},
      {"bset", {0x9408}, 1},
      {"swap", {unary(0x9402, 16)}, 1},
      {"in", {0xB000 | (16 << 4)}, 1},
      {"out", {out(0x05, 16)}, 1},
  };
  for (const Case &c : cases) {
    std::vector<uint16_t> prog = {ldi(26, 0x00),   ldi(27, 0x01), ldi(28, 0x00),
                                  ldi(29, 0x01),   ldi(30, 0x00), ldi(31, 0x00),
                                  ldst(0x920F, 16)}; // push — чтобы работал pop
    prog.insert(prog.end(), c.insn.begin(), c.insn.end());
    auto emu = make_emu({{0, prog}});
    steps(*emu, 7);
    uint64_t c0 = emu->get_cycle_count();
    emu->step_instruction();
    if (emu->get_cycle_count() - c0 != c.cycles) {
      throw mctest::Failure{std::string("cycles of ") + c.name + ": got " +
                            std::to_string(emu->get_cycle_count() - c0) +
                            ", expected " + std::to_string(c.cycles)};
    }
  }
}

TEST(call_ret_roundtrip_costs_documented_cycles) {
  // call 4 ; (слово 2: nop) ; слово 4: ret
  auto emu = make_emu({{0, {0x940E, 0x0004, NOP, NOP, 0x9508}}});
  emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 4u);
  CHECK_EQ(emu->get_cycle_count(), 4u);
  emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 2u); // возврат после двухсловного CALL
  CHECK_EQ(emu->get_cycle_count(), 8u);
}
