# Contributing

Thanks for your interest! This is an ATmega328P emulator: a C++20 core and a Python/PySide6 GUI.
Bug reports, firmware that behaves differently from real hardware, tests and fixes are all welcome.

## Getting set up

Follow the quick start in [README.md](README.md) (CMake presets + `uv`). Useful extras:

```bash
cd gui && uv sync                 # GUI environment, including dev tools
pre-commit install                # ruff, mypy and clang-format on every commit
```

## Running the checks

Run these before opening a pull request (CI runs the same):

```bash
# C++ core, with ASan/UBSan and -Werror, then the optimized build
cmake --preset debug   && cmake --build --preset debug   && ctest --preset debug
cmake --preset release && cmake --build --preset release && ctest --preset release

# Python: lint, format, types, tests (the tests that need the module require step 2 of the quick start)
cd gui
uv run ruff check . ../tests/python
uv run ruff format --check . ../tests/python
uv run mypy src
uv run pytest
```

## What a good change looks like

- **Emulation correctness first.** A change to instruction behavior, timing or peripherals comes with
  a test, ideally one that cites the datasheet or the instruction set manual (see
  [docs/references.md](docs/references.md); the PDFs are not in the repository).
- **Small and focused.** One topic per pull request; unrelated refactoring goes in a separate one.
- **Tests live next to their layer:** C++ in `tests/cpp` (the small framework in `test_framework.h`),
  Python in `tests/python`. GUI logic that does not need Qt goes into plain modules
  (`pin_logic.py`, `run_pacing.py`, `disasm_model.py`) so it can be tested without a display.
- **Style.** C++ is formatted by clang-format (`.clang-format`), Python by ruff (`ruff.toml`).
  Code, comments, commit messages and user-visible strings are in English.
- **Document decisions.** Non-obvious choices go to [docs/decisions.md](docs/decisions.md); update
  [docs/status.md](docs/status.md) when you add or finish a feature.

## Reporting bugs

Use the issue templates. For emulation bugs the most useful report contains the firmware (`.hex`, or
its source), what the real chip does (datasheet reference or a measurement), and what the emulator
does instead.

## License

By contributing you agree that your contribution is licensed under the project's [MIT License](LICENSE).
