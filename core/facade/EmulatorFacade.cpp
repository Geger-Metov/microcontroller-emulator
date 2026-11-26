#include <stdexcept>
#include <string>

#include "bus/BasicMemoryDevices.h"
#include "facade/EmulatorFacade.h"

EmulatorFacade::EmulatorFacade() {}
EmulatorFacade::~EmulatorFacade() { stop(); }

void EmulatorFacade::set_cpu(std::unique_ptr<ICPU> cpu) {
  cpu_ = std::move(cpu);
}

void EmulatorFacade::set_data_bus(std::unique_ptr<IDataBus> bus) {
  data_bus_ = std::move(bus);
}

void EmulatorFacade::set_code_bus(std::unique_ptr<ICodeBus> bus) {
  code_bus_ = std::move(bus);
}

void EmulatorFacade::set_flash_device(FlashDevice *flash) {
  flash_dev_ = flash;
}

void EmulatorFacade::add_memory_device(std::unique_ptr<IMemoryMapped> dev) {
  memory_devices_.push_back(std::move(dev));
}

void EmulatorFacade::add_memory_region(const std::string &name, uint32_t start,
                                       uint32_t end, bool readable,
                                       bool writable) {
  memory_regions_.push_back(MemoryRegion{name, start, end, readable, writable});
}

uint64_t EmulatorFacade::read_data(uint32_t addr, uint8_t size) const {
  if (!data_bus_)
    throw std::runtime_error("Data bus not set");
  uint64_t res = 0;
  for (uint8_t i = 0; i < size; ++i)
    res |= static_cast<uint64_t>(data_bus_->read(addr + i)) << (i * 8);
  return res;
}

void EmulatorFacade::write_data(uint32_t addr, uint64_t value, uint8_t size) {
  if (!data_bus_)
    throw std::runtime_error("Data bus not set");
  for (uint8_t i = 0; i < size; ++i) {
    data_bus_->write(addr + i, static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
  }
  notify_memory_changed(addr, size);
}

uint64_t EmulatorFacade::read_program(uint32_t word_addr,
                                      uint8_t num_words) const {
  if (!code_bus_)
    throw std::runtime_error("Code bus not set");
  uint64_t res = 0;
  if (num_words > 4)
    num_words = 4;
  for (uint8_t i = 0; i < num_words; ++i) {
    uint16_t w = code_bus_->read_word(word_addr + i);
    res |= static_cast<uint64_t>(w) << (i * 16);
  }
  return res;
}

void EmulatorFacade::program_flash(uint32_t word_addr,
                                   const std::vector<uint16_t> &words) {
  if (!flash_dev_)
    throw std::runtime_error("Flash device not set");

  std::vector<uint8_t> bytes;
  bytes.reserve(words.size() * 2);

  for (uint16_t w : words) {
    bytes.push_back(static_cast<uint8_t>(w & 0xFF));
    bytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
  }
  uint32_t byte_addr = word_addr << 1;
  flash_dev_->load_image(byte_addr, bytes.data(), bytes.size());
  notify_memory_changed(byte_addr, static_cast<uint32_t>(bytes.size()));
}

std::vector<IDebuggable::MemoryRegion> EmulatorFacade::get_memory_map() const {
  return memory_regions_;
}

void EmulatorFacade::step_instruction() {
  if (!cpu_)
    throw std::runtime_error("CPU not set");
  cpu_->step();
  uint64_t cycles = cpu_->get_last_instruction_cycles();

  if (data_bus_)
    data_bus_->tick(cycles);
  notify_pc_changed();
}

uint64_t EmulatorFacade::run_for_cycles(uint64_t cycle_budget) {
  if (!cpu_)
    throw std::runtime_error("CPU not set");

  // The order matters: first clear the old request, then declare the run
  // in progress — otherwise a pause() arriving between the two steps would be
  // lost.
  stop_requested_ = false;

  // running_ is cleared on an exception too (for example, Illegal instruction).
  struct RunningGuard {
    std::atomic<bool> &flag;
    explicit RunningGuard(std::atomic<bool> &f) : flag(f) { flag = true; }
    ~RunningGuard() { flag = false; }
  } guard(running_);

  uint64_t executed = 0;
  while (executed < cycle_budget &&
         !stop_requested_.load(std::memory_order_relaxed)) {
    cpu_->step();
    uint64_t cycles = cpu_->get_last_instruction_cycles();
    executed += cycles;
    if (data_bus_)
      data_bus_->tick(cycles);
  }
  notify_pc_changed();
  return executed;
}

void EmulatorFacade::register_reset_device(IResettable *dev) {
  if (!dev)
    return;
  reset_devices_.push_back(dev);
}

void EmulatorFacade::add_owned_device(std::unique_ptr<IResettable> dev) {
  owned_devices_.push_back(std::move(dev));
}

void EmulatorFacade::reset() {
  pause();

  for (auto &dev : reset_devices_)
    dev->reset();

  if (cpu_)
    cpu_->reset();

  notify_pc_changed();
  notify_memory_changed(0, 0xFFFFFFFF);
}

void EmulatorFacade::pause() {
  if (running_)
    stop_requested_ = true;
}

void EmulatorFacade::stop() { pause(); }

uint64_t EmulatorFacade::get_cycle_count() const {
  if (!cpu_)
    throw std::runtime_error("CPU not set");
  return cpu_->get_cycle_count();
}

void EmulatorFacade::set_on_pc_changed(PCCallback cb) {
  on_pc_changed_ = std::move(cb);
}

void EmulatorFacade::set_on_memory_changed(MemoryChangedCallback cb) {
  on_memory_changed_ = std::move(cb);
}

void EmulatorFacade::notify_pc_changed() {
  if (on_pc_changed_)
    on_pc_changed_(cpu_->get_pc());
}

void EmulatorFacade::notify_memory_changed(uint32_t addr, uint32_t size) {
  if (on_memory_changed_)
    on_memory_changed_(addr, size);
}

void EmulatorFacade::load_flash_bytes(uint32_t offset, const uint8_t *data,
                                      size_t len) {
  if (!flash_dev_)
    throw std::runtime_error("Flash device not set");
  flash_dev_->load_image(offset, data, len);
}

void EmulatorFacade::add_pin_port(char name, IPinInput *port) {
  if (port)
    pin_ports_[name] = port;
}

void EmulatorFacade::set_pin_level(char port, uint8_t bit, PinLevel level) {
  const auto it = pin_ports_.find(port);
  if (it == pin_ports_.end())
    throw std::invalid_argument(std::string("No such port: '") + port + "'");
  if (bit > 7)
    throw std::invalid_argument("Pin number must be 0..7, got " +
                                std::to_string(bit));
  it->second->set_external_level(bit, level);
}
