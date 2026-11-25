"""Общие фикстуры GUI-тестов.

Qt-тесты запускаются без экрана: QT_QPA_PLATFORM=offscreen должен быть выставлен
до импорта PySide6.
"""

import os
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

from gui.controller import SimController

REPO_ROOT = Path(__file__).resolve().parents[2]
HEX_DIR = REPO_ROOT / "tests" / "data" / "hex"


class FakeEmulator:
    """Подставной эмулятор: ничего не считает, только хранит состояние и считает вызовы."""

    def __init__(self) -> None:
        self.memory: dict[int, int] = {}
        self.pc = 0
        self.cycles = 0
        self.steps = 0
        self.resets = 0
        self.loaded: list[str] = []
        self.fail_step: Exception | None = None
        self.fail_run: Exception | None = None
        # Что вернёт disassemble(): строки в формате модуля
        self.program: list[dict] = []
        self.runs: list[int] = []  # размеры пачек, переданных в run()
        self.fail_load: Exception | None = None

    def step(self) -> None:
        if self.fail_step:
            raise self.fail_step
        self.steps += 1
        self.pc += 1
        self.cycles += 1

    def run(self, cycles: int) -> int:
        if self.fail_run:
            raise self.fail_run
        self.runs.append(cycles)
        self.cycles += cycles
        return cycles

    def reset(self) -> None:
        self.resets += 1
        self.pc = 0

    def load_hex(self, path: str) -> None:
        if self.fail_load:
            raise self.fail_load
        self.loaded.append(path)

    def get_pc(self) -> int:
        return self.pc

    def get_sp(self) -> int:
        return 0x8FF

    def get_sreg(self) -> int:
        return 0

    def get_registers(self) -> list[int]:
        return [0] * 32

    def get_cycle_count(self) -> int:
        return self.cycles

    def read_memory(self, addr: int) -> int:
        return self.memory.get(addr, 0)

    def read_memory_block(self, addr: int, length: int) -> bytes:
        return bytes(self.memory.get(addr + i, 0) for i in range(length))

    def disassemble(self, word_addr: int, count: int) -> list[dict]:
        rows = self.program[word_addr:]
        return [dict(r) for r in rows[:count]]

    def snapshot(self) -> dict:
        return {
            "pc": self.pc,
            "sp": 0x8FF,
            "sreg": 0,
            "cycles": self.cycles,
            "registers": [0] * 32,
        }


@pytest.fixture
def fake_emu() -> FakeEmulator:
    return FakeEmulator()


@pytest.fixture
def controller(fake_emu: FakeEmulator) -> SimController:
    return SimController(emulator=fake_emu)


@pytest.fixture
def hex_dir() -> Path:
    return HEX_DIR
