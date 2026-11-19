#pragma once

#include <map>
#include <stdexcept>

// временно для отладки
#include <cstring>

#include "../interfaces/Interfaces.h"

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

    // Если устройство поддерживает tick — добавляем в отдельный список
    if (auto *t = dynamic_cast<ITickable *>(dev)) {
      tickables_.push_back(t);
    }
    // отладка
    fprintf(stderr, "[MemoryBus] Mapped 0x%04X-0x%04X to device at %p\n", start,
            end, dev);
  }

  // ===== IBus =====

  uint8_t read(uint32_t addr) const override {
    const auto &info = find_device(addr);
    uint32_t offset = addr - info.start;
    return info.device->read(offset);
  }

  void write(uint32_t addr, uint8_t value) override {
    auto &info = find_device(addr);
    uint32_t offset = addr - info.start;
    info.device->write(offset, value);
  }

  void tick(uint64_t cycles) override {
    for (auto *t : tickables_) {
      t->tick(cycles);
    }
  }

private:
  struct MappedInfo {
    IMemoryMapped *device; // non-owning
    uint32_t start;
    uint32_t end;
  };

  // ключ = start адрес диапазона
  std::map<uint32_t, MappedInfo> devices_;

  // отдельный список для tickable устройств
  std::vector<ITickable *> tickables_;
#if 0
  /**
   * Найти устройство по адресу
   */
  MappedInfo& find_device(uint32_t addr) {
    auto it = devices_.upper_bound(addr);
    if (it == devices_.begin()) {
      // throw std::out_of_range("MemoryBus: no device at address");
      // временно для отладки
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }

    --it;

    if (addr > it->second.end) {
      // throw std::out_of_range("MemoryBus: no device at address");
      // временно для отладки
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }

    return it->second;
  }

  const MappedInfo& find_device(uint32_t addr) const {
    auto it = devices_.upper_bound(addr);
    if (it == devices_.begin()) {
      // throw std::out_of_range("MemoryBus: no device at address");
      // временно для отладки
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }

    --it;

    if (addr > it->second.end) {
      // throw std::out_of_range("MemoryBus: no device at address");
      // временно для отладки
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }

    return it->second;
  }
#endif

  MappedInfo &find_device(uint32_t addr) {
    auto it = devices_.upper_bound(addr);
    if (it == devices_.begin()) {
      fprintf(stderr,
              "[MemoryBus] No device at 0x%04X (empty map or below first)\n",
              addr);
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }
    --it;
    if (addr > it->second.end) {
      fprintf(stderr,
              "[MemoryBus] No device at 0x%04X. Closest mapping: "
              "[0x%04X-0x%04X] -> %p\n",
              addr, it->first, it->second.end, it->second.device);
      // Выведем всю карту для анализа
      fprintf(stderr, "Full map:\n");
      for (const auto &[start, info] : devices_) {
        fprintf(stderr, "  [0x%04X-0x%04X] -> %p\n", start, info.end,
                info.device);
      }
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }
    return it->second;
  }

  const MappedInfo &find_device(uint32_t addr) const {
    auto it = devices_.upper_bound(addr);
    if (it == devices_.begin()) {
      fprintf(stderr,
              "[MemoryBus] No device at 0x%04X (empty map or below first)\n",
              addr);
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }
    --it;
    if (addr > it->second.end) {
      fprintf(stderr,
              "[MemoryBus] No device at 0x%04X. Closest mapping: "
              "[0x%04X-0x%04X] -> %p\n",
              addr, it->first, it->second.end, it->second.device);
      // Выведем всю карту для анализа
      fprintf(stderr, "Full map:\n");
      for (const auto &[start, info] : devices_) {
        fprintf(stderr, "  [0x%04X-0x%04X] -> %p\n", start, info.end,
                info.device);
      }
      throw std::out_of_range("MemoryBus: no device at address 0x" +
                              std::to_string(addr));
    }
    return it->second;
  }
};
