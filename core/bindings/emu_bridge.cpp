#include <atomic>
#include <fstream>
#include <pybind11/functional.h> // in case callbacks are needed
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // for std::vector ↔ list
#include <stdexcept>
#include <string>

#include "common/errors.h"
#include "disasm/avr_disasm.h"
#include "mcu/avr/ATmega328p.h"

namespace py = pybind11;

/**
 * A convenient Python wrapper over IEmulatorFacade.
 * Owns the whole emulator and provides intuitive methods.
 *
 * Threads: run() releases the GIL, so it can be started from a worker thread
 * without freezing the UI. While run() is active, the other methods (except
 * stop() and is_running()) throw EmulatorBusyError: the emulator state is
 * changing at that time and reading it is not safe.
 */
class EmulatorWrapper {
public:
  EmulatorWrapper() {
    facade_ = ATmega328P::create(); // create a ready emulator
  }

  void step() {
    check_idle();
    facade_->step_instruction();
  }

  // Executes instructions until `cycles` cycles are reached or stop() is
  // called. Returns the cycles actually executed.
  uint64_t run(uint64_t cycles) {
    if (busy_.exchange(true))
      throw EmulatorBusyError("Emulator is busy: run() is already in progress");
    struct BusyReset {
      std::atomic<bool> &flag;
      ~BusyReset() { flag = false; }
    } busy_reset{busy_};

    // Destructors run in reverse order: the GIL comes back first, then
    // busy_ is cleared — an exception from the core reaches Python correctly.
    py::gil_scoped_release release;
    return facade_->run_for_cycles(cycles);
  }

  // Asks a running run() to return after the nearest instruction. Safe to call
  // from another thread; does nothing outside run().
  void stop() { facade_->pause(); }
  bool is_running() const { return busy_; }

  void reset() {
    check_idle();
    facade_->reset();
  }

  uint32_t get_pc() {
    check_idle();
    return facade_->get_pc();
  }
  uint32_t get_sp() {
    check_idle();
    return facade_->get_sp();
  }
  uint8_t get_sreg() {
    check_idle();
    return facade_->get_sreg();
  }

  std::vector<uint8_t> get_registers() {
    check_idle();
    const uint8_t *regs = facade_->get_registers();
    if (!regs)
      return {};
    return std::vector<uint8_t>(regs, regs + 32);
  }

  // The whole CPU state in one call.
  py::dict snapshot() {
    check_idle();
    py::dict d;
    d["pc"] = facade_->get_pc();
    d["sp"] = facade_->get_sp();
    d["sreg"] = facade_->get_sreg();
    d["cycles"] = facade_->get_cycle_count();
    d["registers"] = get_registers();
    return d;
  }

  // Read / write a byte of data memory
  uint8_t read_memory(uint32_t addr) {
    check_idle();
    return static_cast<uint8_t>(facade_->read_data(addr, 1) & 0xFF);
  }
  void write_memory(uint32_t addr, uint8_t value) {
    check_idle();
    facade_->write_data(addr, value, 1);
  }

  // Drive an I/O pin from outside: port "B"/"D", bit 0..7,
  // level "low" / "high" / "floating" (nothing connected). ValueError on bad
  // arguments.
  void set_pin_level(const std::string &port, int bit,
                     const std::string &level) {
    check_idle();
    if (port.size() != 1)
      throw std::invalid_argument("Port must be one letter, got '" + port +
                                  "'");
    if (bit < 0 || bit > 7)
      throw std::invalid_argument("Pin number must be 0..7, got " +
                                  std::to_string(bit));
    PinLevel lv;
    if (level == "low")
      lv = PinLevel::Low;
    else if (level == "high")
      lv = PinLevel::High;
    else if (level == "floating")
      lv = PinLevel::Floating;
    else
      throw std::invalid_argument(
          "Level must be 'low', 'high' or 'floating', got '" + level + "'");
    facade_->set_pin_level(port[0], static_cast<uint8_t>(bit), lv);
  }

  // A block of data memory as bytes (consecutive addresses; unmapped → error).
  // This is a debug read: it goes through the bus like a normal one.
  py::bytes read_memory_block(uint32_t addr, uint32_t length) {
    check_idle();
    std::string out;
    out.reserve(length);
    for (uint32_t i = 0; i < length; ++i)
      out.push_back(static_cast<char>(facade_->read_data(addr + i, 1) & 0xFF));
    return py::bytes(out);
  }

  // Read a word from flash
  uint16_t read_program_word(uint32_t word_addr) {
    check_idle();
    return static_cast<uint16_t>(facade_->read_program(word_addr, 1) & 0xFFFF);
  }

