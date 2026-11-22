# Эмулятор микроконтроллера ATmega328P

C++-ядро (CPU, шина, Timer0, GPIO, загрузчик Intel HEX) + GUI на Python/PySide6.
Ядро собирается в статическую библиотеку `mcu_core`, из неё же строятся C++-тесты и
Python-модуль `emulator_backend` (pybind11), который импортирует GUI.

## Требования

| Что                              | Версия                        | Зачем                                     |
| -------------------------------- | ----------------------------- | ----------------------------------------- |
| CMake                            | ≥ 3.21 (пресеты)              | сборка                                    |
| Компилятор                       | GCC ≥ 11 / Clang ≥ 14 (C++20) | ядро; флаги `-g3` рассчитаны на GCC/Clang |
| [uv](https://docs.astral.sh/uv/) | любая свежая                  | окружение GUI (`gui/.venv`)               |
| Python                           | см. `gui/.python-version`     | GUI и модуль `emulator_backend`           |

## Быстрый старт: собрать всё и запустить

Команды выполняются из корня репозитория.

```bash
# 1. Окружение GUI (PySide6 и др.) и pybind11 для сборки модуля — один раз
cd gui
uv sync
cd ..

# 2. Сборка C++-ядра и Python-модуля (оптимизированная, -O2)
PY=$PWD/gui/.venv/bin/python
cmake --preset release-python \
      -Dpybind11_DIR="$($PY -m pybind11 --cmakedir)" -DPython_EXECUTABLE="$PY"
cmake --build --preset release-python

# 3. Проверка: модуль импортируется, тесты проходят
(cd gui && uv run python -c "import emulator_backend; print(emulator_backend.Emulator)")
ctest --test-dir build/release-python --output-on-failure

# 4. Запуск GUI
cd gui && uv run python -m gui.main
```

Собранный `emulator_backend*.so` (`.pyd` на Windows) автоматически попадает в `gui/src`,
дополнительно копировать ничего не нужно. Файл в git не коммитится: после клонирования
его нужно собрать (шаг 2). Python-модуль собирайте только оптимизированно — в Debug с
санитайзерами эмуляция в разы медленнее.

## Свой `CMakeUserPresets.json` (чтобы не вводить `-D...` каждый раз)

В корне репозитория создайте файл `CMakeUserPresets.json`. В нём лежат абсолютные пути
вашей машины, поэтому он в `.gitignore` и **не коммитится** — у каждого разработчика свой.

1. Узнайте два пути:

   ```bash
   gui/.venv/bin/python -m pybind11 --cmakedir   # → pybind11_DIR
   realpath gui/.venv/bin/python                  # → Python_EXECUTABLE (или просто gui/.venv/bin/python)
   ```

2. Вставьте их в файл (пример; пути подставьте свои):

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

   Пресет `my-python` наследует всё от `release-python` (оптимизация, модуль включён).

3. Используйте его вместо длинной команды:

   ```bash
   cmake --preset my-python
   cmake --build --preset my-python
   ```

Если в списке `cmake --list-presets` нового пресета нет — проверьте, что файл лежит в
корне (рядом с `CMakePresets.json`) и это корректный JSON. Пересоздавать `gui/.venv`
(например, при смене версии Python) нужно вместе с этим файлом: пути в нём изменятся.

## Тесты C++ ядра

Python и pybind11 для тестов не нужны.

```bash
# Отладка/тесты: -g3 -O0 + ASan/UBSan + -Werror (как в CI)
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Оптимизированная сборка (-O2) и те же тесты
cmake --preset release && cmake --build --preset release && ctest --preset release
```

## Пресеты и опции CMake

| Пресет           | Что делает                                                   |
| ---------------- | ------------------------------------------------------------ |
| `debug`          | `-g3 -O0`, ASan+UBSan, предупреждения как ошибки, без Python |
| `release`        | `-O2`, предупреждения как ошибки, без Python                 |
| `release-python` | `-O2` + модуль `emulator_backend` (нужен pybind11)           |

Опции: `MCU_BUILD_PYTHON` (модуль `emulator_backend`, нужен pybind11), `MCU_BUILD_TESTS`
(по умолчанию ON), `MCU_SANITIZE` (OFF), `MCU_WARNINGS_AS_ERRORS` (OFF).

## Редактор (clangd / Zed)

Все пресеты экспортируют `compile_commands.json`. Файл `.clangd` в корне указывает clangd
на `build/release-python`: эта сборка содержит и ядро, и тесты, и `emu_bridge.cpp`.
Сначала выполните шаг 2 из быстрого старта, затем перезапустите языковой сервер.

## Типичные проблемы

- **`pybind11 не найден`** при конфигурации — не передан `-Dpybind11_DIR` (или он не
  попал в `CMakeUserPresets.json`), либо не выполнен `uv add --dev pybind11`.
- **`ModuleNotFoundError: emulator_backend`** — модуль не собран (шаг 2) или собран под
  другой Python: в имени `.so` зашита версия (`cpython-315`), она должна совпадать с
  `gui/.venv`. Проверьте `-DPython_EXECUTABLE`.
- **GUI тормозит** — модуль собран пресетом `debug`, а не `release-python`.
