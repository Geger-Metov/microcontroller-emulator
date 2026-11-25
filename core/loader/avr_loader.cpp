#include <cstdio>
#include <cstring>

#include "common/errors.h"
#include "loader/avr_loader.h"

namespace {

std::string hex_addr(uint64_t addr) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "0x%06llX",
                static_cast<unsigned long long>(addr));
  return buf;
}

// Если [addr, addr+len) начинается внутри области [base, base+size), копирует
// данные и возвращает true; если выходит за конец — исключение; иначе false.
bool place(std::vector<uint8_t> &mem, uint32_t base, uint32_t size,
           uint32_t addr, const std::vector<uint8_t> &data, const char *name) {
  const uint64_t begin = base;
  const uint64_t end = begin + size;
  if (addr < begin || addr >= end)
    return false;
  const uint64_t offset = addr - begin;
  if (offset + data.size() > size)
    throw HexFormatError(std::string(name) + " overflow: record at " +
                         hex_addr(addr) + " (" + std::to_string(data.size()) +
                         " bytes) goes past the end of the memory");
  std::memcpy(mem.data() + offset, data.data(), data.size());
  return true;
}

} // namespace

AVRProgramLoader::AVRProgramLoader(const AVRMemoryLayout &layout)
    : layout_(layout) {}

ProgramImage AVRProgramLoader::load(const HexImage &hex) {
  ProgramImage image;

  // Не записанные ячейки — стёртое состояние (0xFF)
  image.flash.assign(layout_.flashSize, 0xFF);
  image.eeprom.assign(layout_.eepromSize, 0xFF);

  for (const auto &rec : hex.records) {
    if (rec.type != HexRecordType::Data || rec.data.empty())
      continue;

    if (place(image.flash, layout_.flashBase, layout_.flashSize, rec.address,
              rec.data, "Flash"))
      continue;
    if (place(image.eeprom, layout_.eepromBase, layout_.eepromSize, rec.address,
              rec.data, "EEPROM"))
      continue;

    image.warnings.push_back(std::to_string(rec.data.size()) + " byte(s) at " +
                             hex_addr(rec.address) +
                             " are outside flash and EEPROM and were ignored");
  }

  return image;
}
