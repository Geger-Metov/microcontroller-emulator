"""Интеграция с настоящим C++ ядром (нужен собранный emulator_backend)."""

import threading
import time

import pytest

pytest.importorskip("emulator_backend", reason="модуль emulator_backend не собран")

import emulator_backend as backend

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


# ---- API биндингов: типизированные ошибки, run/stop, снимок, блоки -----------


@pytest.fixture
def emu(hex_dir):
    e = backend.Emulator()
    e.load_hex(str(hex_dir / "firmware.hex"))
    e.reset()
    return e


def test_errors_are_typed(tmp_path):
    e = backend.Emulator()
    bad = tmp_path / "bad.hex"
    bad.write_text(":0400000011\n")  # заявлено 4 байта, строка короче
    with pytest.raises(backend.HexFormatError, match="line 1"):
        e.load_hex(str(bad))
    with pytest.raises(backend.EmulatorError):  # общий базовый класс
        e.load_hex(str(bad))

    with pytest.raises(backend.MemoryAccessError, match="0x0900"):
        e.read_memory(0x900)


def test_illegal_instruction_error_has_opcode_and_address(tmp_path):
    hex_file = tmp_path / "bad.hex"
    hex_file.write_text(":02000000D89591\n:00000001FF\n")
    e = backend.Emulator()
    e.load_hex(str(hex_file))
    e.reset()
    with pytest.raises(backend.IllegalInstructionError, match="0x95D8"):
        e.step()


def test_run_returns_executed_cycles(emu):
    done = emu.run(10_000)
    assert 10_000 <= done < 10_010
    assert emu.get_cycle_count() == done
    assert not emu.is_running()


def test_stop_interrupts_run_from_another_thread(emu):
    result: list[int] = []
    worker = threading.Thread(target=lambda: result.append(emu.run(10**12)))
    worker.start()
    deadline = time.monotonic() + 5
    while not emu.is_running() and time.monotonic() < deadline:
        time.sleep(0.001)
    assert emu.is_running()

    # Пока run() идёт, GIL свободен (этот поток работает), а остальной API занят.
    with pytest.raises(backend.EmulatorBusyError):
        emu.get_pc()
    with pytest.raises(backend.EmulatorBusyError):
        emu.run(1)

    emu.stop()
    worker.join(timeout=5)
    assert not worker.is_alive(), "stop() не прервал run()"
    assert 0 < result[0] < 10**12
    assert not emu.is_running()
    assert emu.get_cycle_count() == result[0]


def test_stop_when_idle_is_harmless(emu):
    emu.stop()
    assert emu.run(100) >= 100


def test_snapshot_matches_individual_getters(emu):
    emu.run(5000)
    s = emu.snapshot()
    assert s["pc"] == emu.get_pc()
    assert s["sp"] == emu.get_sp()
    assert s["sreg"] == emu.get_sreg()
    assert s["cycles"] == emu.get_cycle_count()
    assert s["registers"] == emu.get_registers()
    assert len(s["registers"]) == 32


def test_read_flash_and_memory_blocks(emu):
    # firmware.hex начинается с JMP: 0x940C -> байты 0C 94 (little-endian)
    assert emu.read_flash(0, 2) == bytes([0x0C, 0x94])
    word = emu.read_program_word(0)
    assert emu.read_flash(1, 1) == bytes([word >> 8])  # нечётный байтовый адрес

    emu.write_memory(0x100, 0xAB)
    emu.write_memory(0x101, 0xCD)
    assert emu.read_memory_block(0x100, 2) == b"\xab\xcd"
    with pytest.raises(backend.MemoryAccessError):
        emu.read_memory_block(0x8FF, 2)  # выходит за SRAM


def test_memory_map_lists_data_space(emu):
    regions = {r["name"]: r for r in emu.get_memory_map()}
    assert (regions["Registers"]["start"], regions["Registers"]["end"]) == (0, 0x1F)
    assert regions["SRAM"]["end"] == 0x8FF


def test_load_hex_returns_warnings(tmp_path):
    hex_file = tmp_path / "fuses.hex"
    hex_file.write_text(
        ":02000004008278\n"  # линейная база 0x0082 -> адреса с 0x820000
        ":01000000FF00\n"
        ":00000001FF\n"
    )
    e = backend.Emulator()
    warnings = e.load_hex(str(hex_file))
    assert len(warnings) == 1 and "0x820000" in warnings[0]
