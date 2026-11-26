#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ========== Base interfaces for devices on the bus ==========

// A device that can handle read/write operations by address
class IMemoryMapped {
public:
  virtual ~IMemoryMapped() = default;

  // Read a byte at an offset relative to the device's base address
  virtual uint8_t read(uint32_t offset) const = 0;

  // Write a byte at an offset relative to the device's base address
  virtual void write(uint32_t offset, uint8_t value) = 0;

  // Device size in bytes (needed by the bus)
  virtual uint32_t size() const = 0;
};

/// A device that needs time updates (peripherals, timers, CPU)
class ITickable {
public:
  virtual ~ITickable() = default;
  virtual void tick(uint64_t cycles) = 0;
};

// ========== Memory bus interface ==========
class ICodeBus {
public:
  virtual ~ICodeBus() = default;
  virtual uint16_t read_word(uint32_t word_addr) const = 0;
};

class IDataBus {
public:
  virtual ~IDataBus() = default;

  virtual uint8_t read(uint32_t addr) const = 0;
  virtual void write(uint32_t addr, uint8_t value) = 0;

  // Advance time for all devices on the bus
  virtual void tick(uint64_t cycles) = 0;
};

// The level that the outside world applies to an I/O pin.
//   Floating — nothing is connected (or a button is released): the pin follows
//              its internal pull-up (PORTx) and otherwise reads as 0.
//   Low/High — the pin is driven externally (for example a button to ground).
enum class PinLevel { Floating, Low, High };

// A port whose pins can be driven from outside (implemented by GPIO).
class IPinInput {
public:
  virtual ~IPinInput() = default;
  virtual void set_external_level(uint8_t bit, PinLevel level) = 0;
};

// A device whose state can be returned to the state after a power-on reset
class IResettable {
public:
  virtual ~IResettable() = default;
  virtual void reset() = 0;
};

// ========== CPU core interface ==========

class ICPU : public ITickable, public IResettable {
public:
  virtual ~ICPU() = default;

  // Execute one instruction
  virtual void step() = 0;

  // Attach the bus (called when the MCU is constructed)
  // Does every architecture have separate code and data buses (Harvard)?
  // virtual void attach_bus(IBus* bus) = 0;
  // virtual void attach_data_bus(IDataBus* bus) = 0;
  // virtual void attach_code_bus(ICodeBus* bus) = 0;

  // Debug information
  virtual uint32_t get_pc() const = 0;
  virtual uint32_t get_sp() const = 0;
  virtual uint8_t get_sreg() const = 0;
  virtual const uint8_t *get_registers() const = 0;

  // Apparently needed by EmulatorFacade. Does this violate the established
  // rules?
  virtual uint64_t get_cycle_count() const = 0;
  virtual uint16_t get_last_instruction_cycles() const = 0;
};

// ========== Debug interface for the GUI ==========

class IDebuggable : public IResettable {
public:
  virtual ~IDebuggable() = default;

  // Data memory access (SRAM, registers, I/O)
  virtual uint64_t read_data(uint32_t addr, uint8_t size) const = 0;
  virtual void write_data(uint32_t addr, uint64_t value, uint8_t size) = 0;

  // Program memory (flash) access
  virtual uint64_t read_program(uint32_t word_addr,
                                uint8_t num_words) const = 0;
  // Writing to flash (programming) — only through a dedicated method
  virtual void program_flash(uint32_t word_addr,
                             const std::vector<uint16_t> &words) = 0;
  // Memory map information (for display in the GUI)
  struct MemoryRegion {
    std::string name;
    uint32_t start;
    uint32_t end;
    bool readable;
    bool writable;
  };
  virtual std::vector<MemoryRegion> get_memory_map() const = 0;

