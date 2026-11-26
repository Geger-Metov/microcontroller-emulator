# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Full ATmega328P instruction set, cycle-accurate timing, interrupt controller with priorities.
- Timer0 in all modes (Normal, CTC, PWM variants), compare/overflow interrupts.
- Strict Intel HEX parser and loader (`HexFormatError` with the line number), EEPROM/flash regions.
- Disassembler in the core, exposed to Python and shown in the GUI with the current PC highlighted.
- Python bindings: GIL-free `run()`, `stop()` from another thread, state snapshot, typed exceptions.
- GUI: Run at the MCU clock frequency with a speed selector, error reporting, pin labels.
- C++ test suite (ASan/UBSan, `-Werror`), Python tests (pytest, pytest-qt), CI on GitHub Actions.
- English documentation and sources; the Russian README is kept as `README.ru.md`.

- GPIO input: `PINx` reads pin levels (outputs, pull-ups, external drive), writing 1 to `PINx` toggles `PORTx`;
  `Emulator.set_pin_level()` and a GUI button that pulls a pin to ground; a button-to-LED example firmware.

### Changed
- Unknown I/O registers read as 0 and ignore writes instead of throwing.
- Includes are relative to the `core/` root.

### Fixed
- Relative branch offsets were decoded as unsigned, so every backward branch went forward.
- Timer0 never ticked; reset did not reset peripherals; cycle accounting drifted by ~25 %.
- Wrong `TIMER0_OVF` vector, stack byte order of the return address, flags of `ADC/ADIW/SBIW`.
