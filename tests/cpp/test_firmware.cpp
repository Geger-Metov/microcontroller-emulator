// Integration tests: loading real HEX files and running the firmware.
#include <fstream>
#include <memory>
#include <string>

#include "interfaces/Interfaces.h"
#include "mcu/avr/ATmega328p.h"
#include "test_framework.h"

#ifndef MCU_HEX_DIR
#error "MCU_HEX_DIR must point to the directory with firmware .hex files"
#endif

namespace {

constexpr uint32_t DDRB = 0x24;
constexpr uint32_t PORTB = 0x25;
constexpr uint8_t PB5 = 1u << 5;

std::unique_ptr<IEmulatorFacade> load_firmware(const std::string &name) {
  const std::string path = std::string(MCU_HEX_DIR) + "/" + name;
  std::ifstream file(path);
  if (!file)
    throw std::runtime_error("cannot open " + path);
  auto emu = ATmega328P::create();
  ATmega328P::load_hex(*emu, file);
  emu->reset();
  return emu;
}

} // namespace

TEST(firmware_hex_is_loaded_into_flash) {
  auto emu = load_firmware("firmware.hex");
  // The first instruction of the RESET vector: JMP k (0x940C, then the
  // address).
  CHECK_EQ(emu->read_program(0, 1), 0x940Cu);
  // Erased flash reads as 0xFF.
  CHECK_EQ(emu->read_program(0x3FFF, 1), 0xFFFFu);
}

TEST(firmware_hex_blinks_pb5) {
  auto emu = load_firmware("firmware.hex");

  int toggles = 0;
  uint8_t last = emu->read_data(PORTB, 1) & PB5;
  // Execution must not throw (it used to fail on a backward BRNE).
  for (int i = 0; i < 3000000 && toggles < 4; ++i) {
    emu->step_instruction();
    uint8_t now = emu->read_data(PORTB, 1) & PB5;
    if (now != last) {
      ++toggles;
      last = now;
    }
  }

  CHECK_EQ(toggles, 4);
  CHECK(emu->read_data(DDRB, 1) & PB5); // the pin is configured as an output
}

TEST(firmware_test_hex_runs_without_errors) {
  auto emu = load_firmware("firmware_test.hex");
  for (int i = 0; i < 200000; ++i)
    emu->step_instruction();
  CHECK(emu->get_cycle_count() > 0);
}

TEST(reset_returns_pc_to_zero) {
  auto emu = load_firmware("firmware.hex");
  for (int i = 0; i < 100; ++i)
    emu->step_instruction();
  CHECK(emu->get_pc() != 0u);
  emu->reset();
  CHECK_EQ(emu->get_pc(), 0u);
}

// A button on PD2 (pull-up, pressed = low) lights the LED on PB5.
TEST(button_firmware_lights_the_led_only_while_pressed) {
  auto emu = load_firmware("button_led.hex");
  emu->run_for_cycles(100);
  CHECK_EQ(emu->read_data(PORTB, 1) & PB5, 0u); // released: LED off

  emu->set_pin_level('D', 2, PinLevel::Low); // pressed
  emu->run_for_cycles(100);
  CHECK_EQ(emu->read_data(PORTB, 1) & PB5, static_cast<uint64_t>(PB5));

  emu->set_pin_level('D', 2, PinLevel::Floating); // released
  emu->run_for_cycles(100);
  CHECK_EQ(emu->read_data(PORTB, 1) & PB5, 0u);
}
