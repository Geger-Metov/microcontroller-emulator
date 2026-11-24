"""Интеграция с настоящим C++ ядром (нужен собранный emulator_backend)."""

import pytest

pytest.importorskip("emulator_backend", reason="модуль emulator_backend не собран")

from gui.controller import DDRB, DDRD, PORTB, PORTD, SimController


@pytest.fixture
def real() -> SimController:
    return SimController()


def test_blink_firmware_toggles_pb5(real, hex_dir):
    assert real.load_hex(str(hex_dir / "firmware.hex")), real.last_error

    levels = set()
    for _ in range(300):  # до ~3 млн тактов; полупериод ≈ 628 тыс.
        assert real.run_cycles(10_000), real.last_error
        regs = real.get_port_registers()["B"]
        assert regs.ddr & 0x20, "PB5 должен быть выходом"
        levels.add((regs.port >> 5) & 1)
        if levels == {0, 1}:
            break
    assert levels == {0, 1}, "PB5 не переключился"


def test_pin_test_firmware_sets_static_pattern(real, hex_dir):
    assert real.load_hex(str(hex_dir / "firmware_test.hex")), real.last_error
    assert real.run_cycles(1000), real.last_error

    b = real.get_port_registers()["B"]
    d = real.get_port_registers()["D"]
    assert (b.ddr, b.port) == (0x1F, 0x0F)
    assert d.ddr == 0xFC
    assert d.port & 0xB8 == 0xB8  # PD3,4,5,7 = 1; PD2 переключается в цикле


def test_missing_file_is_reported(real, tmp_path):
    errors = []
    real.set_error_callback(errors.append)
    assert real.load_hex(str(tmp_path / "nope.hex")) is False
    assert errors and "HEX" in errors[0]


def test_unknown_opcode_is_reported_and_stops(real, tmp_path):
    # Программа из одного слова 0x95D8 (ELPM — на ATmega328P нет)
    hex_file = tmp_path / "bad.hex"
    hex_file.write_text(":02000000D89591\n:00000001FF\n")
    assert real.load_hex(str(hex_file)), real.last_error
    assert real.step() is False
    assert real.last_error and "Illegal" in real.last_error


def test_register_constants_are_real_ports(real, hex_dir):
    # DDRx/PORTx по этим адресам должны отражать то, что делает прошивка
    real.load_hex(str(hex_dir / "firmware_test.hex"))
    real.run_cycles(1000)
    assert real.emu.read_memory(DDRB) == 0x1F
    assert real.emu.read_memory(PORTB) == 0x0F
    assert real.emu.read_memory(DDRD) == 0xFC
    assert real.emu.read_memory(PORTD) & 0xB8 == 0xB8
