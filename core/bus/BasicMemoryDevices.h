#pragma once

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "interfaces/Interfaces.h"

// Статическое ОЗУ (SRAM)
class SRAMDevice : public IMemoryMapped, public IResettable {
public:
  explicit SRAMDevice(size_t size_bytes)
      : data_(size_bytes, 0x00), size_(size_bytes) {}

  uint8_t read(uint32_t offset) const override {
    if (offset >= size_)
      throw std::out_of_range("SRAMDevice::read: offset out of range");
    return data_[offset];
  }

  void write(uint32_t offset, uint8_t value) override {
    if (offset >= size_)
      throw std::out_of_range("SRAMDevice::write: offset out of range");
    data_[offset] = value;
  }

  uint32_t size() const override { return size_; }

  // Power-on reset: содержимое SRAM обнуляется (детерминированное состояние).
  void reset() override { std::fill(data_.begin(), data_.end(), 0); }

private:
  std::vector<uint8_t> data_;
  uint32_t size_;
};

// Flash-память (только чтение через шину)
class FlashDevice : public IMemoryMapped {
public:
  explicit FlashDevice(size_t size_bytes)
      : data_(size_bytes, 0xFF), size_(size_bytes) {}

  uint8_t read(uint32_t offset) const override {
    if (offset >= size_)
      throw std::out_of_range("FlashDevice::read offset out of range");
    return data_[offset];
  }

  void write([[maybe_unused]] uint32_t offset,
             [[maybe_unused]] uint8_t value) override {
    throw std::runtime_error("Flash is read-only via bus");
  }

  uint32_t size() const override { return size_; }

  /**
   * @brief HOST-SIDE IMAGE LOADING
   *
   * @note Not real AVR flash programming.
   */
  void load_image(uint32_t offset, const uint8_t *src, size_t len) {
    if (offset + len > size_) {
      throw std::out_of_range("FlashDevice::load_image");
    }

    std::memcpy(data_.data() + offset, src, len);
  }

  // Future real flash semantics
  void erase_page(uint32_t page);

  void write_page(uint32_t page, const uint8_t *buffer);

private:
  std::vector<uint8_t> data_;
  uint32_t size_;
};
