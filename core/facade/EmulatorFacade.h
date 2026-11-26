#pragma once

#include <atomic>
#include <map>
#include <memory>

#include "interfaces/Interfaces.h"

class FlashDevice;

/**
 * @brief Owns all components, provides the IDebuggable interface
 */
class EmulatorFacade : public IEmulatorFacade {
public:
  EmulatorFacade();
  ~EmulatorFacade() override;

  // Setup (called when the MCU is assembled)
  void set_cpu(std::unique_ptr<ICPU> cpu);
  void set_data_bus(std::unique_ptr<IDataBus> bus);
  void set_code_bus(std::unique_ptr<ICodeBus> bus);
  void set_flash_device(FlashDevice *flash); // does not own
  void add_memory_device(std::unique_ptr<IMemoryMapped> dev);
  // Add a memory region for debugging (for example SRAM, Flash)
  void add_memory_region(const std::string &name, uint32_t start, uint32_t end,
                         bool readable, bool writable);

  // Register a port that can be driven from outside; does not own it.
  void add_pin_port(char name, IPinInput *port);

  // IDebuggable
  uint64_t read_data(uint32_t addr, uint8_t size) const override;
  void write_data(uint32_t addr, uint64_t value, uint8_t size) override;
  uint64_t read_program(uint32_t word_addr, uint8_t num_words) const override;
  void program_flash(uint32_t word_addr,
                     const std::vector<uint16_t> &words) override;

  std::vector<MemoryRegion> get_memory_map() const override;
  uint64_t run_for_cycles(uint64_t cycle_budget) override;
  void step_instruction() override;
  void pause() override;
  void reset() override;
  void stop() override;
  bool is_running() const override { return running_; }
  uint64_t get_cycle_count() const override;
  void set_on_pc_changed(PCCallback cb) override;
  void set_on_memory_changed(MemoryChangedCallback cb) override;

  void register_reset_device(IResettable *dev);
  void add_owned_device(std::unique_ptr<IResettable> dev);

  void load_flash_bytes(uint32_t offset, const uint8_t *data,
                        size_t len) override;
  void set_pin_level(char port, uint8_t bit, PinLevel level) override;

  // accessors
  uint32_t get_pc() const override { return cpu_ ? cpu_->get_pc() : 0; }
  uint32_t get_sp() const override { return cpu_ ? cpu_->get_sp() : 0; }
  uint8_t get_sreg() const override { return cpu_ ? cpu_->get_sreg() : 0; }
  const uint8_t *get_registers() const override {
    return cpu_ ? cpu_->get_registers() : nullptr;
  }

private:
  std::unique_ptr<ICPU> cpu_;
  std::unique_ptr<IDataBus> data_bus_;
  std::unique_ptr<ICodeBus> code_bus_;
  FlashDevice *flash_dev_ = nullptr;
  std::map<char, IPinInput *> pin_ports_; // not owned
  std::vector<std::unique_ptr<IMemoryMapped>>
      memory_devices_;                       // we own the memory
  std::vector<IResettable *> reset_devices_; // devices that support
                                             // reset
  std::vector<std::unique_ptr<IResettable>>
      owned_devices_; // ideally a different interface should be used

  // Memory map for debugging
  std::vector<MemoryRegion> memory_regions_;

  // Simulation control
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  // GUI callbacks
  PCCallback on_pc_changed_;
  MemoryChangedCallback on_memory_changed_;

  // Helper methods
  void notify_pc_changed();
  void notify_memory_changed(uint32_t addr, uint32_t size);
};
