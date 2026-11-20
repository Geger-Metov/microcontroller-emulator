cmake_minimum_required(VERSION 3.18)
project(mc_emu LANGUAGES CXX)

option(MCU_BUILD_TESTS "Build C++ tests" ON)

add_subdirectory(core)

if(MCU_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/cpp)
endif()