  // A block of flash as bytes by byte address (little-endian within a word).
  py::bytes read_flash(uint32_t byte_addr, uint32_t length) {
    check_idle();
    std::string out;
    out.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
      const uint32_t a = byte_addr + i;
      const uint16_t w =
          static_cast<uint16_t>(facade_->read_program(a / 2, 1) & 0xFFFF);
      out.push_back(static_cast<char>((a & 1) ? (w >> 8) : (w & 0xFF)));
    }
    return py::bytes(out);
  }

  // Disassembles up to `count` instructions starting at word `word_addr`.
  // Returns [{addr (words), size (words), words, text, known}]; stops at the
  // end of flash. Jump addresses in text are byte addresses.
  py::list disassemble(uint32_t word_addr, uint32_t count) {
    check_idle();
    py::list rows;
    uint32_t addr = word_addr;
    for (uint32_t i = 0; i < count; ++i) {
      uint16_t w0;
      try {
        w0 = static_cast<uint16_t>(facade_->read_program(addr, 1) & 0xFFFF);
      } catch (const MemoryAccessError &) {
        break; // end of flash
      }
      uint16_t w1 = 0xFFFF;
      try {
        w1 = static_cast<uint16_t>(facade_->read_program(addr + 1, 1) & 0xFFFF);
      } catch (const MemoryAccessError &) {
        // the last instruction of flash: there is no second word
      }
      const avr::Disassembled d = avr::disassemble(w0, w1, addr);
      py::list words;
      words.append(w0);
      if (d.words == 2)
        words.append(w1);
      py::dict row;
      row["addr"] = addr;
      row["size"] = d.words;
      row["words"] = words;
      row["text"] = d.text;
      row["known"] = d.known;
      rows.append(row);
      addr += d.words;
    }
    return rows;
  }

  // Data-space regions: [{name, start, end, readable, writable}].
  py::list get_memory_map() {
    check_idle();
    py::list regions;
    for (const auto &r : facade_->get_memory_map()) {
      py::dict d;
      d["name"] = r.name;
      d["start"] = r.start;
      d["end"] = r.end;
      d["readable"] = r.readable;
      d["writable"] = r.writable;
      regions.append(d);
    }
    return regions;
  }

  // Load a hex file. Returns a list of warnings (for example fuse-bit data
  // that the emulator does not need). A format error is a HexFormatError.
  std::vector<std::string> load_hex(const std::string &path) {
    check_idle();
    std::ifstream file(path);
    if (!file)
      throw std::runtime_error("Cannot open file: " + path);
    return ATmega328P::load_hex(*facade_, file);
  }

  uint64_t get_cycle_count() {
    check_idle();
    return facade_->get_cycle_count();
  }

private:
  void check_idle() const {
    if (busy_)
      throw EmulatorBusyError(
          "Emulator is busy: run() is in progress; call stop() first");
  }

  std::unique_ptr<IEmulatorFacade> facade_;
  std::atomic<bool> busy_{false};
};

// The Python module itself
PYBIND11_MODULE(emulator_backend, m) {
  m.doc() = "AVR MCU Emulator Backend";

  // Typed exceptions: the GUI tells firmware errors from the rest.
  // The base class is registered first — derived ones are checked before it.
  auto &base = py::register_exception<EmulatorError>(m, "EmulatorError");
  py::register_exception<IllegalInstructionError>(m, "IllegalInstructionError",
                                                  base.ptr());
  py::register_exception<HexFormatError>(m, "HexFormatError", base.ptr());
  py::register_exception<EmulatorBusyError>(m, "EmulatorBusyError", base.ptr());
  py::register_exception<MemoryAccessError>(m, "MemoryAccessError", base.ptr());

  py::class_<EmulatorWrapper>(m, "Emulator")
      .def(py::init<>(), "Create an ATmega328P emulator")
      .def("step", &EmulatorWrapper::step, "Execute one instruction")
      .def("run", &EmulatorWrapper::run, py::arg("cycles"),
           "Run for N cycles (releases the GIL). Returns the cycles actually "
           "executed; stops early after stop().")
      .def("stop", &EmulatorWrapper::stop,
           "Ask a running run() to return after the current instruction "
           "(thread-safe; no effect when idle)")
      .def("is_running", &EmulatorWrapper::is_running,
           "True while run() is in progress")
      .def("reset", &EmulatorWrapper::reset, "Reset the CPU and devices")
      .def("get_pc", &EmulatorWrapper::get_pc, "Program counter (words)")
      .def("get_sp", &EmulatorWrapper::get_sp, "Stack pointer")
      .def("get_sreg", &EmulatorWrapper::get_sreg, "Status register")
      .def("get_registers", &EmulatorWrapper::get_registers,
           "Return list of 32 general purpose registers")
      .def("snapshot", &EmulatorWrapper::snapshot,
           "CPU state in one call: dict(pc, sp, sreg, cycles, registers)")
      .def("read_memory", &EmulatorWrapper::read_memory,
           "Read a byte from data memory")
      .def("write_memory", &EmulatorWrapper::write_memory,
           "Write a byte to data memory")
      .def("set_pin_level", &EmulatorWrapper::set_pin_level, py::arg("port"),
           py::arg("bit"), py::arg("level"),
           "Drive an I/O pin from outside: level is 'low', 'high' or "
           "'floating' (a button to ground: pressed = 'low', released = "
           "'floating')")
      .def("read_memory_block", &EmulatorWrapper::read_memory_block,
           py::arg("addr"), py::arg("length"),
           "Read `length` bytes of data memory as bytes")
      .def("read_program_word", &EmulatorWrapper::read_program_word,
           "Read a 16-bit word from flash")
      .def("read_flash", &EmulatorWrapper::read_flash, py::arg("byte_addr"),
           py::arg("length"), "Read `length` bytes of flash as bytes")
      .def("disassemble", &EmulatorWrapper::disassemble, py::arg("word_addr"),
           py::arg("count"),
           "Disassemble up to `count` instructions from word address: list of "
           "dict(addr, size, words, text, known)")
      .def("get_memory_map", &EmulatorWrapper::get_memory_map,
           "Data-space regions: list of dict(name, start, end, readable, "
           "writable); end is inclusive")
      .def("load_hex", &EmulatorWrapper::load_hex,
           "Load firmware from .hex file; returns a list of warnings")
      .def("get_cycle_count", &EmulatorWrapper::get_cycle_count,
           "Total CPU cycles executed");
}
