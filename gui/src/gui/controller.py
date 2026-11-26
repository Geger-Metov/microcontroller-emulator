from collections.abc import Callable, Mapping, Sequence
from typing import Any, NamedTuple, Protocol

from gui.disasm_model import Listing, load_listing
from gui.run_pacing import DEFAULT_CLOCK_HZ

# Port register addresses in the ATmega328P data space (I/O address + 0x20).
PINB, DDRB, PORTB = 0x23, 0x24, 0x25
PIND, DDRD, PORTD = 0x29, 0x2A, 0x2B


class EmulatorLike(Protocol):
    """What the controller needs from an emulator (implemented by `emulator_backend.Emulator`)."""

    def step(self) -> None: ...
    def run(self, cycles: int) -> object: ...  # the real one returns the cycles
    def reset(self) -> None: ...
    def load_hex(
        self, path: str
    ) -> object: ...  # the real one returns a list of warnings
    def get_pc(self) -> int: ...
    def get_sp(self) -> int: ...
    def get_sreg(self) -> int: ...
    def get_registers(self) -> list[int]: ...
    def get_cycle_count(self) -> int: ...
    def read_memory(self, addr: int) -> int: ...
    def set_pin_level(self, port: str, bit: int, level: str) -> None: ...
    def read_memory_block(self, addr: int, length: int) -> bytes: ...
    def snapshot(self) -> Mapping[str, Any]: ...  # a TypedDict fits a Mapping
    def disassemble(
        self, word_addr: int, count: int
    ) -> Sequence[Mapping[str, Any]]: ...


class PortRegisters(NamedTuple):
    pin: int
    ddr: int
    port: int


class SimController:
    """A thin layer between the GUI and the emulator.

    ~Operations that may throw an exception in C++ (a broken HEX, an unknown
    ~instruction, an access outside memory) do not propagate it into Qt slots:
    ~they return False, store the text in `last_error` and call the
    ~error callback. This lets the GUI stop the simulation and show the reason.
    """

    def __init__(self, emulator: EmulatorLike | None = None):
        if emulator is None:
            from emulator_backend import Emulator

            emulator = Emulator()
        self.emu = emulator
        self.clock_hz = DEFAULT_CLOCK_HZ  # the MCU clock frequency for Run mode
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
        """Run fn; on an exception, remember the error and notify the GUI."""
        try:
            fn()
        except Exception as exc:  # noqa: BLE001 — different types cross the pybind11 boundary
            self.last_error = f"{action}: {exc}"
            if self._error_callback:
                self._error_callback(self.last_error)
            return False
        self.last_error = None
        self._notify_gui()
        return True

    def load_hex(self, filepath: str) -> bool:
        """Load firmware from a .hex file and reset the CPU."""

        def action() -> None:
            self.emu.load_hex(filepath)
            self.emu.reset()

        return self._guarded("Failed to load HEX", action)

    def step(self) -> bool:
        """Execute one instruction."""
        return self._guarded("Execution error", self.emu.step)

    def run_cycles(self, count: int) -> bool:
        """Execute the given number of cycles."""
        return self._guarded("Execution error", lambda: self.emu.run(count))

    def reset(self) -> bool:
        return self._guarded("Reset error", self.emu.reset)

    def get_listing(self) -> Listing | None:
        """The disassembled program; None on error (the reason is in `last_error`).

        ~Read-only, so the GUI update callback is not called.
        """
        try:
            return load_listing(self.emu)
        except Exception as exc:  # noqa: BLE001 — see _guarded
            self.last_error = f"Failed to disassemble: {exc}"
            if self._error_callback:
                self._error_callback(self.last_error)
            return None

    def get_state(self):
        """Return a dict with the CPU state for display."""
        snap = self.emu.snapshot()  # in one call, not five
        return {
            "pc": snap["pc"],
            "sp": snap["sp"],
            "sreg": snap["sreg"],
            "regs": snap["registers"],
            "cycles": snap["cycles"],
        }

    def set_pin_level(self, port: str, bit: int, level: str) -> bool:
        """Drive an MCU pin from outside ("low" / "high" / "floating"). False on error."""
        return self._guarded(
            "Failed to set the pin level",
            lambda: self.emu.set_pin_level(port, bit, level),
        )

    def get_port_registers(self) -> dict[str, PortRegisters]:
        """PIN/DDR/PORT of ports B and D."""
        # One PINB..PORTD block (9 bytes; PINC..PORTC in between read as zero).
        block = self.emu.read_memory_block(PINB, PORTD - PINB + 1)

        def port(pin: int, ddr: int, port_reg: int) -> PortRegisters:
            return PortRegisters(
                block[pin - PINB], block[ddr - PINB], block[port_reg - PINB]
            )

        return {"B": port(PINB, DDRB, PORTB), "D": port(PIND, DDRD, PORTD)}

    def get_gpio_state(self):
        """Port B pin state (compatibility with the old format)."""
        regs = self.get_port_registers()["B"]
        return {
            i: {"ddr": (regs.ddr >> i) & 1, "port": (regs.port >> i) & 1}
            for i in range(8)
        }

    def step_and_get_state(self):
        self.step()
        return self.get_state()
