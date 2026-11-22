v 0.1

## Сборка и тесты C++ ядра

Нужны CMake ≥ 3.20 и компилятор с C++20 (GCC ≥ 11, Clang ≥ 14, MSVC 2019 16.11+). Python-модуль и зависимости (pybind11)
для тестов не требуются.

```bash
# Отладка/тесты: -g3 -O0 + ASan/UBSan + -Werror (как в CI)
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Оптимизированная сборка (-O2) и те же тесты
cmake --preset release && cmake --build --preset release && ctest --preset release
```

Пресеты лежат в `CMakePresets.json` (пресеты требуют CMake ≥ 3.21; флаги `-g3` рассчитаны на GCC/Clang).
Опции CMake: `MCU_BUILD_PYTHON` (модуль `emulator_backend`, по умолчанию ON,
нужен pybind11), `MCU_BUILD_TESTS` (ON), `MCU_SANITIZE` (OFF),
`MCU_WARNINGS_AS_ERRORS` (OFF).

Python-модуль для GUI собирайте только оптимизированно (в Debug с санитайзерами
эмуляция в разы медленнее). pybind11 — в том же venv, что и GUI:

```bash
uv add pybind11
cmake --preset release-python -Dpybind11_DIR=$(python -m pybind11 --cmakedir)
cmake --build --preset release-python
```

Затем положить собранный `emulator_backend*.so/.pyd` туда, где его найдёт GUI.
