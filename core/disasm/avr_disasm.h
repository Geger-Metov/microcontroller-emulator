#pragma once

#include <cstdint>
#include <string>

namespace avr {

struct Disassembled {
  uint8_t words;    // длина инструкции в словах (1 или 2)
  bool known;       // false — опкода нет в ATmega328P (выведено как `.word`)
  std::string text; // например "ldi r16, 0x20" или "rjmp 0x0054"
};

/**
 * Дизассемблирует одну инструкцию ATmega328P.
 *
 * @param w0        первое слово инструкции
 * @param w1        следующее слово (нужно только для 2-словных
 * LDS/STS/JMP/CALL)
 * @param word_addr адрес первого слова в **словах** — нужен для адресов
 *                  переходов RJMP/RCALL/BRxx
 *
 * Формат близок к `avr-objdump`: алиасы (`clr`, `lsl`, `sei`, `breq`, …),
 * адреса переходов — **байтовые**, как в objdump и HEX-файле. Неизвестный опкод
 * даёт `.word 0xXXXX` длиной 1 слово.
 * Набор инструкций совпадает с тем, что исполняет эмулятор (проверяется тестом
 * по всем 65536 опкодам).
 */
Disassembled disassemble(uint16_t w0, uint16_t w1, uint32_t word_addr);

} // namespace avr
