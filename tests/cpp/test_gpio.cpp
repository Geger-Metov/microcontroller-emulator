// GPIO: PINx levels, pull-ups, external drive, toggling through PINx.
#include <stdexcept>

#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

namespace {

std::unique_ptr<IEmulatorFacade> idle_emu() { return make_emu({{0, {NOP}}}); }

uint8_t rd(IEmulatorFacade &e, uint32_t addr) {
  return static_cast<uint8_t>(e.read_data(addr, 1));
}

} // namespace

TEST(output_pin_reads_its_port_bit) {
  auto emu = idle_emu();
  emu->write_data(DDRB, 0x20, 1);
  emu->write_data(PORTB, 0x20, 1);
  CHECK_EQ(rd(*emu, PINB), 0x20);
  emu->write_data(PORTB, 0x00, 1);
  CHECK_EQ(rd(*emu, PINB), 0x00);
}

TEST(floating_input_reads_pullup_state) {
  auto emu = idle_emu();
  CHECK_EQ(rd(*emu, PINB), 0x00);  // no pull-up, nothing connected
  emu->write_data(PORTB, 0x0C, 1); // pull-ups on PB2, PB3 (DDRB = 0: inputs)
  CHECK_EQ(rd(*emu, PINB), 0x0C);
}

TEST(external_level_overrides_the_pullup) {
  auto emu = idle_emu();
  emu->write_data(PORTB, 0x04, 1);           // pull-up on PB2
  emu->set_pin_level('B', 2, PinLevel::Low); // a pressed button
  CHECK_EQ(rd(*emu, PINB), 0x00);
  emu->set_pin_level('B', 2, PinLevel::Floating); // released
  CHECK_EQ(rd(*emu, PINB), 0x04);
}

TEST(external_high_is_read_without_pullup) {
  auto emu = idle_emu();
  emu->set_pin_level('D', 7, PinLevel::High);
  CHECK_EQ(rd(*emu, PIND), 0x80);
  CHECK_EQ(rd(*emu, PINB), 0x00); // the other port is unaffected
}

TEST(external_level_does_not_override_an_output) {
  auto emu = idle_emu();
  emu->write_data(DDRB, 0x01, 1);
  emu->write_data(PORTB, 0x01, 1);
  emu->set_pin_level('B', 0, PinLevel::Low);
  CHECK_EQ(rd(*emu, PINB) & 0x01, 0x01);
}

TEST(writing_one_to_pin_toggles_port) {
  auto emu = idle_emu();
  emu->write_data(PORTB, 0b0000'0101, 1);
  emu->write_data(PINB, 0b0000'0011, 1); // toggle bits 0 and 1
  CHECK_EQ(rd(*emu, PORTB), 0b0000'0110);
  emu->write_data(PINB, 0x00, 1); // writing zeros changes nothing
  CHECK_EQ(rd(*emu, PORTB), 0b0000'0110);
  CHECK_EQ(rd(*emu, DDRB), 0x00); // DDRx is untouched
}

TEST(external_levels_survive_reset) {
  auto emu = idle_emu();
  emu->set_pin_level('B', 1, PinLevel::High);
  emu->reset();
  CHECK_EQ(rd(*emu, PINB), 0x02);
}

TEST(bad_port_or_bit_throws) {
  auto emu = idle_emu();
  CHECK_THROWS(emu->set_pin_level('C', 0, PinLevel::Low));
  CHECK_THROWS(emu->set_pin_level('B', 8, PinLevel::Low));
}

// Firmware: a button on PD2 (pull-up) is mirrored to an LED pin PB5.
//   ldi r16,0x04 ; out PORTD,r16 ; ldi r17,0x20 ; out DDRB,r17
//   loop: in r16,PIND ; out PORTB,r16 ; rjmp loop
TEST(firmware_reads_a_button_and_drives_an_output) {
  auto emu =
      make_emu({{0,
                 {ldi(16, 0x04), out(0x0B, 16), ldi(17, 0x20), out(0x04, 17),
                  in(16, 0x09), out(0x05, 16), rjmp(-3)}}});
  emu->run_for_cycles(100);
  CHECK_EQ(rd(*emu, PORTB), 0x04);       // released: PD2 = 1 (the pull-up)
  CHECK_EQ(rd(*emu, PINB) & 0x20, 0x00); // the LED pin PB5 stays 0

  emu->set_pin_level('D', 2, PinLevel::Low); // the button is pressed
  emu->run_for_cycles(100);
  CHECK_EQ(rd(*emu, PORTB), 0x00);

  emu->set_pin_level('D', 2, PinLevel::Floating); // released again
  emu->run_for_cycles(100);
  CHECK_EQ(rd(*emu, PORTB), 0x04);
}
