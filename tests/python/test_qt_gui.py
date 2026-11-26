"""Widget tests (pytest-qt, no display: QT_QPA_PLATFORM=offscreen).

The emulator is a fake (FakeEmulator): we test the GUI behavior itself.
"""

import pytest

pytest.importorskip("PySide6", reason="PySide6 is not installed")
pytest.importorskip("pytestqt", reason="pytest-qt is not installed")

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor

from gui.controller import DDRB, DDRD, PORTB, PORTD
from gui.sim_view import SimulatorView
from gui.wire_item import WireItem

RED = QColor(Qt.GlobalColor.red)
GRAY = QColor(Qt.GlobalColor.gray)


@pytest.fixture
def view(qtbot, controller, monkeypatch):
    v = SimulatorView(controller)
    qtbot.addWidget(v)
    # A modal dialog would hang the test forever — we record messages instead of it.
    v.shown_errors = []
    monkeypatch.setattr(v, "show_error", v.shown_errors.append)
    return v


def connect_pin_to_led(view, port, pin):
    """A chip + an LED + a wire from a pin to the LED; returns the LED."""
    view.show_chip()
    led = view.chip_scene.add_led()
    connectors = (
        view.chip_scene.chip_item.pin_connectors_B
        if port == "B"
        else view.chip_scene.chip_item.pin_connectors_D
    )
    wire = WireItem(connectors[pin], led.conn_anode)
    view.chip_scene.addItem(wire)
    view.chip_scene.wires.append(wire)
    return led


def test_step_button_executes_one_instruction(qtbot, view, fake_emu):
    qtbot.mouseClick(view.btn_step, Qt.MouseButton.LeftButton)
    assert fake_emu.steps == 1


def test_reset_button_resets_emulator(qtbot, view, fake_emu):
    before = fake_emu.resets
    qtbot.mouseClick(view.btn_reset, Qt.MouseButton.LeftButton)
    assert fake_emu.resets == before + 1


def test_run_button_toggles_timer_and_label(qtbot, view):
    qtbot.mouseClick(view.btn_run, Qt.MouseButton.LeftButton)
    assert view.timer.isActive()
    assert view.btn_run.text() == "Stop"
    qtbot.mouseClick(view.btn_run, Qt.MouseButton.LeftButton)
    assert not view.timer.isActive()
    assert view.btn_run.text() == "Run"


class FakeClock:
    """Controlled time: the test decides how many seconds have "passed"."""

    def __init__(self):
        self.t = 100.0

    def __call__(self):
        return self.t


def test_run_executes_cycles_in_batches_by_clock_frequency(view, fake_emu):
    clock = FakeClock()
    view._now = clock
    view.controller.clock_hz = 1_000_000
    view._on_run()  # start: counting time from t=100

    clock.t += 0.016  # 16 ms passed
    view._timer_tick()
    assert fake_emu.runs == [16_000]  # 1 MHz * 0.016 s

    clock.t += 0.05
    view._timer_tick()
    assert fake_emu.runs == [16_000, 50_000]
    view._on_run()  # stop


def test_run_speed_selector_scales_batches(view, fake_emu):
    clock = FakeClock()
    view._now = clock
    view.controller.clock_hz = 1_000_000
    view.speed_box.setCurrentIndex(1)  # 0.1×
    view._on_run()

    clock.t += 0.1
    view._timer_tick()
    assert fake_emu.runs == [10_000]
    view._on_run()


def test_run_updates_gui_after_each_batch(view, fake_emu):
    clock = FakeClock()
    view._now = clock
    calls = []
    view.update_callback = lambda: calls.append(1)
    view._on_run()
    clock.t += 0.016
    view._timer_tick()
    assert calls == [1]
    view._on_run()


def test_run_timer_ticks_by_itself(qtbot, view, fake_emu):
    qtbot.mouseClick(view.btn_run, Qt.MouseButton.LeftButton)
    qtbot.waitUntil(lambda: len(fake_emu.runs) >= 3, timeout=2000)
    qtbot.mouseClick(view.btn_run, Qt.MouseButton.LeftButton)


def test_emulator_error_stops_run_and_is_shown(qtbot, view, fake_emu):
    fake_emu.fail_run = RuntimeError("Illegal AVR instruction")

    qtbot.mouseClick(view.btn_run, Qt.MouseButton.LeftButton)
    qtbot.waitUntil(lambda: bool(view.shown_errors), timeout=2000)

    assert "Illegal AVR instruction" in view.shown_errors[0]
    assert not view.timer.isActive()  # auto-run stopped
    assert view.btn_run.text() == "Run"
    qtbot.wait(200)
    assert len(view.shown_errors) == 1  # one message, not one per tick


