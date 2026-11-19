cmake_minimum_required(VERSION 3.18)
project(mcu_emulator_backend LANGUAGES CXX)

# Требуем C++17 (auto, make_unique, и т.д.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем pybind11 (должен быть установлен через pip)
find_package(pybind11 REQUIRED)

# Собираем Python-модуль из перечисленных исходников
pybind11_add_module(emulator_backend
    # Bindings
    bindings/emu_bridge.cpp

    # CPU
    cpu/AVR_CPU.cpp
    cpu/avr_instructions.cpp
    cpu/avre_plus_instruction_table.cpp

    # Decoder
    decoder/avr_decode_helpers.cpp

    # Facade
    facade/EmulatorFacade.cpp

    # Hex parser
    hex_parser/hex_parser.cpp

    # Loader
    loader/avr_loader.cpp

    # MCU specific
    mcu/avr/ATmega328p.cpp
    mcu/avr/AVR_GPIO.cpp

    # Peripherals
    mcu/peripherals/Timer0.cpp
)

# Добавляем пути ко всем заголовочным каталогам, чтобы #include "file.h" работали без префиксов
target_include_directories(emulator_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
    ${CMAKE_CURRENT_SOURCE_DIR}/bus
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/facade
    ${CMAKE_CURRENT_SOURCE_DIR}/hex_parser
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/loader
    ${CMAKE_CURRENT_SOURCE_DIR}/mcu
    ${CMAKE_CURRENT_SOURCE_DIR}/mcu/avr
    ${CMAKE_CURRENT_SOURCE_DIR}/mcu/peripherals
)

# При необходимости можно задать определения препроцессора
# target_compile_definitions(emulator_backend PRIVATE ...)

# Если используются какие-то нестандартные флаги, можно добавить
if(MSVC)
  target_compile_options(emulator_backend PRIVATE /W4)
else()
  target_compile_options(emulator_backend PRIVATE -Wall -Wextra)
endif()
