# Glossary and AVR conventions

| Term                            | What it is                                                                                                                                         |
| ------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| **PC is in words**              | The program counter counts 16-bit words. A (byte) address in `objdump`/HEX = PC × 2. So the `TIMER0_OVF` vector = word `0x20` = byte `0x40`.       |
| **Two-word instructions**       | `JMP`, `CALL`, `LDS`, `STS` take 2 words; PC advances by 2 on normal continuation.                                                                 |
| **Data address vs I/O address** | In `IN/OUT/SBI/CBI` the I/O address is `0x00–0x3F`. In the data space (`LD/ST/LDS/STS`, the bus) it is **+0x20**. TIFR0: I/O `0x15` → data `0x35`. |
| **Data space**                  | `0x00–0x1F` registers, `0x20–0xFF` I/O, `0x100–0x8FF` SRAM (ATmega328P).                                                                           |
| **Flash**                       | 32 KiB, but addressed by words; the HEX file stores bytes.                                                                                         |
| **SREG**                        | Bits 0..7: `C, Z, N, V, S, H, T, I`. `S = N xor V`.                                                                                                |
| **Sticky Z**                    | For `SBC`, `SBCI`, `CPC` the Z flag = (result == 0) **and** the old Z. `ADC` uses the plain rule.                                                  |
| **Interrupt vector**            | An address in words; on the 328P vectors take 2 words (`JMP`). `RESET` = 0, `INT0` = 2, … `TIMER0_OVF` = `0x20`.                                   |
| **Priority**                    | The lower the vector address, the higher the priority (`TIMER0_COMPA` `0x1C` beats `TIMER0_OVF` `0x20`).                                           |
| **Stack**                       | Grows down; `PUSH` writes at SP and decrements SP; a return address is 2 bytes (14-bit PC): low byte first, then high byte.                        |
| **RAZ/WI**                      | Read-As-Zero / Write-Ignored — the behavior of an unknown register.                                                                                |
| **TOV0 / OCF0x / TOIE0**        | Timer0 overflow / compare-match flag (in `TIFR0`) / enable bit in `TIMSK0`. An interrupt fires when `flag & mask` and `I=1`.                       |
| **PINx / DDRx / PORTx**         | Input / direction (1 = output) / output (for an output pin — the level, for an input pin — the pull-up).                                           |
| **`MCUContext`**                | A temporary set of pointers from which `ATmega328P::create()` assembles a ready `EmulatorFacade`.                                                  |
