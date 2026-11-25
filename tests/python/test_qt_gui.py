"""Тесты виджетов (pytest-qt, без экрана: QT_QPA_PLATFORM=offscreen).

Эмулятор подставной (FakeEmulator): проверяем именно поведение GUI.
"""

import pytest

pytest.importorskip("PySide6", reason="PySide6 не установлен")
pytest.importorskip("pytestqt", reason="pytest-qt не установлен")

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
    # Модальный диалог в тесте повис бы навсегда — запоминаем сообщения вместо него.
    v.shown_errors = []
    monkeypatch.setattr(v, "show_error", v.shown_errors.append)
    return v


def connect_pin_to_led(view, port, pin):
    """Чип + светодиод + провод от пина к светодиоду; возвращает светодиод."""
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
    """Управляемое время: тест сам решает, сколько секунд «прошло»."""

    def __init__(self):
        self.t = 100.0

    def __call__(self):
        return self.t


def test_run_executes_cycles_in_batches_by_clock_frequency(view, fake_emu):
    clock = FakeClock()
    view._now = clock
    view.controller.clock_hz = 1_000_000
    view._on_run()  # старт: отсчёт времени с t=100

    clock.t += 0.016  # прошло 16 мс
    view._timer_tick()
    assert fake_emu.runs == [16_000]  # 1 МГц * 0.016 с

    clock.t += 0.05
    view._timer_tick()
    assert fake_emu.runs == [16_000, 50_000]
    view._on_run()  # стоп


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
    assert not view.timer.isActive()  # автозапуск остановлен
    assert view.btn_run.text() == "Run"
    qtbot.wait(200)
    assert len(view.shown_errors) == 1  # сообщение одно, а не по каждому тику


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
    fake_emu.memory.update({DDRB: 0x00, PORTB: 0x20})  # вход с подтяжкой

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
