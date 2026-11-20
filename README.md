v 0.1

## Сборка и тесты C++ ядра

Нужны CMake ≥ 3.18 и компилятор с C++17. Python-модуль и зависимости (pybind11)
для тестов не требуются.

```bash
# Тесты с санитайзерами (так же, как в CI)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
      -DMCU_BUILD_PYTHON=OFF -DMCU_SANITIZE=ON -DMCU_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Опции CMake: `MCU_BUILD_PYTHON` (модуль `emulator_backend`, по умолчанию ON,
нужен pybind11), `MCU_BUILD_TESTS` (ON), `MCU_SANITIZE` (OFF),
`MCU_WARNINGS_AS_ERRORS` (OFF).

Python-модуль для GUI: `cmake -S . -B build -DMCU_BUILD_PYTHON=ON`, затем
положить собранный `emulator_backend*.so/.pyd` туда, где его найдёт GUI.
