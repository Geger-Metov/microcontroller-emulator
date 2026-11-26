# Project status

Updated: 2026-11-25.

## Done

- Build: CMake presets (`debug`, `release`, `release-python`), CI (debug/release matrix), pre-commit (clang-format, ruff).
- Core: the complete ATmega328P instruction set, a single cycle counter, reset, hardware-style stack,
  interrupts (queue, priorities, flag clearing, the "one more instruction" rule), SLEEP.
- Timer0: Normal/CTC/Fast PWM/Phase Correct, prescalers, TOV0/OCF0A/OCF0B, TIMSK0, three vectors.
- Unknown I/O registers (0x20–0xFF) are RAZ/WI, with no exception.
- GUI tests (`tests/python/`): pin logic, controller, Qt widgets (offscreen), integration with the real
  `emulator_backend`; run in CI (the `gui-tests` job).
- One `ruff.toml` in the root: the editor, pre-commit and CI sort imports the same way.
- C++ tests: ALU and MUL are verified by iterating over all operands against an oracle.
- The Python module `emulator_backend` builds and imports.
- HEX: record types 00–05, robust parsing (errors with a line number, no UB on broken files), EEPROM at
  `0x810000`, warnings about data outside flash/EEPROM.
- A disassembler in the core (`core/disasm`), `avr-objdump`-like output, consistent with the decoder over all
  65536 opcodes; in the GUI — a table that highlights the PC.
- Module API: `run()` without the GIL that returns the cycles executed, `stop()` from another thread, `snapshot()`,
  memory and flash blocks, a memory map, typed exceptions.

## Not implemented / limitations

| What                                                   | Note                                                                                              |
| ------------------------------------------------------ | ------------------------------------------------------------------------------------------------- |
| `SPM`                                                  | executes as a NOP (writing to flash is not modeled)                                               |
| `WDR`, `BREAK`                                         | NOP; the watchdog is not modeled                                                                  |
| Timer0: OC0A/OC0B pins, FOC0x, external clock input T0 | COM0xx bits are stored but pins do not change; CS=110/111 → the timer stands still                |
| UART, ADC, SPI, TWI, Timer1/2, EEPROM                  | registers read as 0, writes are ignored                                                           |
| GPIO input timing                                      | the PINx synchronizer delay (~1 cycle) is not modeled; no pin-change interrupts (INT0/INT1/PCINT) |
| Sleep modes                                            | any `SLEEP` = Idle (peripherals keep running)                                                     |
| Data access ≥ 0x900                                    | throws an exception (the GUI reports it)                                                          |

## Open issues

- **GPIO input**: a button can pull a pin low, but there are no pin-change or external interrupts yet, and
  the only external sources are "low", "high" and "floating" (no analog levels, no pin-to-pin wires).
- **Debugging**: no breakpoints; PC/memory change events are not exported to Python.
- **Memory**: unknown-register accesses are silently stored but not logged; the bus uses a `std::map`
  (a flat table would be faster); `FlashDevice::erase_page/write_page` are declared but not defined.
- **EEPROM** is not emulated; EEPROM data in a HEX file is dropped with a warning.

## GUI — known issues

- Run: every 16 ms the emulator executes as many cycles as real time elapsed × the MCU clock (16 MHz, `RunPacer`);
  a speed multiplier 1×/0.1×/0.01× is in the toolbar. It runs in the GUI thread: the core is faster than real
  time, and the frame cap (0.1 s) prevents freezes.
- Emulator errors are shown in a dialog and stop Run (`SimController` returns `False`, see `last_error`).
- A disassembler exists (right panel); there is no memory viewer or breakpoints; the Start/Pause/Stop buttons
  of the main window are placeholders.
- `pyproject.toml`: the script `gui = "gui:main"` points to a non-existent function (run with
  `uv run python -m gui.main`).

## Test firmware (`tests/data/hex/`)

- `firmware.hex` — blinks PB5 (≈628 thousand cycles per half period); does not read inputs.
- `firmware_test.hex` — a static set of levels on PB0–PB4 and PD2–PD7 (PD2 toggles in a loop). Does not read inputs.

## Next steps

1. GPIO input and the button + a test firmware that reads the button.
2. Memory/I/O viewer; breakpoints.
3. Peripherals for real sketches: USART, ADC, Timer1/2, INT0/1, PCINT, EEPROM.
4. Open-source preparation: contributing guide, README with examples and screenshots, `pyproject.toml`
   (metadata, launch script, `requires-python`), CI on Windows/macOS.
5. Finish the English translation (code comments, GUI strings, error messages and tests).
