#pragma once

#include <map>
#include <stdexcept>

#include "../common/errors.h"
#include "Interfaces.h"

class CodeBus : public ICodeBus {
public:
  void map(uint32_t start_word, uint32_t end_word, IMemoryMapped *dev) {
    if (start_word > end_word) {
      throw std::logic_error("CodeBus::map invalid range");
    }

    // overlap check
    for (const auto &[region_start, info] : regions_) {
      if (!(end_word < region_start || start_word > info.end_word)) {
        throw std::logic_error("CodeBus::map overlapping regions");
      }
    }

    regions_[start_word] = Region{dev, start_word, end_word};
  }

  uint16_t read_word(uint32_t word_addr) const override {
    const auto &r = find_region(word_addr);

    uint32_t byte_offset = (word_addr - r.start_word) * 2;
    uint8_t lo = r.device->read(byte_offset);
    uint8_t hi = r.device->read(byte_offset + 1);

    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
  }

private:
  struct Region {
    IMemoryMapped *device;
    uint32_t start_word;
    uint32_t end_word;
  };

  std::map<uint32_t, Region> regions_;

  Region &find_region(uint32_t addr) {
    auto it = regions_.upper_bound(addr);

    if (it == regions_.begin()) {
      throw MemoryAccessError("CodeBus: unmapped address");
    }

    --it;

    if (addr > it->second.end_word) {
      throw MemoryAccessError("CodeBus: unmapped address");
    }

    return it->second;
  }

  const Region &find_region(uint32_t addr) const {
    auto it = regions_.upper_bound(addr);

    if (it == regions_.begin()) {
      throw MemoryAccessError("CodeBus: unmapped address");
    }

    --it;

    if (addr > it->second.end_word) {
      throw MemoryAccessError("CodeBus: unmapped address");
    }

    return it->second;
  }
};
