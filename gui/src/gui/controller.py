from collections.abc import Callable, Mapping, Sequence
from typing import Any, NamedTuple, Protocol

from gui.disasm_model import Listing, load_listing
from gui.run_pacing import DEFAULT_CLOCK_HZ

# Адреса регистров портов в пространстве данных ATmega328P (I/O-адрес + 0x20).
PINB, DDRB, PORTB = 0x23, 0x24, 0x25
PIND, DDRD, PORTD = 0x29, 0x2A, 0x2B


class EmulatorLike(Protocol):
    """Что контроллеру нужно от эмулятора (его выполняет `emulator_backend.Emulator`)."""

    def step(self) -> None: ...
    def run(self, cycles: int) -> object: ...  # настоящий возвращает такты
    def reset(self) -> None: ...
    def load_hex(self, path: str) -> object: ...  # настоящий — список предупреждений
    def get_pc(self) -> int: ...
    def get_sp(self) -> int: ...
    def get_sreg(self) -> int: ...
    def get_registers(self) -> list[int]: ...
    def get_cycle_count(self) -> int: ...
    def read_memory(self, addr: int) -> int: ...
    def read_memory_block(self, addr: int, length: int) -> bytes: ...
    def snapshot(self) -> Mapping[str, Any]: ...  # TypedDict подходит под Mapping
    def disassemble(
        self, word_addr: int, count: int
    ) -> Sequence[Mapping[str, Any]]: ...


class PortRegisters(NamedTuple):
    pin: int
    ddr: int
    port: int


class SimController:
    """Тонкая прослойка между GUI и эмулятором.

    Операции, которые могут бросить исключение в C++ (битый HEX, неизвестная
    инструкция, обращение за пределы памяти), не пробрасывают его в Qt-слоты:
    они возвращают False, запоминают текст в `last_error` и вызывают
    колбэк ошибки. Так GUI может остановить симуляцию и показать причину.
    """

    def __init__(self, emulator: EmulatorLike | None = None):
        if emulator is None:
            from emulator_backend import Emulator

            emulator = Emulator()
        self.emu = emulator
        self.clock_hz = DEFAULT_CLOCK_HZ  # частота МК для режима Run
        self.emu.reset()
        self.last_error: str | None = None
        self._gui_callback: Callable[[], None] | None = None
        self._error_callback: Callable[[str], None] | None = None

    def set_gui_callback(self, callback: Callable[[], None] | None) -> None:
        self._gui_callback = callback

    def set_error_callback(self, callback: Callable[[str], None] | None) -> None:
        self._error_callback = callback

    def _notify_gui(self) -> None:
        if self._gui_callback:
            self._gui_callback()

    def _guarded(self, action: str, fn: Callable[[], object]) -> bool:
        """Выполнить fn; при исключении запомнить ошибку и сообщить GUI."""
        try:
            fn()
        except Exception as exc:  # noqa: BLE001 — границу pybind11 пересекают разные типы
            self.last_error = f"{action}: {exc}"
            if self._error_callback:
                self._error_callback(self.last_error)
            return False
        self.last_error = None
        self._notify_gui()
        return True

    def load_hex(self, filepath: str) -> bool:
        """Загрузить прошивку из .hex файла и сбросить CPU."""

        def action() -> None:
            self.emu.load_hex(filepath)
            self.emu.reset()

        return self._guarded("Не удалось загрузить HEX", action)

    def step(self) -> bool:
        """Выполнить одну инструкцию."""
        return self._guarded("Ошибка выполнения", self.emu.step)

    def run_cycles(self, count: int) -> bool:
        """Выполнить указанное число тактов."""
        return self._guarded("Ошибка выполнения", lambda: self.emu.run(count))

    def reset(self) -> bool:
        return self._guarded("Ошибка сброса", self.emu.reset)

    def get_listing(self) -> Listing | None:
        """Дизассемблированная программа; при ошибке None (причина — в `last_error`).

        Только чтение, поэтому GUI-колбэк обновления не вызывается.
        """
        try:
            return load_listing(self.emu)
        except Exception as exc:  # noqa: BLE001 — см. _guarded
            self.last_error = f"Не удалось дизассемблировать: {exc}"
            if self._error_callback:
                self._error_callback(self.last_error)
            return None

    def get_state(self):
        """Вернуть словарь с состоянием CPU для отображения."""
        snap = self.emu.snapshot()  # одним вызовом, а не пятью
        return {
            "pc": snap["pc"],
            "sp": snap["sp"],
            "sreg": snap["sreg"],
            "regs": snap["registers"],
            "cycles": snap["cycles"],
        }

    def get_port_registers(self) -> dict[str, PortRegisters]:
        """PIN/DDR/PORT портов B и D."""
        # Один блок PINB..PORTD (9 байт; PINC..PORTC между ними — RAZ).
        block = self.emu.read_memory_block(PINB, PORTD - PINB + 1)

        def port(pin: int, ddr: int, port_reg: int) -> PortRegisters:
            return PortRegisters(
                block[pin - PINB], block[ddr - PINB], block[port_reg - PINB]
            )

        return {"B": port(PINB, DDRB, PORTB), "D": port(PIND, DDRD, PORTD)}

    def get_gpio_state(self):
        """Состояние пинов порта B (совместимость со старым форматом)."""
        regs = self.get_port_registers()["B"]
        return {
            i: {"ddr": (regs.ddr >> i) & 1, "port": (regs.port >> i) & 1}
            for i in range(8)
        }

    def step_and_get_state(self):
        self.step()
        return self.get_state()
