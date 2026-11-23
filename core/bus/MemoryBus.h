#pragma once

#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "Interfaces.h"

/**
 * Шина памяти:
 * - владеет mapping адресов → устройств
 * - НЕ владеет самими устройствами
 * - маршрутизирует read/write
 * - тикает устройства, поддерживающие ITickable
 */
class MemoryBus : public IDataBus {
public:
  MemoryBus() = default;

  /**
   * @brief Зарегистрировать устройство в диапазоне [start, end] (включительно)
   * @warning bus не владеет устройством (lifetime(ЖЦ) управляется MCU)
   */
  void map(uint32_t start, uint32_t end, IMemoryMapped *dev) {
    if (start > end) {
      throw std::logic_error("MemoryBus::map: invalid range");
    }

    uint32_t expected_size = end - start + 1;
    if (expected_size != dev->size()) {
      throw std::logic_error("MemoryBus::map size mismatch");
    }

    // Проверка на пересечение диапазонов
    for (const auto &[r_start, info] : devices_) {
      if (!(end < r_start || start > info.end)) {
        throw std::logic_error("MemoryBus::map: overlapping ranges");
      }
    }

    devices_[start] = MappedInfo{dev, start, end};

    // Если устройство поддерживает tick — оно получает время от шины
    if (auto *t = dynamic_cast<ITickable *>(dev)) {
      add_tickable(t);
    }
  }

  /**
   * @brief Устройство, получающее время от шины, но не отображённое в память
   * целиком (например, таймер, у которого в карте лежат отдельные регистры).
   * Повторная регистрация одного и того же устройства игнорируется.
   */
  void add_tickable(ITickable *t) {
    if (std::find(tickables_.begin(), tickables_.end(), t) == tickables_.end())
      tickables_.push_back(t);
  }

  /**
   * @brief Устройство «по умолчанию» для адресов диапазона [start, end],
   * на которые ничего не отображено (регистры, которых ещё нет в модели).
   * Устройство получает смещение от start. Вне диапазона обращение к
   * неотображённому адресу по-прежнему считается ошибкой.
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

  // ключ = start адрес диапазона
  std::map<uint32_t, MappedInfo> devices_;
  MappedInfo fallback_;
  bool has_fallback_ = false;

  // отдельный список для tickable устройств
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
    throw std::out_of_range(std::string("MemoryBus: no device at address ") +
                            buf);
  }
};
