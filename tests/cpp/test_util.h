// Общие помощники для тестов: сборка эмулятора с небольшой программой во flash.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "ATmega328p.h"
#include "EmulatorFacade.h"

namespace testutil {

// Адреса регистров в пространстве данных
constexpr uint32_t TIFR0 = 0x35;
constexpr uint32_t TCCR0A = 0x44;
constexpr uint32_t TCCR0B = 0x45;
constexpr uint32_t TCNT0 = 0x46;
constexpr uint32_t OCR0A = 0x47;
constexpr uint32_t OCR0B = 0x48;
constexpr uint32_t TIMSK0 = 0x6E;
constexpr uint32_t SREG = 0x5F;

struct Chunk {
  uint32_t word_addr;
  std::vector<uint16_t> words;
};

inline std::unique_ptr<IEmulatorFacade> make_emu(std::vector<Chunk> chunks) {
  auto emu = ATmega328P::create();
  for (const Chunk &c : chunks)
    emu->program_flash(c.word_addr, c.words);
  emu->reset();
  return emu;
}

// --- кодировщики опкодов ---
inline uint16_t rjmp(int k) { return 0xC000 | (k & 0x0FFF); }
inline uint16_t ldi(int rd, int k) {
  return 0xE000 | ((k & 0xF0) << 4) | (((rd - 16) & 0xF) << 4) | (k & 0x0F);
}
inline uint16_t out(int io, int rr) {
  return 0xB800 | ((io & 0x30) << 5) | ((rr & 0x10) << 4) | ((rr & 0x0F) << 4) |
         (io & 0x0F);
}
inline uint16_t inc(int rd) { return 0x9403 | (rd << 4); }
inline uint16_t mov(int rd, int rr) {
  return 0x2C00 | ((rd & 0x10) << 4) | ((rr & 0x10) << 5) | ((rd & 0x0F) << 4) |
         (rr & 0x0F);
}
// Rd,Rr: 0000 00rd dddd rrrr с заданной базой (ADD=0x0C00, SUB=0x1800, ...)
inline uint16_t rd_rr(uint16_t base, int rd, int rr) {
  return base | ((rr & 0x10) << 5) | (rd << 4) | (rr & 0x0F);
}
// Rd(16..31),K: CPI=0x3000, SBCI=0x4000, SUBI=0x5000, ORI=0x6000, ANDI=0x7000
inline uint16_t rd_k(uint16_t base, int rd, int k) {
  return base | ((k & 0xF0) << 4) | ((rd - 16) << 4) | (k & 0x0F);
}
// Однооперандные: COM=0x9400, NEG=0x9401, SWAP=0x9402, INC=0x9403, ...
inline uint16_t unary(uint16_t base, int rd) { return base | (rd << 4); }

inline uint16_t sbiw_adiw(uint16_t base, int rd_low, int k) {
  return base | ((k & 0x30) << 2) | (((rd_low - 24) / 2) << 4) | (k & 0x0F);
}

constexpr uint16_t RETI = 0x9518;
constexpr uint16_t NOP = 0x0000;

} // namespace testutil
