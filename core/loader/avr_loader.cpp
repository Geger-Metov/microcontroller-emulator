#include "avr_loader.h"

#include <cstring>
#include <stdexcept>

AVRProgramLoader::AVRProgramLoader(const AVRMemoryLayout& layout)
    : layout_(layout) {}

ProgramImage AVRProgramLoader::load(const HexImage& hex) {
  ProgramImage image;

  // Инициализация памяти пустыми значениями (0xFF)
  image.flash.resize(layout_.flashSize, 0xFF);
  image.eeprom.resize(layout_.eepromSize, 0xFF);

  for (const auto& rec : hex.records) {
    // Обрабатываем только записи данных
    if (rec.type != HexRecordType::Data) continue;

    const uint32_t addr = rec.address;
    const auto& data = rec.data;
    if (data.empty()) continue;

    // Запись во flash
    if (addr >= layout_.flashBase &&
        addr < layout_.flashBase + layout_.flashSize) {
      const uint32_t offset = addr - layout_.flashBase;
      if (offset + data.size() > layout_.flashSize) {
        throw std::runtime_error("Flash overflow at address 0x" +
                                 std::to_string(addr));
      }
      std::memcpy(image.flash.data() + offset, data.data(), data.size());
    }
    // Запись в EEPROM
    else if (addr >= layout_.eepromBase &&
             addr < layout_.eepromBase + layout_.eepromSize) {
      const uint32_t offset = addr - layout_.eepromBase;
      if (offset + data.size() > layout_.eepromSize) {
        throw std::runtime_error("EEPROM overflow at address 0x" +
                                 std::to_string(addr));
      }
      std::memcpy(image.eeprom.data() + offset, data.data(), data.size());
    }
    // Для остальных адресов (например, конфигурационные ячейки)
    // в MVP можно молча игнорировать
  }

  return image;
}
