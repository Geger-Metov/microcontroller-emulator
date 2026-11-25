// Интеграционные тесты: загрузка реальных HEX-файлов и выполнение прошивки.
#include <fstream>
#include <memory>
#include <string>

#include "ATmega328p.h"
#include "Interfaces.h"
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
  // Первая инструкция вектора RESET: JMP k (0x940C, затем адрес).
  CHECK_EQ(emu->read_program(0, 1), 0x940Cu);
  // Стёртый flash читается как 0xFF.
  CHECK_EQ(emu->read_program(0x3FFF, 1), 0xFFFFu);
}

TEST(firmware_hex_blinks_pb5) {
  auto emu = load_firmware("firmware.hex");

  int toggles = 0;
  uint8_t last = emu->read_data(PORTB, 1) & PB5;
  // Выполнение не должно бросать исключений (раньше падало на BRNE назад).
  for (int i = 0; i < 3000000 && toggles < 4; ++i) {
    emu->step_instruction();
    uint8_t now = emu->read_data(PORTB, 1) & PB5;
    if (now != last) {
      ++toggles;
      last = now;
    }
  }

  CHECK_EQ(toggles, 4);
  CHECK(emu->read_data(DDRB, 1) & PB5); // пин сконфигурирован как выход
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
