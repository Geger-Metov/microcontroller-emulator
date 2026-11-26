#pragma once

#include <stdexcept>

// Typed emulator errors. The Python module turns them into
// exceptions of the same name (emulator_backend.IllegalInstructionError etc.),
// so that the GUI can tell a "broken firmware" from an internal error.

// The common base class of emulator errors (in Python:
// emulator_backend.EmulatorError).
struct EmulatorError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// An attempt to execute an opcode that does not exist on the ATmega328P.
struct IllegalInstructionError : EmulatorError {
  using EmulatorError::EmulatorError;
};

// An access to an address where there is no device/memory (data or flash).
// Inherits std::out_of_range so that older code catching it keeps working.
struct MemoryAccessError : std::out_of_range {
  using std::out_of_range::out_of_range;
};

// An invalid Intel HEX format, or data that does not fit into the MCU memory.
struct HexFormatError : EmulatorError {
  using EmulatorError::EmulatorError;
};

// A call that is not allowed while run() is in progress in another thread.
struct EmulatorBusyError : EmulatorError {
  using EmulatorError::EmulatorError;
};