def test_update_callback_is_called_after_successful_step(qtbot, view):
    calls = []
    view.update_callback = lambda: calls.append(1)
    qtbot.mouseClick(view.btn_step, Qt.MouseButton.LeftButton)
    assert calls == [1]


def test_update_callback_not_called_after_failed_step(qtbot, view, fake_emu):
    fake_emu.fail_step = RuntimeError("boom")
    calls = []
    view.update_callback = lambda: calls.append(1)
    qtbot.mouseClick(view.btn_step, Qt.MouseButton.LeftButton)
    assert calls == []
    assert view.shown_errors


def test_led_lights_when_connected_pin_is_output_high(view, fake_emu):
    led = connect_pin_to_led(view, "B", 5)
    fake_emu.memory.update({DDRB: 0x20, PORTB: 0x20})

    view.chip_scene.refresh_pins()

    assert led.body.brush().color() == RED


def test_led_off_when_pin_is_output_low(view, fake_emu):
    led = connect_pin_to_led(view, "B", 5)
    fake_emu.memory.update({DDRB: 0x20, PORTB: 0x20})
    view.chip_scene.refresh_pins()
    fake_emu.memory[PORTB] = 0x00

    view.chip_scene.refresh_pins()

    assert led.body.brush().color() == GRAY


def test_led_ignores_input_pin(view, fake_emu):
    led = connect_pin_to_led(view, "B", 5)
    fake_emu.memory.update({DDRB: 0x00, PORTB: 0x20})  # an input with a pull-up

    view.chip_scene.refresh_pins()

    assert led.body.brush().color() == GRAY


def test_led_on_port_d_follows_port_d_registers(view, fake_emu):
    led = connect_pin_to_led(view, "D", 3)
    fake_emu.memory.update({DDRD: 0x08, PORTD: 0x08, DDRB: 0x00, PORTB: 0x08})

    view.chip_scene.refresh_pins()

    assert led.body.brush().color() == RED


def test_chip_pin_indicator_colors(view, fake_emu):
    view.show_chip()
    fake_emu.memory.update({DDRB: 0x03, PORTB: 0x01, DDRD: 0x00, PORTD: 0x04})

    view.chip_scene.refresh_pins()

    chip = view.chip_scene.chip_item
    assert chip.pin_ellipses_B[0].brush().color() == QColor(Qt.GlobalColor.red)
    assert chip.pin_ellipses_B[1].brush().color() == QColor(0, 128, 0)
    assert chip.pin_ellipses_B[2].brush().color() == GRAY
    assert chip.pin_ellipses_D[2].brush().color() == QColor(Qt.GlobalColor.yellow)


def test_pin_labels_name_the_right_port(view):
    view.show_chip()
    chip = view.chip_scene.chip_item
    assert [chip.pin_labels_B[i].toPlainText() for i in range(8)] == [
        f"PB{i}" for i in range(8)
    ]
    assert [chip.pin_labels_D[i].toPlainText() for i in range(8)] == [
        f"PD{i}" for i in range(8)
    ]


# ---- Disassembler ------------------------------------------------------------


@pytest.fixture
def disasm(qtbot):
    from gui.disasm_model import Instruction, Listing
    from gui.disasm_widget import DisassemblyWidget

    w = DisassemblyWidget()
    qtbot.addWidget(w)
    w.set_listing(
        Listing(
            [
                Instruction(0, 2, (0x940C, 0x0034), "jmp 0x0068", True),
                Instruction(2, 1, (0x0000,), "nop", True),
                Instruction(3, 1, (0x95D8,), ".word 0x95D8", False),
            ]
        )
    )
    return w


def test_disasm_widget_shows_byte_address_code_and_text(disasm):
    assert disasm.rowCount() == 3
    assert [disasm.item(0, c).text() for c in range(3)] == [
        "0x0000",
        "940C 0034",
        "jmp 0x0068",
    ]
    assert disasm.item(2, 0).text() == "0x0006"  # word 3 -> byte 6


def test_disasm_widget_highlights_current_pc_row(disasm):
    disasm.highlight_pc(2)
    assert disasm.current_row() == 1
    assert disasm.item(1, 2).background().color() == QColor(255, 235, 130)

    disasm.highlight_pc(0)  # the previous row is cleared
    assert disasm.current_row() == 0
    assert disasm.item(1, 2).background().color() != QColor(255, 235, 130)


def test_disasm_widget_pc_outside_program_clears_highlight(disasm):
    disasm.highlight_pc(2)
    disasm.highlight_pc(0x100)
    assert disasm.current_row() is None


def test_disasm_widget_without_listing_is_safe(qtbot):
    from gui.disasm_widget import DisassemblyWidget

    w = DisassemblyWidget()
    qtbot.addWidget(w)
    w.highlight_pc(5)
    w.set_listing(None)
    assert w.rowCount() == 0
