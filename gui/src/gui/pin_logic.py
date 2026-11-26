"""Pure pin logic: no Qt and no emulator, so it is tested with plain pytest."""

from collections.abc import Hashable, Iterable, Mapping
from typing import Literal

# An MCU pin: ("B", 5) is PB5, ("D", 2) is PD2.
PinKey = tuple[str, int]

PinLevelName = Literal["low", "high", "floating"]

PinKind = Literal["output_high", "output_low", "input_pullup", "input"]


def bit(value: int, n: int) -> int:
    return (value >> n) & 1


def pin_kind(ddr_bit: int, port_bit: int) -> PinKind:
    """Pin mode from the DDRx and PORTx bits."""
    if ddr_bit:
        return "output_high" if port_bit else "output_low"
    return "input_pullup" if port_bit else "input"


def output_levels(
    registers: Mapping[str, tuple[int, int]],
) -> dict[PinKey, int]:
    """Levels of all pins configured as outputs.

    ~registers: {"B": (ddr, port), "D": (ddr, port)}. Inputs are not in the result:
    ~their level is set by the external circuit, not by the firmware.
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
    """LED states: lit if connected to an output pin holding 1.

    ~links: (pin, LED) pairs, one per wire. An LED connected
    ~to an input pin is not in the result (we leave its state alone).
    ~If several wires lead to an LED, it is lit when any of them is lit.
    """
    states: dict[T, bool] = {}
    for pin, led in links:
        if pin in outputs:
            states[led] = states.get(led, False) or outputs[pin] == 1
    return states


def button_levels(
    links: Iterable[tuple[PinKey, bool]],
) -> dict[PinKey, PinLevelName]:
    """External levels that buttons apply to MCU pins.

    links: (pin, pressed) pairs, one per wire. A button connects the pin to ground, so
    a pressed button pulls the pin "low" and a released one leaves it "floating"
    (the internal pull-up, if enabled, then reads as 1). If several buttons are on
    one pin, it is low when any of them is pressed.
    """
    levels: dict[PinKey, PinLevelName] = {}
    for pin, pressed in links:
        if pressed:
            levels[pin] = "low"
        else:
            levels.setdefault(pin, "floating")
    return levels
