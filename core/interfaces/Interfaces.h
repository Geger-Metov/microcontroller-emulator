#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ========== Базовые интерфейсы для устройств на шине ==========

// Устройство, способное обрабатывать операции чтения/записи по адресу
class IMemoryMapped {
public:
  virtual ~IMemoryMapped() = default;

  // Чтение байта по смещению относительно базового адреса устройства
  virtual uint8_t read(uint32_t offset) const = 0;

  // Запись байта по смещению относительно базового адреса устройства
  virtual void write(uint32_t offset, uint8_t value) = 0;

  // Размер устройства в байтах (необходим для шины)
  virtual uint32_t size() const = 0;
};

/// Устройство, которому нужно обновление времени (периферия, таймеры, CPU)
class ITickable {
public:
  virtual ~ITickable() = default;
  virtual void tick(uint64_t cycles) = 0;
};

// ========== Интерфейс шины памяти ==========
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

  // Продвинуть время для всех устройств на шине
  virtual void tick(uint64_t cycles) = 0;
};

// Устройство, состояние которого можно вернуть к состоянию после power-on reset
class IResettable {
public:
  virtual ~IResettable() = default;
  virtual void reset() = 0;
};

// ========== Интерфейс процессорного ядра ==========

class ICPU : public ITickable, public IResettable {
public:
  virtual ~ICPU() = default;

  // Выполнить одну инструкцию
  virtual void step() = 0;

  // Подключить шину (вызывается при конструировании MCU)
  // У всех ли Гарвардская архитектура?
  // virtual void attach_bus(IBus* bus) = 0;
  // virtual void attach_data_bus(IDataBus* bus) = 0;
  // virtual void attach_code_bus(ICodeBus* bus) = 0;

  // Отладочная информация
  virtual uint32_t get_pc() const = 0;
  virtual uint32_t get_sp() const = 0;
  virtual uint8_t get_sreg() const = 0;
  virtual const uint8_t *get_registers() const = 0;

  // Вроде как, требуется в EmulatorFacade. Не нарушает ли это установленные
  // правила?
  virtual uint64_t get_cycle_count() const = 0;
  virtual uint16_t get_last_instruction_cycles() const = 0;
};

// ========== Интерфейс отладки для GUI ==========

class IDebuggable : public IResettable {
public:
  virtual ~IDebuggable() = default;

  // Доступ к памяти данных (SRAM, регистры, IO)
  virtual uint64_t read_data(uint32_t addr, uint8_t size) const = 0;
  virtual void write_data(uint32_t addr, uint64_t value, uint8_t size) = 0;

  // Доступ к памяти программ (flash)
  virtual uint64_t read_program(uint32_t word_addr,
                                uint8_t num_words) const = 0;
  // Запись во flash (программирование) — только через специальный метод
  virtual void program_flash(uint32_t word_addr,
                             const std::vector<uint16_t> &words) = 0;
  // Информация о карте памяти (для отображения в GUI)
  struct MemoryRegion {
    std::string name;
    uint32_t start;
    uint32_t end;
    bool readable;
    bool writable;
  };
  virtual std::vector<MemoryRegion> get_memory_map() const = 0;

  // Управление симуляцией
  virtual void run_for_cycles(uint64_t cycle_budget) = 0;
  virtual void step_instruction() = 0;
  virtual void pause() = 0;
  virtual void stop() = 0;
  virtual uint64_t get_cycle_count() const = 0;

  // Колбэки для GUI
  using PCCallback = std::function<void(uint32_t)>;
  using MemoryChangedCallback =
      std::function<void(uint32_t addr, uint32_t size)>;
  virtual void set_on_pc_changed(PCCallback cb) = 0;
  virtual void set_on_memory_changed(MemoryChangedCallback cb) = 0;

  // Доступ к CPU для отладки
  virtual uint32_t get_pc() const = 0;
  virtual uint32_t get_sp() const = 0;
  virtual uint8_t get_sreg() const = 0;
  virtual const uint8_t *get_registers() const = 0;

  // Загрузка байт во flash (для инициализации)
  virtual void load_flash_bytes(uint32_t offset, const uint8_t *data,
                                size_t len) = 0;
};

// ========== Фасад эмулятора для GUI (объединяет все интерфейсы) ==========

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
