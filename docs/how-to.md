# Рецепты

Все команды — из корня репозитория. Полная инструкция по сборке и запуску — в корневом `README.md`.

## Собрать и проверить

```bash
cmake --preset debug   && cmake --build --preset debug   && ctest --preset debug     # -g3 -O0, ASan+UBSan, -Werror (~20 с)
cmake --preset release && cmake --build --preset release && ctest --preset release   # -O2 (~1 с)
```

Запускать **оба**: некоторые ошибки видны только при оптимизации. Это же делает CI.

## Добавить инструкцию

Пример: новая инструкция `XYZ`.

1. **Семантика** — функция `exec_XYZ(AVRExecutionContext &ctx, const DecodedInstruction &instr)` в
   `core/cpu/avr_instructions.cpp` + объявление в `avr_instructions.h`. Флаги — по формулам из
   `docs/AVR-InstructionSet-Manual-DS40002198.pdf`. Для 8-битной арифметики используйте готовые
   `add8`/`sub8`/`write_logic`/`set_nzvs`. Дополнительные такты (переход, пропуск) — `ctx.cpu().add_cycles(n)`.
2. **Операнды** — если нужной формы разбора нет, добавьте `decode_*` в `core/decoder/avr_decode_helpers.*`
   (поля `DecodedInstruction`: `Rd, Rr, imm, rel, io, bit, sreg_bit, ptr, mode`).
3. **Таблица** — строка в `core/cpu/avre_plus_instruction_table.cpp`:
   `{маска, паттерн, длина_в_словах, такты, decode_*, exec_*}`. Записи не должны пересекаться
   (тест `decoder_table_entries_never_overlap` это проверит).
4. **Тесты**:
   - арифметика/флаги — по образцу `tests/cpp/test_isa_alu.cpp` (оракул на широкой арифметике, перебор операндов);
   - поведение/такты — по образцу `tests/cpp/test_isa.cpp` (`make_emu({{0, {...слова программы...}}})`,
     `steps(*emu, n)`, проверка `read_data`, `get_pc`, `get_cycle_count`).
5. Проверка «тест действительно ловит ошибку»: временно сломайте формулу и убедитесь, что тест падает.

## Добавить регистр или периферию

- **Простой регистр** (хранит значение): `MMIORegister` в `MCUContext`, создать в `build_context`,
  отобразить в `build_memory_map` (`data_bus->map(адрес, адрес, &reg)`).
- **Регистр с реакцией**: `reg.on_write(...)`, `reg.on_read(...)`.
- **Периферия со временем** (как Timer0): класс `: ITickable, IResettable`, регистры — публичные `MMIORegister`;
  в `build_memory_map` отобразить каждый регистр; в `connect_cpu` — `data_bus->add_tickable(dev)`;
  в `create()` — `register_reset_device(dev)`. Прерывание: `attach_interrupt_sink(cpu)` и вызовы
  `raise_interrupt/clear_interrupt(вектор)` (константы векторов — `ATmega328P_Constants.h`);
  сброс флага при входе в обработчик — `set_interrupt_ack_handler`.
- **Адреса**: в даташите I/O-адрес (например, TIFR0 = 0x15) → в пространстве данных **+0x20** (0x35).
- **Новый .cpp добавьте в `core/CMakeLists.txt`** (список файлов `mcu_core` ведётся вручную).
- **Тест** — по образцу `tests/cpp/test_timer0.cpp`; новый тестовый файл — в `tests/cpp/CMakeLists.txt`.

## Добавить тест

- Новый файл `tests/cpp/test_*.cpp` → строка в `tests/cpp/CMakeLists.txt`.
- Каркас свой (`test_framework.h`): `TEST(имя) { CHECK(...); CHECK_EQ(a, b); CHECK_THROWS(...); }`.
- Помощники в `test_util.h`: `make_emu`, кодировщики опкодов (`ldi`, `out`, `rjmp`, `rd_rr`, `rd_k`, …),
  адреса регистров.
- Прошивки для тестов — `tests/data/hex/` (путь в коде — макрос `MCU_HEX_DIR`).

## Тесты GUI

```bash
cd gui
uv add --dev pytest pytest-qt          # один раз
uv run pytest -v                       # тесты лежат в ../tests/python; offscreen выставляется в conftest.py
```

Три уровня (файлы в `tests/python/`):

- `test_pin_logic.py`, `test_controller.py` — чистая логика и контроллер на `FakeEmulator`; не требуют ни Qt, ни ядра.
- `test_qt_gui.py` — виджеты через `qtbot` без экрана (кнопки, остановка по ошибке, светодиод, подписи пинов).
  Пропускается, если нет PySide6/pytest-qt.
- `test_backend_integration.py` — настоящее ядро: загрузка `tests/data/hex/*.hex`, состояние портов, ошибки.
  Пропускается, если не собран `emulator_backend`.

Правило: логику (что гореть, какой цвет, какие адреса) держим в `pin_logic.py`/`controller.py` и тестируем без Qt;
в виджетах остаётся только отрисовка.

## Посмотреть, что делает прошивка

В GUI: правая панель → меню → «Дизассемблер». Листинг строится после загрузки HEX; строка, куда указывает PC,
подсвечена. Адреса — **байтовые** (как в `avr-objdump` и HEX), цели переходов тоже. Серым — опкоды, которых нет
в ATmega328P (`.word 0x…`). Из Python: `emu.disassemble(word_addr, count)`.
Для сверки с исходниками: `avr-objdump -d firmware.elf`. Для трассировки без GUI — маленькая C++-программа с
`ATmega328P::create()` + `load_hex` + `step_instruction()`.

## Типичные ловушки

- PC в эмуляторе — в **словах**, адреса в objdump — в **байтах** (×2).
- При изменении таблицы инструкций пересоберите всё: справочник декодера строится по таблице.
- Python-модуль собирайте только `release-python` — в Debug с санитайзерами он очень медленный.
