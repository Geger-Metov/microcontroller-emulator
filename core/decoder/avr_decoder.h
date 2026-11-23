#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "Interfaces.h"
#include "avr_instructions.h"

/**
 * Универсальный табличный декодер инструкций AVR.
 *
 * Таблица (mask/pattern) разворачивается один раз в справочник на все 65536
 * значений первого слова, поэтому decode() — это одно обращение к массиву,
 * а не линейный перебор. Семантика «первое совпадение выигрывает» сохранена.
 * Справочник общий для всех декодеров с одной и той же таблицей.
 */
class AVRDecoder : public IDecoder {
public:
  AVRDecoder(const avr::InstructionDesc *table, std::size_t count)
      : table_(table), lookup_(shared_lookup(table, count)) {}

  const avr::InstructionDesc *
  decode(uint32_t opcode, avr::DecodedInstruction &out) const override {
    int16_t idx = (*lookup_)[opcode & 0xFFFF];
    if (idx < 0)
      return nullptr;
    const auto &desc = table_[idx];
    desc.decode(opcode, out);
    return &desc;
  }

private:
  using Lookup = std::array<int16_t, 65536>;

  static std::shared_ptr<const Lookup>
  shared_lookup(const avr::InstructionDesc *table, std::size_t count) {
    static std::mutex mtx;
    static std::map<std::pair<const avr::InstructionDesc *, std::size_t>,
                    std::shared_ptr<const Lookup>>
        cache;
    std::lock_guard<std::mutex> lock(mtx);
    auto &slot = cache[{table, count}];
    if (!slot) {
      auto lut = std::make_shared<Lookup>();
      lut->fill(-1);
      for (std::size_t i = count; i-- > 0;) { // с конца: первые записи затирают
        for (uint32_t op = 0; op <= 0xFFFF; ++op) {
          if ((op & table[i].mask) == table[i].pattern)
            (*lut)[op] = static_cast<int16_t>(i);
        }
      }
      slot = std::move(lut);
    }
    return slot;
  }

  const avr::InstructionDesc *table_;
  std::shared_ptr<const Lookup> lookup_;
};
