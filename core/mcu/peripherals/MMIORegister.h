#pragma once

#include <functional>
#include <stdexcept>

#include "interfaces/Interfaces.h"

class MMIORegister : public IMemoryMapped {
public:
  using ReadHandler = std::function<uint8_t()>;
  using WriteHandler = std::function<void(uint8_t)>;

  MMIORegister() = default;
  explicit MMIORegister(uint8_t initial) : value_(initial) {}

  void on_read(ReadHandler h) { read_handler_ = std::move(h); }
  void on_write(WriteHandler h) { write_handler_ = std::move(h); }

  uint8_t read(uint32_t offset) const override {
    if (offset != 0)
      throw std::out_of_range("MMIORegister read offset");
    if (read_handler_)
      return read_handler_();
    return value_;
  }

  void write(uint32_t offset, uint8_t value) override {
    if (offset != 0)
      throw std::out_of_range("MMIORegister write offset");
    if (write_handler_) {
      write_handler_(value);
      return;
    }
    value_ = value;
  }

  uint32_t size() const override { return 1; }
  uint8_t value() const { return value_; }
  void set(uint8_t v) { value_ = v; }

private:
  uint8_t value_ = 0;
  ReadHandler read_handler_;
  WriteHandler write_handler_;
};
