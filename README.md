# ATmega328P microcontroller emulator

🇬🇧 English · 🇷🇺 [Русский](README.ru.md)

A C++ core (CPU, buses, Timer0, GPIO, Intel HEX loader) plus a Python/PySide6 GUI.
The core builds into the static library `mcu_core`, which is also used for the C++ tests and for the
Python module `emulator_backend` (pybind11) imported by the GUI.

## Requirements

| What                             | Version                       | Why                                    |
| -------------------------------- | ----------------------------- | -------------------------------------- |
| CMake                            | ≥ 3.21 (presets)              | build                                  |
| Compiler                         | GCC ≥ 11 / Clang ≥ 14 (C++20) | core; the `-g3` flags assume GCC/Clang |
| [uv](https://docs.astral.sh/uv/) | any recent version            | GUI environment (`gui/.venv`)          |
| Python                           | see `gui/.python-version`     | GUI and the `emulator_backend` module  |

## Quick start: build everything and run

Run the commands from the repository root.

```bash
# 1. GUI environment (PySide6 etc.) and pybind11 for building the module — once
cd gui
uv sync
cd ..

# 2. Build the C++ core and the Python module (optimized, -O2)
PY=$PWD/gui/.venv/bin/python
cmake --preset release-python \
      -Dpybind11_DIR="$($PY -m pybind11 --cmakedir)" -DPython_EXECUTABLE="$PY"
cmake --build --preset release-python

# 3. Check: the module imports and the tests pass
(cd gui && uv run python -c "import emulator_backend; print(emulator_backend.Emulator)")
ctest --test-dir build/release-python --output-on-failure

# 4. Run the GUI
cd gui && uv run python -m gui.main
```

The built `emulator_backend*.so` (`.pyd` on Windows) is placed into `gui/src` automatically;
there is nothing to copy. The file is not committed to git: after cloning you have to build it
(step 2). Always build the Python module optimized — with sanitizers in a Debug build the emulation
is several times slower.

## Your own `CMakeUserPresets.json` (so you don't type `-D...` every time)

Create `CMakeUserPresets.json` in the repository root. It contains absolute paths of your machine,
so it is listed in `.gitignore` and is **not committed** — every developer has their own.

1. Find two paths:

   ```bash
   gui/.venv/bin/python -m pybind11 --cmakedir   # → pybind11_DIR
   realpath gui/.venv/bin/python                  # → Python_EXECUTABLE (or just gui/.venv/bin/python)
   ```

2. Put them into the file (an example; substitute your own paths):

   ```json
   {
     "version": 3,
     "configurePresets": [
       {
         "name": "my-python",
         "inherits": "release-python",
         "cacheVariables": {
           "pybind11_DIR": "/abs/path/to/repo/gui/.venv/lib/python3.X/site-packages/pybind11/share/cmake/pybind11",
           "Python_EXECUTABLE": "/abs/path/to/repo/gui/.venv/bin/python"
         }
       }
     ]
   }
   ```

   The `my-python` preset inherits everything from `release-python` (optimization, module enabled).

3. Use it instead of the long command:

   ```bash
   cmake --preset my-python
   cmake --build --preset my-python
   ```

If the new preset is missing from `cmake --list-presets`, check that the file is in the repository
root (next to `CMakePresets.json`) and is valid JSON. When you recreate `gui/.venv` (for example
after changing the Python version), update this file too: the paths in it will change.

## C++ core tests

Python and pybind11 are not needed for the tests.

```bash
# Debug/tests: -g3 -O0 + ASan/UBSan + -Werror (as in CI)
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Optimized build (-O2) and the same tests
cmake --preset release && cmake --build --preset release && ctest --preset release
```

## CMake presets and options

| Preset           | What it does                                              |
| ---------------- | --------------------------------------------------------- |
| `debug`          | `-g3 -O0`, ASan+UBSan, warnings as errors, no Python      |
| `release`        | `-O2`, warnings as errors, no Python                      |
| `release-python` | `-O2` + the `emulator_backend` module (requires pybind11) |

Options: `MCU_BUILD_PYTHON` (the `emulator_backend` module, requires pybind11), `MCU_BUILD_TESTS`
(ON by default), `MCU_SANITIZE` (OFF), `MCU_WARNINGS_AS_ERRORS` (OFF).

## Editor (clangd / Zed)

All presets export `compile_commands.json`. The `.clangd` file in the root points clangd to
`build/release-python`: that build contains the core, the tests and `emu_bridge.cpp`.
Run step 2 of the quick start first, then restart the language server.

## Troubleshooting

- **`pybind11 not found`** during configuration — `-Dpybind11_DIR` was not passed (or is missing from
  `CMakeUserPresets.json`), or `uv sync` was not run.
- **`ModuleNotFoundError: emulator_backend`** — the module is not built (step 2) or was built for a
  different Python: the `.so` name contains the version (`cpython-315`), which must match
  `gui/.venv`. Check `-DPython_EXECUTABLE`.
- **The GUI is slow** — the module was built with the `debug` preset instead of `release-python`.

## Documentation

See [docs/](docs/README.md): architecture, recipes, status, decision log, glossary.

## License

[MIT](LICENSE)
