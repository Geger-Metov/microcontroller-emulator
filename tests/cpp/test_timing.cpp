// Cycle accounting, reset, interrupts (priority, the delay after RETI), the
// stack.
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "common/errors.h"
#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

TEST(cycle_counts_for_taken_and_not_taken_branches) {
  // ldi r16,3 ; dec r16 ; brne .-2 ; rjmp .-1
  auto emu = make_emu({{0, {ldi(16, 3), 0x950A, 0xF7F1, rjmp(-1)}}});
  for (int i = 0; i < 7; ++i) // ldi + 3 * (dec, brne)
    emu->step_instruction();
  // 1 + (1+2) + (1+2) + (1+1) = 9: a taken BRNE costs 2, a not-taken one 1
  CHECK_EQ(emu->get_cycle_count(), 9u);
}

TEST(skip_instructions_cost_extra_cycles) {
  // cpse r16,r16 (equal -> skip) before a two-word JMP: 1 + 2 = 3 cycles
  auto emu = make_emu({{0, {0x1300, 0x940C, 0x0010, NOP}}});
  emu->step_instruction();
  CHECK_EQ(emu->get_pc(), 3u);
  CHECK_EQ(emu->get_cycle_count(), 3u);

  // skipping a one-word instruction: 1 + 1 = 2 cycles
  auto emu2 = make_emu({{0, {0x1300, NOP, NOP}}});
  emu2->step_instruction();
  CHECK_EQ(emu2->get_pc(), 2u);
  CHECK_EQ(emu2->get_cycle_count(), 2u);
}

TEST(run_for_cycles_matches_cpu_cycle_count) {
  // They used to diverge by ~25%: the extra branch cycles did not reach the
  // budget and the peripheral ticks.
  auto emu = make_emu({{0, {ldi(16, 3), 0x950A, 0xF7F1, rjmp(-1)}}});
  emu->run_for_cycles(100000);
  uint64_t c = emu->get_cycle_count();
  CHECK(c >= 100000u);
  CHECK(c < 100000u + 8u); // an overshoot of no more than one instruction
}

TEST(timer_runs_at_cpu_speed_with_taken_branches) {
  // A loop of taken branches: the timer must see the same cycles as the CPU.
  // ldi r16,0 ; dec r16 ; brne .-2 -> 256 iterations (dec: 0 -> 255 ...)
  auto emu = make_emu({{0, {ldi(16, 0), 0x950A, 0xF7F1, rjmp(-1)}}});
  emu->write_data(TCCR0B, 1, 1); // clk/1
  emu->run_for_cycles(1000);
  uint64_t cycles = emu->get_cycle_count();
  CHECK_EQ(emu->read_data(TCNT0, 1), cycles % 256);
}

TEST(call_pushes_return_address_like_real_avr) {
  // rjmp -> 0x1FD ; call 0x300 (return to 0x1FF: high 0x01, low 0xFF)
  auto emu = make_emu({{0, {rjmp(0x1FC)}}, {0x1FD, {0x940E, 0x0300}}});
  emu->step_instruction(); // rjmp
  CHECK_EQ(emu->get_pc(), 0x1FDu);
  uint32_t sp0 = emu->get_sp();
  emu->step_instruction(); // call
  CHECK_EQ(emu->get_pc(), 0x300u);
  CHECK_EQ(emu->get_sp(), sp0 - 2);
  // On a real AVR: the low byte goes to the higher address, the high byte to
  // the lower one
  CHECK_EQ(emu->read_data(sp0, 1), 0xFFu);     // PCL
  CHECK_EQ(emu->read_data(sp0 - 1, 1), 0x01u); // PCH
}

