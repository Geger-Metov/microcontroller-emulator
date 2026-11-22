add_executable(mcu_tests
    test_main.cpp
    test_decoder.cpp
    test_firmware.cpp
    test_hex_parser.cpp
    test_isa.cpp
    test_isa_alu.cpp
    test_timer0.cpp
    test_timing.cpp
)
target_link_libraries(mcu_tests PRIVATE mcu_core)
target_compile_definitions(mcu_tests PRIVATE
    MCU_HEX_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/hex")
mcu_apply_warnings(mcu_tests)

add_test(NAME mcu_tests COMMAND mcu_tests)
