import sys

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QApplication,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMenu,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QStackedWidget,
    QStatusBar,
    QTabWidget,
    QTextBrowser,
    QVBoxLayout,
    QWidget,
)

from gui.controller import SimController
from gui.cpu_state_widget import CpuStateWidget
from gui.disasm_widget import DisassemblyWidget
from gui.sim_view import SimulatorView

# Indices of the pages in the right panel's QStackedWidget.
PAGE_COMPONENTS, PAGE_DOCS, PAGE_PROPERTIES, PAGE_CPU, PAGE_DISASM = range(5)
PAGE_TITLES = {
    PAGE_COMPONENTS: "Components",
    PAGE_DOCS: "Documentation",
    PAGE_PROPERTIES: "Properties",
    PAGE_CPU: "CPU",
    PAGE_DISASM: "Disassembler",
}
# Which right-panel page goes with each central tab.
PAGE_FOR_TAB = [PAGE_COMPONENTS, PAGE_DOCS, PAGE_COMPONENTS, PAGE_PROPERTIES]

COMPONENT_TYPES = {
    "ATmega328P": "Microcontroller (AVR)",
    "LED": "LED",
    "Button": "Button",
}
DOCS = [
    "ATmega328P guide",
    "Register reference",
    "Code examples for beginners",
    "Communication protocols: UART, I2C, SPI",
]


class PlaceholderWidget(QWidget):
    """A tab for a feature that is not implemented yet."""

    def __init__(self, text: str = "Under development"):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(text))


class CodeEditor(PlaceholderWidget):
    def __init__(self):
        super().__init__("Code editor\n(under development)")
        text_edit = QPlainTextEdit()
        text_edit.setPlaceholderText("The code editor will be here...")
        self.layout().addWidget(text_edit)


class ComponentsPanel(PlaceholderWidget):
    def __init__(self):
        super().__init__("Components panel\n(under development)")


class DocsViewer(PlaceholderWidget):
    def __init__(self):
        super().__init__("Documentation viewer\n(under development)")


