# Recipes

All commands are run from the repository root. The full build and run instructions are in the root `README.md`.

## Build and check

```bash
cmake --preset debug   && cmake --build --preset debug   && ctest --preset debug     # -g3 -O0, ASan+UBSan, -Werror (~20 s)
cmake --preset release && cmake --build --preset release && ctest --preset release   # -O2 (~1 s)
```

Run **both**: some bugs show up only with optimization. CI does the same.

## Add an instruction

Example: a new instruction `XYZ`.

1. **Semantics** — a function `exec_XYZ(AVRExecutionContext &ctx, const DecodedInstruction &instr)` in
   `core/cpu/avr_instructions.cpp` plus a declaration in `avr_instructions.h`. Flags follow the formulas in the
   AVR Instruction Set Manual (see [references.md](references.md)). For 8-bit arithmetic use the existing
   `add8`/`sub8`/`write_logic`/`set_nzvs`. Extra cycles (branch, skip): `ctx.cpu().add_cycles(n)`.
2. **Operands** — if the needed operand format has no decoder, add a `decode_*` in
   `core/decoder/avr_decode_helpers.*` (`DecodedInstruction` fields: `Rd, Rr, imm, rel, io, bit, sreg_bit, ptr, mode`).
3. **Table** — a row in `core/cpu/avre_plus_instruction_table.cpp`:
   `{mask, pattern, length_in_words, cycles, decode_*, exec_*}`. Entries must not overlap
   (the test `decoder_table_entries_never_overlap` checks this).
4. **Disassembler** — add the mnemonic to `core/disasm/avr_disasm.cpp`; the test
   `disasm_agrees_with_decoder_on_all_65536_opcodes` fails until the decoder and disassembler agree.
5. **Tests**:
   - arithmetic/flags — follow `tests/cpp/test_isa_alu.cpp` (an oracle on wide arithmetic, all operands);
   - behavior/cycles — follow `tests/cpp/test_isa.cpp` (`make_emu({{0, {...program words...}}})`,
     `steps(*emu, n)`, checking `read_data`, `get_pc`, `get_cycle_count`).
6. Check that the test really catches a bug: temporarily break the formula and make sure the test fails.

## Add a register or a peripheral

- **A plain register** (stores a value): an `MMIORegister` in `MCUContext`, created in `build_context`,
  mapped in `build_memory_map` (`data_bus->map(address, address, &reg)`).
- **A register with side effects**: `reg.on_write(...)`, `reg.on_read(...)`.
- **A peripheral with time** (like Timer0): a class `: ITickable, IResettable` with public `MMIORegister`s;
  map every register in `build_memory_map`; call `data_bus->add_tickable(dev)` in `connect_cpu`;
  call `register_reset_device(dev)` in `create()`. Interrupts: `attach_interrupt_sink(cpu)` and calls to
  `raise_interrupt/clear_interrupt(vector)` (vector constants are in `ATmega328P_Constants.h`);
  clearing the flag on interrupt entry — `set_interrupt_ack_handler`.
- **Addresses**: an I/O address from the datasheet (for example TIFR0 = 0x15) is **+0x20** in the data space (0x35).
- **Add the new .cpp to `core/CMakeLists.txt`** (the `mcu_core` file list is maintained by hand).
- **Test** — follow `tests/cpp/test_timer0.cpp`; add the new test file to `tests/cpp/CMakeLists.txt`.

## Add a test

- A new file `tests/cpp/test_*.cpp` → a line in `tests/cpp/CMakeLists.txt`.
- The framework is our own (`test_framework.h`): `TEST(name) { CHECK(...); CHECK_EQ(a, b); CHECK_THROWS(...); }`.
- Helpers in `test_util.h`: `make_emu`, opcode encoders (`ldi`, `out`, `rjmp`, `rd_rr`, `rd_k`, …),
  register addresses.
- Firmware for tests lives in `tests/data/hex/` (in code the path is the macro `MCU_HEX_DIR`).

## GUI tests

```bash
cd gui
uv sync                                # pytest and pytest-qt are in the dev dependencies
uv run pytest -v                       # the tests are in ../tests/python; offscreen is set in conftest.py
```

Three levels (files in `tests/python/`):

- `test_pin_logic.py`, `test_controller.py`, `test_disasm_model.py`, `test_run_pacing.py` — pure logic and the
  controller on a `FakeEmulator`; need neither Qt nor the core.
- `test_qt_gui.py` — widgets through `qtbot` without a display (buttons, stop on error, LED, pin labels,
  disassembly view). Skipped if PySide6/pytest-qt is missing.
- `test_backend_integration.py` — the real core: loading `tests/data/hex/*.hex`, port state, errors, `run`/`stop`.
  Skipped if `emulator_backend` is not built.

Rule: keep logic (what is lit, which color, which addresses) in plain modules such as `pin_logic.py`/`controller.py`
and test it without Qt; widgets only draw.

## See what a firmware does

In the GUI: right panel → menu → "Disassembly". The listing is built after loading a HEX file; the row the PC
points to is highlighted. Addresses are **byte** addresses (as in `avr-objdump` and HEX), jump targets too.
Gray rows are opcodes that do not exist on the ATmega328P (`.word 0x…`). From Python:
`emu.disassemble(word_addr, count)`.
To compare with the sources: `avr-objdump -d firmware.elf`. For tracing without the GUI — a small C++ program with
`ATmega328P::create()` + `load_hex` + `step_instruction()`.

## Common pitfalls

- The PC in the emulator is in **words**; addresses in objdump are in **bytes** (×2).
- After changing the instruction table, rebuild everything: the decoder lookup is built from the table.
- Build the Python module only with `release-python` — in Debug with sanitizers it is very slow.
