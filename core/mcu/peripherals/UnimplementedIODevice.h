#pragma once

#include <set>

#include "Interfaces.h"

/**
 * Заглушка для ещё не реализованных регистров ввода-вывода.
 *
 * Чтение возвращает 0, запись игнорируется (RAZ/WI), а не бросает исключение:
 * реальные прошивки (например, Arduino init()) при старте обращаются к десяткам
 * регистров UART/ADC/SPI/TWI/..., и эмулятор не должен падать из-за этого.
 * Все обращённые адреса запоминаются и доступны через touched() для диагностики
 * («что прошивка ожидает от периферии, которой ещё нет»).
 */
class UnimplementedIODevice : public IMemoryMapped {
public:
  explicit UnimplementedIODevice(uint32_t size) : size_(size) {}

  uint8_t read(uint32_t offset) const override {
    touched_.insert(offset);
    return 0;
  }
  void write(uint32_t offset, uint8_t) override { touched_.insert(offset); }
  uint32_t size() const override { return size_; }

  /// Смещения (от начала диапазона), к которым обращалась программа.
  const std::set<uint32_t> &touched() const { return touched_; }

private:
  uint32_t size_;
  mutable std::set<uint32_t> touched_;
};
