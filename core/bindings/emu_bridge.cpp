#include <atomic>
#include <fstream>
#include <pybind11/functional.h> // если потребуются колбэки
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // для std::vector ↔ list

#include "common/errors.h"
#include "disasm/avr_disasm.h"
#include "mcu/avr/ATmega328p.h"

namespace py = pybind11;

/**
 * Удобная Python-обёртка над IEmulatorFacade.
 * Владеет всем эмулятором и предоставляет интуитивные методы.
 *
 * Потоки: run() отпускает GIL, поэтому его можно запускать из рабочего потока,
 * не замораживая интерфейс. Пока run() идёт, остальные методы (кроме stop() и
 * is_running()) бросают EmulatorBusyError — состояние эмулятора в это время
 * меняется и читать его небезопасно.
 */
class EmulatorWrapper {
public:
  EmulatorWrapper() {
    facade_ = ATmega328P::create(); // создаём готовый эмулятор
  }

  void step() {
    check_idle();
    facade_->step_instruction();
  }

  // Выполняет инструкции, пока не набрано `cycles` тактов или не вызван stop().
  // Возвращает фактически выполненные такты.
  uint64_t run(uint64_t cycles) {
    if (busy_.exchange(true))
      throw EmulatorBusyError("Emulator is busy: run() is already in progress");
    struct BusyReset {
      std::atomic<bool> &flag;
      ~BusyReset() { flag = false; }
    } busy_reset{busy_};

    // Деструкторы идут в обратном порядке: сначала вернётся GIL, потом
    // сбросится busy_ — исключение из ядра доедет до Python корректно.
    py::gil_scoped_release release;
    return facade_->run_for_cycles(cycles);
  }

  // Просит идущий run() выйти после ближайшей инструкции. Безопасно звать из
  // другого потока; вне run() ничего не делает.
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

  // Всё состояние CPU одним вызовом.
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

  // Чтение / запись байта в памяти данных
  uint8_t read_memory(uint32_t addr) {
    check_idle();
    return static_cast<uint8_t>(facade_->read_data(addr, 1) & 0xFF);
  }
  void write_memory(uint32_t addr, uint8_t value) {
    check_idle();
    facade_->write_data(addr, value, 1);
  }

  // Блок памяти данных как bytes (адреса подряд; неотображённый → ошибка).
  // Это отладочное чтение: оно проходит через шину, как обычное.
  py::bytes read_memory_block(uint32_t addr, uint32_t length) {
    check_idle();
    std::string out;
    out.reserve(length);
    for (uint32_t i = 0; i < length; ++i)
      out.push_back(static_cast<char>(facade_->read_data(addr + i, 1) & 0xFF));
    return py::bytes(out);
  }

  // Чтение слова из флеша
  uint16_t read_program_word(uint32_t word_addr) {
    check_idle();
    return static_cast<uint16_t>(facade_->read_program(word_addr, 1) & 0xFFFF);
  }

  // Блок flash как bytes по байтовому адресу (little-endian внутри слова).
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

  // Дизассемблирует до `count` инструкций начиная со слова `word_addr`.
  // Возвращает [{addr (слова), size (слов), words, text, known}]; доходит до
  // конца flash и останавливается. Адреса переходов в text — байтовые.
  py::list disassemble(uint32_t word_addr, uint32_t count) {
    check_idle();
    py::list rows;
    uint32_t addr = word_addr;
    for (uint32_t i = 0; i < count; ++i) {
      uint16_t w0;
      try {
        w0 = static_cast<uint16_t>(facade_->read_program(addr, 1) & 0xFFFF);
      } catch (const MemoryAccessError &) {
        break; // конец flash
      }
      uint16_t w1 = 0xFFFF;
      try {
        w1 = static_cast<uint16_t>(facade_->read_program(addr + 1, 1) & 0xFFFF);
      } catch (const MemoryAccessError &) {
        // последняя инструкция flash: второго слова нет
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

  // Регионы пространства данных: [{name, start, end, readable, writable}].
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

  // Загрузка hex‑файла. Возвращает список предупреждений (например, данные
  // fuse-битов, которые эмулятору не нужны). Ошибка формата — HexFormatError.
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

// Сам модуль Python
PYBIND11_MODULE(emulator_backend, m) {
  m.doc() = "AVR MCU Emulator Backend";

  // Типизированные исключения: GUI отличает ошибки прошивки от прочих.
  // Базовый класс регистрируется первым — производные проверяются раньше него.
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