  // Simulation control
  // Executes instructions until cycle_budget cycles are reached or
  // pause()/stop() is called. Returns the cycles actually executed (may be
  // slightly above the budget: an instruction cannot be interrupted midway).
  virtual uint64_t run_for_cycles(uint64_t cycle_budget) = 0;
  virtual void step_instruction() = 0;
  // Asks the current run_for_cycles() to return after the nearest instruction.
  // The only method that is safe to call from another thread while a run is
  // in progress. The request applies only to a run already in progress: a new
  // run_for_cycles() does not see it. Does nothing outside a run.
  virtual void pause() = 0;
  virtual void stop() = 0;             // same as pause() (no separate state)
  virtual bool is_running() const = 0; // is run_for_cycles() in progress
  virtual uint64_t get_cycle_count() const = 0;

  // Drive an I/O pin from outside (a button, a signal source). `port` is the
  // port letter ('B', 'D'); throws std::invalid_argument for an unknown port
  // or a bit above 7. The level stays until it is changed again, also across
  // reset().
  virtual void set_pin_level(char port, uint8_t bit, PinLevel level) = 0;

  // GUI callbacks
  using PCCallback = std::function<void(uint32_t)>;
  using MemoryChangedCallback =
      std::function<void(uint32_t addr, uint32_t size)>;
  virtual void set_on_pc_changed(PCCallback cb) = 0;
  virtual void set_on_memory_changed(MemoryChangedCallback cb) = 0;

  // CPU access for debugging
  virtual uint32_t get_pc() const = 0;
  virtual uint32_t get_sp() const = 0;
  virtual uint8_t get_sreg() const = 0;
  virtual const uint8_t *get_registers() const = 0;

  // Load bytes into flash (for initialization)
  virtual void load_flash_bytes(uint32_t offset, const uint8_t *data,
                                size_t len) = 0;
};

// ========== Emulator facade for the GUI (combines all interfaces) ==========

class IEmulatorFacade : public IDebuggable {
public:
  virtual ~IEmulatorFacade() = default;
};

class IDevice : public IMemoryMapped, public ITickable, public IResettable {
public:
  virtual ~IDevice() = default;
};

struct Register {
  uint8_t value = 0;
  std::function<uint8_t()> read;
  std::function<void(uint8_t)> write;

  void on_read(std::function<uint8_t()> f) { read = std::move(f); }
  void on_write(std::function<void(uint8_t)> f) { write = std::move(f); };
};

/**
 * Generic interrupt sink interface.
 *
 * Peripheral devices raise interrupts through this
 * abstraction instead of directly depending on CPU.
 */
class IInterruptSink {
public:
  virtual ~IInterruptSink() = default;

  /**
   * Raise interrupt vector.
   *
   * Vector must use CPU-native addressing.
   * AVR uses word-addressed vectors.
   */
  virtual void raise_interrupt(uint16_t vector) = 0;

  /**
   * Withdraw a pending request (flag cleared by software, interrupt
   * disabled in the mask register, ...). No-op if the vector is not pending.
   */
  virtual void clear_interrupt(uint16_t vector) = 0;

  /**
   * Register a callback that the CPU invokes when it actually enters the
   * vector. Used by peripherals to clear the hardware-cleared interrupt flag
   * (e.g. TOV0) on interrupt entry.
   */
  virtual void set_interrupt_ack_handler(uint16_t vector,
                                         std::function<void()> on_ack) = 0;
};

class IRegisterFile : public IResettable {
public:
  virtual ~IRegisterFile() = default;
  virtual uint8_t read_reg(uint8_t index) const = 0;
  virtual void write_reg(uint8_t index, uint8_t val) = 0;
  virtual uint8_t &ref(uint8_t index) = 0;
  virtual const uint8_t &ref(uint8_t index) const = 0;
};

class ISREG {
public:
  virtual ~ISREG() = default;
  virtual uint8_t value() const = 0;
  virtual void set(uint8_t) = 0;
  virtual bool get_flag(uint8_t bit) const = 0;
  virtual void set_flag(uint8_t bit, bool val) = 0;
};

namespace avr {
struct DecodedInstruction;
struct InstructionDesc;
} // namespace avr

class IDecoder {
public:
  virtual ~IDecoder() = default;

  virtual const avr::InstructionDesc *
  decode(uint32_t opcode, avr::DecodedInstruction &out) const = 0;
};
