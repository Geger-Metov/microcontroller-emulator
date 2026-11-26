"""Smoke tests for the main window (pytest-qt, fake emulator)."""

import pytest

pytest.importorskip("PySide6", reason="PySide6 is not installed")
pytest.importorskip("pytestqt", reason="pytest-qt is not installed")

from gui import main as main_module
from gui.main import (
    PAGE_COMPONENTS,
    PAGE_DISASM,
    PAGE_DOCS,
    PAGE_PROPERTIES,
    MainWindow,
)


@pytest.fixture
def window(qtbot, controller, monkeypatch):
    monkeypatch.setattr(main_module, "SimController", lambda: controller)
    w = MainWindow()
    qtbot.addWidget(w)
    return w


def test_window_builds_with_default_pages(window):
    assert window.windowTitle() == "MCU Simulator v0.1"
    assert window.central_tabs.count() == 4
    assert window.right_stack.count() == 5
    assert window.right_stack.currentIndex() == PAGE_COMPONENTS


def test_central_tab_switches_right_panel(window):
    window.central_tabs.setCurrentIndex(1)
    assert window.right_stack.currentIndex() == PAGE_DOCS
    assert window.right_panel_title.text() == "Documentation"
    window.central_tabs.setCurrentIndex(3)
    assert window.right_stack.currentIndex() == PAGE_PROPERTIES
    assert window.right_panel_title.text() == "Properties"


def test_right_panel_menu_can_open_disassembler(window):
    window.switch_right_panel(PAGE_DISASM)
    assert window.right_panel_title.text() == "Disassembler"


def test_selecting_a_component_shows_its_name_and_properties(window):
    item = window.component_list.item(0)
    assert item.text() == "ATmega328P"
    window.on_component_selected(item)
    assert "ATmega328P" in window.properties_view.toPlainText()
    assert "Microcontroller" in window.properties_view.toPlainText()
    assert window.right_stack.currentIndex() == PAGE_PROPERTIES
    assert window.sim_view.chip_scene is not None
