cmake_minimum_required(VERSION 3.20)
project(mcu_emulator_backend LANGUAGES CXX)

# C++20: в коде уже используются designated initializers (таблица инструкций).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MCU_BUILD_PYTHON "Build the emulator_backend Python module (needs pybind11)" ON)
option(MCU_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(MCU_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Предупреждения компилятора для цели (используется и тестами).
function(mcu_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(MCU_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    if(MCU_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# Санитайзеры: PUBLIC, чтобы флаги линковки получили и тесты.
function(mcu_apply_sanitizers target)
  if(MCU_SANITIZE)
    if(MSVC)
      message(FATAL_ERROR "MCU_SANITIZE is not supported with MSVC")
    endif()
    target_compile_options(${target} PUBLIC
      -fsanitize=address,undefined -fno-sanitize-recover=undefined
      -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  endif()
endfunction()

# Ядро эмулятора — статическая библиотека без привязки к Python.
# Её используют и Python-модуль, и C++-тесты.
add_library(mcu_core STATIC
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
set_target_properties(mcu_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Добавляем пути ко всем заголовочным каталогам, чтобы #include "file.h" работали без префиксов
target_include_directories(mcu_core PUBLIC
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
mcu_apply_warnings(mcu_core)
mcu_apply_sanitizers(mcu_core)

# Python-модуль (pybind11 должен быть установлен, например через pip).
if(MCU_BUILD_PYTHON)
  find_package(pybind11 REQUIRED)
  pybind11_add_module(emulator_backend bindings/emu_bridge.cpp)
  target_link_libraries(emulator_backend PRIVATE mcu_core)
  mcu_apply_warnings(emulator_backend)
endif()
