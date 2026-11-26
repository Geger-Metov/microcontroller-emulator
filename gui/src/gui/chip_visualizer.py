from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QPen
from PySide6.QtWidgets import (
    QGraphicsEllipseItem,
    QGraphicsItem,
    QGraphicsRectItem,
    QGraphicsScene,
    QGraphicsTextItem,
)

from gui.interactive_items import ButtonItem, ConnectorItem, LEDItem, MovableItem
from gui.wire_item import WireItem


class ChipVisualizer(QGraphicsScene):
    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self.controller = controller
        self.setSceneRect(0, 0, 600, 400)

        # Chip body
        chip_rect = QGraphicsRectItem(100, 80, 300, 240)
        chip_rect.setPen(QPen(Qt.GlobalColor.black, 2))
        chip_rect.setBrush(QBrush(QColor(200, 200, 200)))
        self.addItem(chip_rect)
        label = QGraphicsTextItem("ATmega328P")
        label.setPos(180, 180)
        self.addItem(label)

        # Port B pins
        self.pin_ellipses = {}
        self.pin_connectors = {}
        for i in range(8):
            y = 100 + i * 25
            ellipse, conn = self._create_pin(f"PB{i}", 95, y)
            self.pin_ellipses[i] = ellipse
            self.pin_connectors[i] = conn

        self.connect_mode = False
        self.first_connector = None
        self.wires = []

    def _create_pin(self, name, x, y):
        ellipse = QGraphicsEllipseItem(QRectF(x - 5, y - 5, 10, 10))
        ellipse.setPen(QPen(Qt.GlobalColor.black, 1))
        ellipse.setBrush(QBrush(Qt.GlobalColor.gray))
        self.addItem(ellipse)

        label = QGraphicsTextItem(name)
        label.setPos(x - 12, y + 5)
        self.addItem(label)

        conn = ConnectorItem()  # no parent
        conn.setPos(x + 10, y)
        self.addItem(conn)
        return ellipse, conn

    def add_led(self, x=300, y=50):
        led = LEDItem()
        led.setPos(x, y)
        self.addItem(led)
        return led

    def add_button(self, x=340, y=50):
        btn = ButtonItem()
        btn.setPos(x, y)
        self.addItem(btn)
        return btn

    def set_connect_mode(self, enabled):
        self.connect_mode = enabled
        for item in self.items():
            if isinstance(item, MovableItem):
                item.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, not enabled)
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
                        )  # selection highlight
                    else:
                        if item is not self.first_connector:
                            wire = WireItem(self.first_connector, item)
                            self.addItem(wire)
                            self.wires.append(wire)
                        self.first_connector.setBrush(QBrush(Qt.GlobalColor.yellow))
                        self.first_connector = None
                    event.accept()
                    return
            if self.first_connector:
                self.first_connector.setBrush(QBrush(Qt.GlobalColor.yellow))
                self.first_connector = None
            event.ignore()
        else:
            super().mousePressEvent(event)

    def refresh_pins(self):
        if not self.controller:
            return
        DDRB = 0x24
        PORTB = 0x25
        ddr = self.controller.emu.read_memory(DDRB)
        port = self.controller.emu.read_memory(PORTB)
        for i in range(8):
            ddr_bit = (ddr >> i) & 1
            port_bit = (port >> i) & 1
            if ddr_bit == 1:
                color = Qt.GlobalColor.red if port_bit == 1 else QColor(0, 128, 0)
            else:
                color = Qt.GlobalColor.yellow if port_bit == 1 else Qt.GlobalColor.gray
            self.pin_ellipses[i].setBrush(QBrush(color))
        for w in self.wires:
            w.update_position()
