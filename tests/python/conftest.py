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
        self.fail_load: Exception | None = None

    def step(self) -> None:
        if self.fail_step:
            raise self.fail_step
        self.steps += 1
        self.pc += 1
        self.cycles += 1

    def run(self, cycles: int) -> None:
        self.cycles += cycles

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


@pytest.fixture
def fake_emu() -> FakeEmulator:
    return FakeEmulator()


@pytest.fixture
def controller(fake_emu: FakeEmulator) -> SimController:
    return SimController(emulator=fake_emu)


@pytest.fixture
def hex_dir() -> Path:
    return HEX_DIR
