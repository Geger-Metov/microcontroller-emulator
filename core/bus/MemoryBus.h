#pragma once

#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/errors.h"
#include "interfaces/Interfaces.h"

/**
 * Memory bus:
 * - owns the address → device mapping
 * - does NOT own the devices themselves
 * - routes read/write
 * - ticks devices that support ITickable
 */
class MemoryBus : public IDataBus {
public:
  MemoryBus() = default;

  /**
   * @brief Register a device over the range [start, end] (inclusive)
   * @warning the bus does not own the device (its lifetime is managed by the
   * MCU)
   */
  void map(uint32_t start, uint32_t end, IMemoryMapped *dev) {
    if (start > end) {
      throw std::logic_error("MemoryBus::map: invalid range");
    }

    uint32_t expected_size = end - start + 1;
    if (expected_size != dev->size()) {
      throw std::logic_error("MemoryBus::map size mismatch");
    }

    // Check for overlapping ranges
    for (const auto &[r_start, info] : devices_) {
      if (!(end < r_start || start > info.end)) {
        throw std::logic_error("MemoryBus::map: overlapping ranges");
      }
    }

    devices_[start] = MappedInfo{dev, start, end};

    // If the device supports tick, it receives time from the bus
    if (auto *t = dynamic_cast<ITickable *>(dev)) {
      add_tickable(t);
    }
  }

  /**
   * @brief A device that receives time from the bus but is not mapped into
   * memory as a whole (for example a timer whose individual registers are in
   * the map). Registering the same device again is ignored.
   */
  void add_tickable(ITickable *t) {
    if (std::find(tickables_.begin(), tickables_.end(), t) == tickables_.end())
      tickables_.push_back(t);
  }

  /**
   * @brief The "default" device for addresses of the range [start, end]
   * that have nothing mapped (registers that are not in the model yet).
   * The device receives the offset from start. Outside the range, access to an
   * unmapped address is still an error.
   */
  void set_fallback(uint32_t start, uint32_t end, IMemoryMapped *dev) {
    if (start > end || end - start + 1 != dev->size())
      throw std::logic_error("MemoryBus::set_fallback: size mismatch");
    fallback_ = MappedInfo{dev, start, end};
    has_fallback_ = true;
  }

  // ===== IBus =====

  uint8_t read(uint32_t addr) const override {
    const MappedInfo &info = find_device(addr);
    return info.device->read(addr - info.start);
  }

  void write(uint32_t addr, uint8_t value) override {
    const MappedInfo &info = find_device(addr);
    info.device->write(addr - info.start, value);
  }

  void tick(uint64_t cycles) override {
    for (auto *t : tickables_) {
      t->tick(cycles);
    }
  }

private:
  struct MappedInfo {
    IMemoryMapped *device = nullptr; // non-owning
    uint32_t start = 0;
    uint32_t end = 0;
  };

  // key = the start address of the range
  std::map<uint32_t, MappedInfo> devices_;
  MappedInfo fallback_;
  bool has_fallback_ = false;

  // a separate list for tickable devices
  std::vector<ITickable *> tickables_;

  const MappedInfo &find_device(uint32_t addr) const {
    auto it = devices_.upper_bound(addr);
    if (it != devices_.begin()) {
      --it;
      if (addr <= it->second.end)
        return it->second;
    }
    if (has_fallback_ && addr >= fallback_.start && addr <= fallback_.end)
      return fallback_;

    char buf[48];
    std::snprintf(buf, sizeof buf, "0x%04X", static_cast<unsigned>(addr));
    throw MemoryAccessError(std::string("MemoryBus: no device at address ") +
                            buf);
  }
};
