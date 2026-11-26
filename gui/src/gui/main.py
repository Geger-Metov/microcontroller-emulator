import sys

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QAction
from PySide6.QtWidgets import (
    QApplication,
    QLabel,
    QListWidget,
    QMainWindow,
    QMenu,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QStackedWidget,
    QStatusBar,
    QTabWidget,
    QTextBrowser,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

from gui.controller import SimController
from gui.cpu_state_widget import CpuStateWidget
from gui.disasm_widget import DisassemblyWidget
from gui.sim_view import SimulatorView


# Temporary stubs for widgets that are not ready
class PlaceholderWidget(QWidget):
    def __init__(self, text="Under development", parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        label = QLabel(text)
        layout.addWidget(label)


# Stubs for specific widgets
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
    # Signals for communication between widgets
    component_selected = Signal(str)
    doc_requested = Signal(str)

    central_tabs: QTabWidget
    left_panel: QListWidget
    right_panel: QWidget
    bottom_panel: QTextBrowser

    def __init__(self):
        super().__init__()
        self.setWindowTitle("MCU SImulator v0.1")
        self.resize(1200, 700)

        # Widgets are created in init_ui(); the type is declared at class level.
        self.controller = SimController()
        self.controller.set_gui_callback(self.on_emulator_updated)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.timer_tick)

        # UI initialization
        self.init_ui()
        self.connect_signals()

    def timer_tick(self):
        state = self.controller.step_and_get_state()
        self.cpu_widget.update_state(state)
        if self.sim_view.chip_scene:
            self.sim_view.chip_scene.refresh_pins()
        self.bottom_panel.append(f"PC=0x{state['pc']:04X} PB5={state['regs'][5]:02X}")

    def init_ui(self):
        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # The main layout is VERTICAL (top to bottom)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(2, 2, 2, 2)
        main_layout.setSpacing(2)

        # HORIZONTAL splitter for left-center-right
        horizontal_splitter = QSplitter(Qt.Orientation.Horizontal)

        # 1.
        self.central_tabs = self.create_central_tabs()
        # 2.
        self.left_panel = self.create_left_panel()
        # 3.
        self.right_panel = self.create_right_panel()
        # 4.
        self.bottom_panel = self.create_bottom_panel()

        self.cpu_widget = CpuStateWidget()
        self.right_stack.addWidget(self.cpu_widget)
        self.right_panel_menu.addAction("CPU", lambda: self.switch_right_panel(3))

        self.disasm_widget = DisassemblyWidget()
        self.right_stack.addWidget(self.disasm_widget)
        self.right_panel_menu.addAction(
            "Disassembler", lambda: self.switch_right_panel(4)
        )
        self.sim_view.program_loaded_callback = self.reload_disassembly

        # Assemble the horizontal splitter
        horizontal_splitter.addWidget(self.left_panel)
        horizontal_splitter.addWidget(self.central_tabs)
        horizontal_splitter.addWidget(self.right_panel)

        # Initial panel sizes
        horizontal_splitter.setSizes([120, 800, 280])
        # not sure this is needed. It seems to forbid resizing
        # horizontal_splitter.setChildrenCollapsible(False)

        # Add to the vertical layout
        main_layout.addWidget(horizontal_splitter, 3)
        main_layout.addWidget(self.bottom_panel, 1)

        # Status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Ready")

        self.sim_view.set_controller(self.controller)

    def on_emulator_updated(self):
        """Called by the controller after every action."""
        state = self.controller.get_state()
        self.cpu_widget.update_state(state)

        self.disasm_widget.highlight_pc(state["pc"])

        # update the chip
        if self.sim_view.chip_scene is not None:
            self.sim_view.chip_scene.refresh_pins()

        # Print brief information to the terminal
        msg = f"Step executed. PC=0x{state['pc']:04X}, SREG=0x{state['sreg']:02X}"
        self.bottom_panel.append(msg)

    def reload_disassembly(self):
        """The firmware was reloaded — rebuild the listing."""
        self.disasm_widget.set_listing(self.controller.get_listing())
        self.disasm_widget.highlight_pc(self.controller.get_state()["pc"])

    def create_left_panel(self):
        panel = QListWidget()
        # Not sure I need this
        # panel.setMaximumWidth(150)

        # The style should probably be moved to separate files
        # since I also want to add switching between a light and a dark theme

        # nav.setStyleSheet("""
        #    QListWidget {
        #        background-color: #2b2b2b;
        #        color: #e0e0e0;
        #        border: none;
        #        font-size: 12px;
        #    }
        #    QListWidget::item {
        #        padding: 12px 8px;
        #        border-bottom: 1px solid #3a3a3a;
        #    }
        #    QListWidget::item:hover {
        #        background-color: #3a3a3a;
        #    }
        #    QListWidget::item:selected {
        #        background-color: #4CAF50;
        #        color: white;
        #        border-left: 4px solid #2196F3;
        #    }
        # """)

        panel.addItems(["Simulator", "Code", "Components", "Documentation"])
        panel.setCurrentRow(0)

        # Link the selection to the tabs
        panel.currentRowChanged.connect(self.central_tabs.setCurrentIndex)

        return panel

    def create_central_tabs(self):
        # Central panel with tabs
        tab_widget = QTabWidget()
        # Why and what for?
        tab_widget.setDocumentMode(True)
        tab_widget.setMovable(True)
        tab_widget.setTabsClosable(False)

        # Create the tab contents
        self.sim_view = SimulatorView()
        self.sim_view.update_callback = self.on_emulator_updated
        self.code_editor = CodeEditor()
        self.components_view = ComponentsPanel()
        self.docs_viewer = DocsViewer()

        # Add the tabs
        # I wonder how to make the language switchable here and across the app
        # (English/Russian)
        tab_widget.addTab(self.sim_view, "Simulator")
        tab_widget.addTab(self.code_editor, "Code editor")
        tab_widget.addTab(self.components_view, "Components")
        tab_widget.addTab(self.docs_viewer, "Documentation")

        # Tab styling
        # tab_widget.setStyleSheet("""
        #    QTabWidget::pane {
        #        border: 1px solid #ccc;
        #        background-color: white;
        #    }
        #    QTabBar::tab {
        #        background-color: #f0f0f0;
        #        padding: 8px 16px;
        #        margin-right: 2px;
        #        border: 1px solid #ccc;
        #        border-bottom: none;
        #    }
        #    QTabBar::tab:selected {
        #        background-color: white;
        #        font-weight: bold;
        #    }
        #    QTabBar::tab:hover {
        #        background-color: #e0e0e0;
        #    }
        # """)

        return tab_widget

    def create_right_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)

        # Panel title with a drop-down menu
        self.right_panel_title = QPushButton("TEST")
        # self.right_panel_title.setStyleSheet("""
        #    QPushButton {
        #        background-color: #4CAF50;
        #        color: white;
        #        font-weight: bold;
        #        padding: 10px;
        #        border: none;
        #        text-align: left;
        #        font-size: 13px;
        #    }
        #    QPushButton:hover {
        #        background-color: #45a049;
        #    }
        #    QPushButton::menu-indicator {
        #        image: none;
        #    }
        # """)

        # Menu for switching the right panel modes
        self.right_panel_menu = QMenu(self)
        # Add actions to the menu
        self.right_panel_menu.addAction(
            "Components", lambda: self.switch_right_panel(0)
        )
        self.right_panel_menu.addAction(
            "Documentation", lambda: self.switch_right_panel(1)
        )
        self.right_panel_menu.addAction(
            "Properties", lambda: self.switch_right_panel(2)
        )

        # Set the menu on the button
        self.right_panel_title.setMenu(self.right_panel_menu)

        # Widget stack for the different content
        self.right_stack = QStackedWidget()

        # 1. The component list
        self.component_list = QListWidget()
        # self.component_list.setStyleSheet("""
        #    QListWidget {
        #        border: none;
        #        background-color: white;
        #    }
        #    QListWidget::item {
        #        padding: 8px;
        #        border-bottom: 1px solid #eee;
        #    }
        #    QListWidget::item:hover {
        #        background-color: #f5f5f5;
        #    }
        #    QListWidget::item:selected {
        #        background-color: #e3f2fd;
        #        color: #1565c0;
        #    }
        # """)

        # Sample components
        components = ["ATmega328P", "LED", "Button"]
        self.component_list.addItems(components)

        # 2. The documentation list
        self.docs_list = QListWidget()
        # self.docs_list.setStyleSheet(self.component_list.styleSheet())
        docs = [
            "📘 ATmega328P guide",
            "📗 Register reference",
            "📙 Code examples for beginners",
            "📕 Communication protocols: UART, I2C, SPI",
        ]
        self.docs_list.addItems(docs)

        # Not sure this is needed
        # 3.
        self.properties_view = QTextBrowser()
        # self.properties_view.setStyleSheet("""
        #    QTextBrowser {
        #        background-color: white;
        #        border: none;
        #        padding: 10px;
        #        font-size: 12px;
        #    }
        # """)
        # self.properties_view.setHtml("""
        #     <h3 style='color: #4CAF50;'>Properties</h3>
        #     <p>Select a component to view its properties</p>
        # """)

        # Add the widgets to the stack
        self.right_stack.addWidget(self.component_list)
        self.right_stack.addWidget(self.docs_list)
        self.right_stack.addWidget(self.properties_view)

        # Assemble the layout
        layout.addWidget(self.right_panel_title)
        layout.addWidget(self.right_stack)

        return panel

    def create_bottom_panel(self):
        panel = QTextBrowser()
        # panel.setMaximumHeight(200)
        # panel.setMinimumHeight(150)
        # panel.setStyleSheet("""
        #    QTextBrowser {
        #        background-color: #1e1e1e;
        #        color: #d4d4d4;
        #        font-family: 'Consolas', 'Monaco', monospace;
        #        font-size: 11px;
        #        border-top: 2px solid #4CAF50;
        #        padding: 5px;
        #    }
        # """)

        # Title and the initial message
        panel.append(
            "<span style='color:#4CAF50'>╔═══════════════════════════════════════════════════════╗</span>"
        )
        panel.append(
            "<span style='color:#4CAF50'>║</span><b>  MCU SIMULATOR TERMINAL v0.1</b>".ljust(
                53
            )
            + "<span style='color:#4CAF50'>║</span>"
        )
        panel.append(
            "<span style='color:#4CAF50'>╚═══════════════════════════════════════════════════════╝</span>"
        )
        panel.append("")
        panel.append(
            "<span style='color:#4da6ff'>[INFO]</span> Simulator initialized successfully"
        )
        panel.append("<span style='color:#4da6ff'>[INFO]</span> Ready")
        panel.append(
            "<span style='color:#ffcc00'>[WARN]</span> Some modules are still under development"
        )
        panel.append(
            "<span style='color:#4da6ff'>[INFO]</span> Use the left panel for navigation"
        )

        return panel

    # move to sim_view, not every tab needs it
    def create_toolbar(self):
        toolbar = QToolBar("Main toolbar")
        # What is this?
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        # Simulator control buttons
        actions = [
            ("▶️", "Start simulation", self.start_simulation),
            ("⏸️", "Pause", self.pause_simulation),
            ("⏹️", "Stop", self.stop_simulation),
            ("🔄", "Reset", self.reset_simulation),
            ("", "", None),  # separator
            ("💾", "Save project", self.save_project),
            ("📂", "Open project", self.open_project),
            ("", "", None),
            ("⚙️", "Settings", self.open_settings),
            ("❓", "Help", self.open_help),
        ]

        for icon, text, callback in actions:
            if icon == "" and text == "":
                toolbar.addSeparator()
            else:
                action = QAction(f"{icon} {text}", self)
                if callback:
                    action.triggered.connect(callback)
                toolbar.addAction(action)

    def connect_signals(self):
        """Connect the signals between widgets"""
        # When the central tabs switch, change the right panel
        self.central_tabs.currentChanged.connect(self.on_central_tab_changed)

        # When a component is selected
        self.component_list.itemClicked.connect(self.on_component_selected)
        self.docs_list.itemClicked.connect(self.on_doc_selected)

    def on_central_tab_changed(self, index):
        """When the tab changes, change the right panel content"""
        titles = [
            "📋 Components",
            "📚 Documentation",
            "📚 Documentation",
            "⚙️ Properties",
        ]
        panel_index = [0, 1, 0, 2]  # which widget to show for each tab

        self.right_panel_title.setText(titles[index])
        self.right_stack.setCurrentIndex(panel_index[index])

        # Update the status bar
        tab_names = ["Simulator", "Code editor", "Components", "Documentation"]
        self.status_bar.showMessage(f"Active tab: {tab_names[index]}")

    def switch_right_panel(self, index):
        self.right_stack.setCurrentIndex(index)
        titles = [
            "📋 Components",
            "📚 Documentation",
            "⚙️ Properties",
            "🖥️ CPU",
            "📜 Disassembler",
        ]
        if index < len(titles):
            self.right_panel_title.setText(titles[index])

    def on_component_selected(self, item):
        component = item.text()
        if component == "ATmega328P":
            self.sim_view.show_chip()
        elif component == "LED":
            self.sim_view.add_led()
        elif component == "Button":
            self.sim_view.add_button()
        else:
            # For other components — maybe hide the chip, or nothing
            pass

        # Show in the terminal
        self.bottom_panel.append(
            f"<span style='color:#4da6ff'>[COMPONENT]</span> Selected component: {component[2:]}"
        )

        # Update the properties
        self.properties_view.setHtml(f"""
            <h3 style='color: #4CAF50;'>Component properties</h3>
            <p><b>Name:</b> {component[2:]}</p>
            <p><b>Type:</b> {self.get_component_type(component)}</p>
            <p><b>Status:</b> <span style='color:green'>Available</span></p>
            <p><b>Description:</b> The component is ready to be added to the simulation</p>
            <hr>
            <p style='color: #666;'><i>Detailed properties will be available once the module is implemented</i></p>
        """)

        # Switch to the properties tab
        self.right_stack.setCurrentIndex(2)
        self.right_panel_title.setText("⚙️ Properties")

    def get_component_type(self, component_name):
        mapping = {
            "ATmega328P": "Microcontroller (AVR)",
            "LED": "LED",
            "Button": "Button",
        }
        return mapping.get(component_name, "Unknown type")

    def on_doc_selected(self, item):
        """Handle the documentation choice"""
        doc = item.text()
        self.bottom_panel.append(
            f"<span style='color:#ffcc00'>[DOCS]</span> Document requested: {doc[2:]}"
        )

    def start_simulation(self):
        """Start the simulation"""
        self.bottom_panel.append(
            "<span style='color:#4da6ff'>[SIM]</span> Starting the simulation..."
        )
        self.status_bar.showMessage("Simulation is running")

    def pause_simulation(self):
        """Pause the simulation"""
        self.bottom_panel.append(
            "<span style='color:#ffcc00'>[SIM]</span> Simulation paused"
        )

    def stop_simulation(self):
        """Stop the simulation"""
        self.bottom_panel.append(
            "<span style='color:#ff6600'>[SIM]</span> Simulation stopped"
        )

    def reset_simulation(self):
        """Reset the simulation"""
        self.bottom_panel.append(
            "<span style='color:#4da6ff'>[SIM]</span> Simulation reset"
        )

    def save_project(self):
        """Save the project"""
        self.bottom_panel.append(
            "<span style='color:#4da6ff'>[FILE]</span> Project save requested"
        )

    def open_project(self):
        """Open a project"""
        self.bottom_panel.append(
            "<span style='color:#4da6ff'>[FILE]</span> Project open requested"
        )

    def open_settings(self):
        """Open the settings"""
        self.bottom_panel.append(
            "<span style='color:#9966ff'>[SYS]</span> Opening the settings"
        )

    def open_help(self):
        """Open the help"""
        self.bottom_panel.append(
            "<span style='color:#9966ff'>[SYS]</span> Opening the help"
        )


def main() -> int:
    """Entry point of the `gui` console script."""
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