class MainWindow(QMainWindow):
    component_selected = Signal(str)
    doc_requested = Signal(str)

    # Created in init_ui().
    central_tabs: QTabWidget
    left_panel: QListWidget
    right_panel: QWidget
    bottom_panel: QTextBrowser
    sim_view: SimulatorView

    def __init__(self):
        super().__init__()
        self.setWindowTitle("MCU Simulator v0.1")
        self.resize(1200, 700)

        self.controller = SimController()
        self.controller.set_gui_callback(self.on_emulator_updated)

        self.init_ui()
        self.connect_signals()

    # ---- construction -------------------------------------------------------

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(2, 2, 2, 2)
        main_layout.setSpacing(2)

        self.central_tabs = self.create_central_tabs()
        self.left_panel = self.create_left_panel()
        self.right_panel = self.create_right_panel()
        self.bottom_panel = self.create_bottom_panel()

        # CPU state and disassembler pages (PAGE_CPU, PAGE_DISASM).
        self.cpu_widget = CpuStateWidget()
        self.right_stack.addWidget(self.cpu_widget)
        self.disasm_widget = DisassemblyWidget()
        self.right_stack.addWidget(self.disasm_widget)
        self.right_panel_menu.addAction(
            PAGE_TITLES[PAGE_CPU], lambda: self.switch_right_panel(PAGE_CPU)
        )
        self.right_panel_menu.addAction(
            PAGE_TITLES[PAGE_DISASM], lambda: self.switch_right_panel(PAGE_DISASM)
        )
        self.sim_view.program_loaded_callback = self.reload_disassembly

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.addWidget(self.left_panel)
        splitter.addWidget(self.central_tabs)
        splitter.addWidget(self.right_panel)
        splitter.setSizes([120, 800, 280])

        main_layout.addWidget(splitter, 3)
        main_layout.addWidget(self.bottom_panel, 1)

        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Ready")

        self.sim_view.set_controller(self.controller)

    def create_left_panel(self) -> QListWidget:
        panel = QListWidget()
        panel.addItems(["Simulator", "Code", "Components", "Documentation"])
        panel.setCurrentRow(0)
        panel.currentRowChanged.connect(self.central_tabs.setCurrentIndex)
        return panel

    def create_central_tabs(self) -> QTabWidget:
        tabs = QTabWidget()
        tabs.setDocumentMode(True)
        tabs.setMovable(True)
        tabs.setTabsClosable(False)

        self.sim_view = SimulatorView()
        self.sim_view.update_callback = self.on_emulator_updated
        self.code_editor = CodeEditor()
        self.components_view = ComponentsPanel()
        self.docs_viewer = DocsViewer()

        tabs.addTab(self.sim_view, "Simulator")
        tabs.addTab(self.code_editor, "Code editor")
        tabs.addTab(self.components_view, "Components")
        tabs.addTab(self.docs_viewer, "Documentation")
        return tabs

    def create_right_panel(self) -> QWidget:
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)

        # The title is a button with a drop-down menu for switching pages.
        self.right_panel_title = QPushButton(PAGE_TITLES[PAGE_COMPONENTS])
        self.right_panel_menu = QMenu(self)
        for page in (PAGE_COMPONENTS, PAGE_DOCS, PAGE_PROPERTIES):
            self._add_page_action(page)
        self.right_panel_title.setMenu(self.right_panel_menu)

        self.right_stack = QStackedWidget()
        self.component_list = QListWidget()
        self.component_list.addItems(list(COMPONENT_TYPES))
        self.docs_list = QListWidget()
        self.docs_list.addItems(DOCS)
        self.properties_view = QTextBrowser()
        # The order must match PAGE_COMPONENTS, PAGE_DOCS, PAGE_PROPERTIES.
        self.right_stack.addWidget(self.component_list)
        self.right_stack.addWidget(self.docs_list)
        self.right_stack.addWidget(self.properties_view)

        layout.addWidget(self.right_panel_title)
        layout.addWidget(self.right_stack)
        return panel

    def _add_page_action(self, page: int) -> None:
        """Add a menu entry that opens `page`. A method (not a loop lambda), so
        each entry captures its own `page`."""
        self.right_panel_menu.addAction(
            PAGE_TITLES[page], lambda: self.switch_right_panel(page)
        )

    def create_bottom_panel(self) -> QTextBrowser:
        panel = QTextBrowser()
        panel.append("<b>MCU Simulator</b> v0.1")
        self._log("INFO", "#4da6ff", "Simulator initialized successfully", panel)
        self._log("WARN", "#ffcc00", "Some modules are still under development", panel)
        self._log("INFO", "#4da6ff", "Use the left panel for navigation", panel)
        return panel

    def connect_signals(self):
        self.central_tabs.currentChanged.connect(self.on_central_tab_changed)
        self.component_list.itemClicked.connect(self.on_component_selected)
        self.docs_list.itemClicked.connect(self.on_doc_selected)

    # ---- helpers ------------------------------------------------------------

    def _log(
        self,
        tag: str,
        color: str,
        message: str,
        panel: QTextBrowser | None = None,
    ):
        """Append a line like `[TAG] message` to the terminal panel."""
        target = panel if panel is not None else self.bottom_panel
        target.append(f"<span style='color:{color}'>[{tag}]</span> {message}")

    # ---- slots --------------------------------------------------------------

    def on_emulator_updated(self):
        """Called by the controller after every action."""
        state = self.controller.get_state()
        self.cpu_widget.update_state(state)
        self.disasm_widget.highlight_pc(state["pc"])
        if self.sim_view.chip_scene is not None:
            self.sim_view.chip_scene.refresh_pins()
        self.bottom_panel.append(
            f"Step executed. PC=0x{state['pc']:04X}, SREG=0x{state['sreg']:02X}"
        )

    def reload_disassembly(self):
        """The firmware was reloaded — rebuild the listing."""
        self.disasm_widget.set_listing(self.controller.get_listing())
        self.disasm_widget.highlight_pc(self.controller.get_state()["pc"])

    def on_central_tab_changed(self, index: int):
        """When the central tab changes, switch the right panel to match."""
        if index < len(PAGE_FOR_TAB):
            self.switch_right_panel(PAGE_FOR_TAB[index])
        self.status_bar.showMessage(f"Active tab: {self.central_tabs.tabText(index)}")

    def switch_right_panel(self, page: int):
        self.right_stack.setCurrentIndex(page)
        self.right_panel_title.setText(PAGE_TITLES[page])

    def on_component_selected(self, item: QListWidgetItem):
        component = item.text()
        if component == "ATmega328P":
            self.sim_view.show_chip()
        elif component == "LED":
            self.sim_view.add_led()
        elif component == "Button":
            self.sim_view.add_button()

        self._log("COMPONENT", "#4da6ff", f"Selected component: {component}")
        self.properties_view.setHtml(f"""
            <h3>Component properties</h3>
            <p><b>Name:</b> {component}</p>
            <p><b>Type:</b> {COMPONENT_TYPES.get(component, "Unknown type")}</p>
            <hr>
            <p><i>Detailed properties will be available once the module is implemented</i></p>
        """)
        self.switch_right_panel(PAGE_PROPERTIES)

    def on_doc_selected(self, item: QListWidgetItem):
        self._log("DOCS", "#ffcc00", f"Document requested: {item.text()}")


def main() -> int:
    """Entry point of the `gui` console script."""
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
