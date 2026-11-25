#pragma once

#include <stdexcept>

// Типизированные ошибки эмулятора. Python-модуль превращает их в
// одноимённые исключения (emulator_backend.IllegalInstructionError и т. д.),
// чтобы GUI мог отличить «битую прошивку» от внутренней ошибки.

// Общий базовый класс ошибок эмулятора (в Python:
// emulator_backend.EmulatorError).
struct EmulatorError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Попытка выполнить опкод, которого нет в ATmega328P.
struct IllegalInstructionError : EmulatorError {
  using EmulatorError::EmulatorError;
};

// Обращение к адресу, где нет устройства/памяти (данные или flash).
// Наследует std::out_of_range, чтобы прежний код, ловивший его, работал.
struct MemoryAccessError : std::out_of_range {
  using std::out_of_range::out_of_range;
};

// Неверный формат Intel HEX или данные не помещаются в память МК.
struct HexFormatError : EmulatorError {
  using EmulatorError::EmulatorError;
};

// Вызов, недопустимый, пока в другом потоке идёт run().
struct EmulatorBusyError : EmulatorError {
  using EmulatorError::EmulatorError;
};
