from PySide6.QtCore import Qt
from PySide6.QtGui import QBrush
from PySide6.QtWidgets import QGraphicsScene

from gui.chip_item import ChipItem
from gui.interactive_items import ButtonItem, ConnectorItem, LEDItem, MovableItem
from gui.pin_logic import led_states, output_levels
from gui.wire_item import WireItem


class SimulationScene(QGraphicsScene):
    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self.controller = controller
        self.setSceneRect(-60, 0, 660, 400)
        self.setBackgroundBrush(
            QBrush(Qt.GlobalColor.white)
        )  # temporarily white for debugging

        self.chip_item = None  # no chip initially
        self.connect_mode = False
        self.first_connector = None
        self.wires = []

    def show_chip(self):
        """Add the chip to the scene if it is not there yet."""
        if self.chip_item is None:
            self.chip_item = ChipItem(self.controller)
            self.addItem(self.chip_item)
            print(f"[Scene] Chip added, total items: {len(self.items())}")

    def hide_chip(self):
        """Remove the chip from the scene."""
        if self.chip_item is not None:
            self.removeItem(self.chip_item)
            self.chip_item = None
            print("[Scene] Chip removed")

    def add_led(self, x=300, y=50):
        led = LEDItem()
        led.setPos(x, y)
        self.addItem(led)
        print(f"[Scene] LED added at ({x}, {y})")
        return led

    def add_button(self, x=340, y=50):
        btn = ButtonItem()
        btn.setPos(x, y)
        self.addItem(btn)
        print(f"[Scene] Button added at ({x}, {y})")
        return btn

    def set_connect_mode(self, enabled):
        self.connect_mode = enabled
        # Block moving all MovableItems in connection mode
        for item in self.items():
            if isinstance(item, MovableItem):
                item.setFlag(item.ItemIsMovable, not enabled)
        if not enabled:
            self.first_connector = None

    def mousePressEvent(self, event):
        if self.connect_mode and event.button() == Qt.MouseButton.LeftButton:
            items = self.items(
                event.scenePos(),
                Qt.ItemSelectionMode.IntersectsItemBoundingRect,
                Qt.SortOrder.DescendingOrder,
            )
            for item in items:
                if isinstance(item, ConnectorItem):
                    if self.first_connector is None:
                        self.first_connector = item
                        item.setBrush(
                            QBrush(Qt.GlobalColor.cyan)
                        )  # highlight the first connector
                        print("[Scene] First connector selected")
                    else:
                        if item is not self.first_connector:
                            wire = WireItem(self.first_connector, item)
                            self.addItem(wire)
                            self.wires.append(wire)
                            print("[Scene] Wire created")
                        self.first_connector.setBrush(QBrush(Qt.GlobalColor.yellow))
                        self.first_connector = None
                    event.accept()
                    return
            # A click away from a connector – reset
            if self.first_connector:
                self.first_connector.setBrush(QBrush(Qt.GlobalColor.yellow))
                self.first_connector = None
            event.ignore()
        else:
            super().mousePressEvent(event)

    def _pin_of(self, connector):
        """The chip pin key ("B", 5) for a connector, or None if it is not a pin."""
        if self.chip_item is None:
            return None
        for port_name, connectors in (
            ("B", self.chip_item.pin_connectors_B),
            ("D", self.chip_item.pin_connectors_D),
        ):
            for num, conn in connectors.items():
                if conn is connector:
                    return (port_name, num)
        return None

    def led_links(self):
        """(chip pin, LED) pairs for all wires of the scene."""
        links = []
        for wire in self.wires:
            for pin_conn, other in (
                (wire.start_conn, wire.end_conn),
                (wire.end_conn, wire.start_conn),
            ):
                pin = self._pin_of(pin_conn)
                if pin is not None and isinstance(other.owner, LEDItem):
                    links.append((pin, other.owner))
                    break
        return links

    def refresh_pins(self):
        """Update the chip pin colors and the state of the connected peripherals."""
        if not self.controller or not self.chip_item:
            return

        ports = self.controller.get_port_registers()
        b, d = ports["B"], ports["D"]

        # Pin indicators on the chip body
        self.chip_item.update_pin_colors(b.ddr, b.port, d.ddr, d.port)

        # LEDs: lit if connected to an output pin holding 1
        outputs = output_levels({"B": (b.ddr, b.port), "D": (d.ddr, d.port)})
        for led, on in led_states(self.led_links(), outputs).items():
            led.set_state(on)
        # Other devices (for example a button) are not driven yet.

        # Update the wire positions (in case items were moved)
        for w in self.wires:
            w.update_position()