TEST(reset_restores_power_on_state) {
  auto emu = make_emu({{0, {rjmp(-1)}}});
  emu->write_data(16, 0xAB, 1);    // R16
  emu->write_data(0x100, 0xCD, 1); // SRAM
  emu->write_data(SREG, 0xFF, 1);
  emu->write_data(TCCR0B, 1, 1);
  emu->write_data(TCNT0, 0x55, 1);
  emu->write_data(0x25, 0x20, 1); // PORTB
  emu->run_for_cycles(100);

  emu->reset();

  CHECK_EQ(emu->get_pc(), 0u);
  CHECK_EQ(emu->get_cycle_count(), 0u);
  CHECK_EQ(emu->get_sp(), 0x08FFu);
  CHECK_EQ(emu->read_data(16, 1), 0u);
  CHECK_EQ(emu->read_data(0x100, 1), 0u);
  CHECK_EQ(emu->read_data(SREG, 1), 0u);
  CHECK_EQ(emu->read_data(TCCR0B, 1), 0u);
  CHECK_EQ(emu->read_data(TCNT0, 1), 0u);
  CHECK_EQ(emu->read_data(0x25, 1), 0u);
}

TEST(unimplemented_io_is_read_zero_write_ignored) {
  auto emu = make_emu({{0, {NOP}}});
  // 0x60 (WDTCSR), 0xC6 (UDR0) — registers not in the model
  CHECK_EQ(emu->read_data(0x60, 1), 0u);
  emu->write_data(0xC6, 0x41, 1);
  CHECK_EQ(emu->read_data(0xC6, 1), 0u);
}

TEST(access_outside_address_space_still_throws_with_hex_address) {
  auto emu = make_emu({{0, {NOP}}});
  bool threw = false;
  try {
    (void)emu->read_data(0x0900, 1);
  } catch (const std::out_of_range &e) {
    threw = true;
    CHECK(std::string(e.what()).find("0x0900") != std::string::npos);
  }
  CHECK(threw);
}

// ---- Stop and run state ----------------------------------------------

TEST(run_for_cycles_returns_executed_cycles) {
  auto emu = make_emu({{0, {0xCFFF}}}); // RJMP .-1 : 2 cycles per instruction
  uint64_t done = emu->run_for_cycles(1000);
  CHECK_EQ(done, 1000u);
  CHECK_EQ(emu->get_cycle_count(), 1000u);
  CHECK(!emu->is_running());
}

TEST(pause_from_another_thread_stops_a_long_run) {
  auto emu = make_emu({{0, {0xCFFF}}}); // an infinite loop
  std::atomic<uint64_t> done{0};
  std::thread worker([&] { done = emu->run_for_cycles(~0ull >> 2); });

  // Wait until the run has actually started.
  for (int i = 0; i < 2000 && !emu->is_running(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  CHECK(emu->is_running());

  emu->pause();
  worker.join();
  CHECK(!emu->is_running());
  CHECK(done.load() > 0);
  CHECK(done.load() < (~0ull >> 2));
  CHECK_EQ(done.load(), emu->get_cycle_count()); // the counter is consistent
}

TEST(pause_without_a_run_does_not_block_the_next_run) {
  auto emu = make_emu({{0, {0xCFFF}}});
  emu->pause();
  emu->stop();
  CHECK_EQ(emu->run_for_cycles(100), 100u);
}

TEST(running_flag_is_cleared_when_run_throws) {
  auto emu = make_emu({{0, {0x95D8}}}); // ELPM: Illegal instruction
  CHECK_THROWS(emu->run_for_cycles(100));
  CHECK(!emu->is_running());
}

TEST(illegal_instruction_error_names_opcode_and_address) {
  auto emu = make_emu({{0, {0x0000, 0x95D8}}});
  try {
    emu->run_for_cycles(100);
    CHECK(false);
  } catch (const IllegalInstructionError &e) {
    std::string m = e.what();
    CHECK(m.find("0x95D8") != std::string::npos);
    CHECK(m.find("0x0001") != std::string::npos);  // the address in words
    CHECK(m.find("0x00002") != std::string::npos); // the address in bytes
  }
}

TEST(bus_errors_are_memory_access_errors) {
  auto emu = make_emu({{0, {0x0000}}});
  bool data = false;
  try {
    emu->read_data(0x9000, 1);
  } catch (const MemoryAccessError &) {
    data = true;
  }
  CHECK(data);
}
