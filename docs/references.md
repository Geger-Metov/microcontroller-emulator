# References

The emulator follows these Microchip (formerly Atmel) documents. They are copyrighted and **not part of
this repository** — download them from the manufacturer:

| Document                                                   | Used for                                             |
| ---------------------------------------------------------- | ---------------------------------------------------- |
| ATmega48A/PA/88A/PA/168A/PA/328/P datasheet (DS40002061)   | memory map, I/O registers, interrupt vectors, Timer0 |
| AVR Instruction Set Manual (DS40002198, older: Atmel-0856) | instruction semantics, flag formulas, cycle counts   |

Product page with the datasheet: <https://www.microchip.com/en-us/product/ATmega328P>.
Search for the document number (for example `DS40002198`) on microchip.com to find the instruction manual.

Other useful tools:

- `avr-gcc` / `avr-objcopy` / `avr-objdump` — build firmware, produce `.hex`, disassemble for comparison
  with the emulator's disassembler.
- [simavr](https://github.com/buserror/simavr) — another AVR simulator, useful as a reference for comparison.
