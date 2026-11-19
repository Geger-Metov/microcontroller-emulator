#include <pybind11/functional.h> // если потребуются колбэки
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // для std::vector ↔ list

#include <fstream>

#include "ATmega328p.h"
#include "EmulatorFacade.h"

namespace py = pybind11;

/**
 * Удобная Python-обёртка над IEmulatorFacade.
 * Владеет всем эмулятором и предоставляет интуитивные методы.
 */
class EmulatorWrapper {
public:
  EmulatorWrapper() {
    facade_ = ATmega328P::create(); // создаём готовый эмулятор
  }

  void step() { facade_->step_instruction(); }
  void run(uint64_t cycles) { facade_->run_for_cycles(cycles); }
  void reset() { facade_->reset(); }

  uint32_t get_pc() { return facade_->get_pc(); }
  uint32_t get_sp() { return facade_->get_sp(); }
  uint8_t get_sreg() { return facade_->get_sreg(); }

  std::vector<uint8_t> get_registers() {
    const uint8_t *regs = facade_->get_registers();
    if (!regs)
      return {};
    return std::vector<uint8_t>(regs, regs + 32);
  }

  // Чтение / запись байта в памяти данных
  uint8_t read_memory(uint32_t addr) {
    return static_cast<uint8_t>(facade_->read_data(addr, 1) & 0xFF);
  }
  void write_memory(uint32_t addr, uint8_t value) {
    facade_->write_data(addr, value, 1);
  }

  // Чтение слова из флеша
  uint16_t read_program_word(uint32_t word_addr) {
    return static_cast<uint16_t>(facade_->read_program(word_addr, 1) & 0xFFFF);
  }

  // Загрузка hex‑файла
  void load_hex(const std::string &path) {
    std::ifstream file(path);
    if (!file)
      throw std::runtime_error("Cannot open file: " + path);
    ATmega328P::load_hex(*facade_, file);
  }

  uint64_t get_cycle_count() { return facade_->get_cycle_count(); }

private:
  std::unique_ptr<IEmulatorFacade> facade_;
};

// Сам модуль Python
PYBIND11_MODULE(emulator_backend, m) {
  m.doc() = "AVR MCU Emulator Backend";

  py::class_<EmulatorWrapper>(m, "Emulator")
      .def(py::init<>(), "Create an ATmega328P emulator")
      .def("step", &EmulatorWrapper::step, "Execute one instruction")
      .def("run", &EmulatorWrapper::run, "Run for N cycles")
      .def("reset", &EmulatorWrapper::reset, "Reset the CPU and devices")
      .def("get_pc", &EmulatorWrapper::get_pc, "Program counter (words)")
      .def("get_sp", &EmulatorWrapper::get_sp, "Stack pointer")
      .def("get_sreg", &EmulatorWrapper::get_sreg, "Status register")
      .def("get_registers", &EmulatorWrapper::get_registers,
           "Return list of 32 general purpose registers")
      .def("read_memory", &EmulatorWrapper::read_memory,
           "Read a byte from data memory")
      .def("write_memory", &EmulatorWrapper::write_memory,
           "Write a byte to data memory")
      .def("read_program_word", &EmulatorWrapper::read_program_word,
           "Read a 16-bit word from flash")
      .def("load_hex", &EmulatorWrapper::load_hex,
           "Load firmware from .hex file")
      .def("get_cycle_count", &EmulatorWrapper::get_cycle_count,
           "Total CPU cycles executed");
}
