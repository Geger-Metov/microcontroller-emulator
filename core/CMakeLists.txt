cmake_minimum_required(VERSION 3.20)
project(mcu_emulator_backend LANGUAGES CXX)

# C++20: the code already uses designated initializers (the instruction table).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MCU_BUILD_PYTHON "Build the emulator_backend Python module (needs pybind11)" ON)
option(MCU_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(MCU_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Compiler warnings for the target (also used by the tests).
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

# Sanitizers: PUBLIC, so the tests also get the link flags.
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

# The emulator core is a static library with no Python dependency.
# It is used by both the Python module and the C++ tests.
add_library(mcu_core STATIC
    # CPU
    cpu/AVR_CPU.cpp
    cpu/avr_instructions.cpp
    cpu/avre_plus_instruction_table.cpp

    # Decoder
    decoder/avr_decode_helpers.cpp

    # Disassembler
    disasm/avr_disasm.cpp

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

# Headers are included from the root of core/: #include "cpu/AVR_CPU.h", "common/errors.h".
# PUBLIC — so the C++ tests and the Python module see the same paths.
target_include_directories(mcu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
mcu_apply_warnings(mcu_core)
mcu_apply_sanitizers(mcu_core)

# The Python module. pybind11 is installed into the GUI venv (uv sync), and its path
# is passed like this: -Dpybind11_DIR="$(python -m pybind11 --cmakedir)"
# and -DPython_EXECUTABLE=<python from the venv>. The built emulator_backend*.so goes
# straight into gui/src, where `from emulator_backend import Emulator` finds it.
if(MCU_BUILD_PYTHON)
  set(PYBIND11_FINDPYTHON ON) # modern FindPython (the old modules were removed in CMake 4)
  find_package(pybind11 CONFIG QUIET)
  if(NOT pybind11_FOUND)
    message(FATAL_ERROR
      "pybind11 not found. Specify it: "
      "-Dpybind11_DIR=\"$(python -m pybind11 --cmakedir)\" "
      "and -DPython_EXECUTABLE=<python from the venv>. "
      "Or build without the module: -DMCU_BUILD_PYTHON=OFF")
  endif()
  pybind11_add_module(emulator_backend bindings/emu_bridge.cpp)
  target_link_libraries(emulator_backend PRIVATE mcu_core)
  set_target_properties(emulator_backend PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/gui/src")
  mcu_apply_warnings(emulator_backend)
endif()
