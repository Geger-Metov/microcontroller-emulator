"""Чистая логика пинов: без Qt и без эмулятора, поэтому тестируется обычным pytest."""

from collections.abc import Hashable, Iterable, Mapping
from typing import Literal

# Пин микроконтроллера: ("B", 5) — PB5, ("D", 2) — PD2.
PinKey = tuple[str, int]

PinKind = Literal["output_high", "output_low", "input_pullup", "input"]


def bit(value: int, n: int) -> int:
    return (value >> n) & 1


def pin_kind(ddr_bit: int, port_bit: int) -> PinKind:
    """Режим пина по битам DDRx и PORTx."""
    if ddr_bit:
        return "output_high" if port_bit else "output_low"
    return "input_pullup" if port_bit else "input"


def output_levels(
    registers: Mapping[str, tuple[int, int]],
) -> dict[PinKey, int]:
    """Уровни всех пинов, настроенных как выходы.

    registers: {"B": (ddr, port), "D": (ddr, port)}. Входы в результат не попадают:
    их уровень определяется внешней схемой, а не прошивкой.
    """
    levels: dict[PinKey, int] = {}
    for port_name, (ddr, port) in registers.items():
        for i in range(8):
            if bit(ddr, i):
                levels[(port_name, i)] = bit(port, i)
    return levels


def led_states[T: Hashable](
    links: Iterable[tuple[PinKey, T]],
    outputs: Mapping[PinKey, int],
) -> dict[T, bool]:
    """Состояние светодиодов: горит, если подключён к выходному пину с единицей.

    links: пары (пин, светодиод), по одной на каждый провод. Светодиод, подключённый
    к входному пину, в результат не попадает (его состояние не меняем).
    Если к светодиоду ведёт несколько проводов, он горит, когда горит любой из них.
    """
    states: dict[T, bool] = {}
    for pin, led in links:
        if pin in outputs:
            states[led] = states.get(led, False) or outputs[pin] == 1
    return states
