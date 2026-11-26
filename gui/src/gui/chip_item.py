from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QPen
from PySide6.QtWidgets import (
    QGraphicsEllipseItem,
    QGraphicsItem,
    QGraphicsRectItem,
    QGraphicsTextItem,
)

from gui.interactive_items import ConnectorItem
from gui.pin_logic import bit, pin_kind

# Pin indicator color on the chip body: output=1 red, output=0 green,
# input with pull-up yellow, input gray.
PIN_COLORS = {
    "output_high": QColor(Qt.GlobalColor.red),
    "output_low": QColor(0, 128, 0),
    "input_pullup": QColor(Qt.GlobalColor.yellow),
    "input": QColor(Qt.GlobalColor.gray),
}


class ChipItem(QGraphicsRectItem):
    def __init__(self, controller, parent=None):
        super().__init__(QRectF(0, 0, 300, 240), parent)
        self.controller = controller
        self.setPos(100, 80)
        self.setPen(QPen(Qt.GlobalColor.black, 2))
        self.setBrush(QBrush(QColor(200, 200, 200)))
        self.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, True)
        self.setFlag(QGraphicsItem.GraphicsItemFlag.ItemSendsGeometryChanges, True)

        print(f"[ChipItem] Created at pos ({self.pos().x()}, {self.pos().y()})")

        # Chip label
        label = QGraphicsTextItem("ATmega328P", self)
        label.setPos(90, 100)
        label.setDefaultTextColor(Qt.GlobalColor.black)
        label.setZValue(10)

        # Port B
        self.pin_ellipses_B = {}
        self.pin_labels_B = {}
        self.pin_connectors_B = {}
        for i in range(8):
            y = 20 + i * 25
            ellipse = QGraphicsEllipseItem(QRectF(-5, y - 5, 10, 10), self)
            ellipse.setPen(QPen(Qt.GlobalColor.black, 1))
            ellipse.setBrush(QBrush(Qt.GlobalColor.gray))

            text = QGraphicsTextItem(f"PB{i}", self)
            text.setPos(-25, y - 2)  # or slightly to the right, e.g. -20
            text.setDefaultTextColor(Qt.GlobalColor.red)
            text.setZValue(100)  # high Z
            font = text.font()
            font.setPointSize(12)
            font.setBold(True)
            text.setFont(font)
            print(f"[ChipItem] PB{i} text at ({text.pos().x()}, {text.pos().y()})")

            conn = ConnectorItem(self)
            conn.setPos(10, y)

            self.pin_ellipses_B[i] = ellipse
            self.pin_labels_B[i] = text
            self.pin_connectors_B[i] = conn

        # Port D
        self.pin_ellipses_D = {}
        self.pin_labels_D = {}
        self.pin_connectors_D = {}
        for i in range(8):
            y = 20 + i * 25
            ellipse = QGraphicsEllipseItem(QRectF(295, y - 5, 10, 10), self)
            ellipse.setPen(QPen(Qt.GlobalColor.black, 1))
            ellipse.setBrush(QBrush(Qt.GlobalColor.gray))

            text = QGraphicsTextItem(f"PD{i}", self)
            text.setPos(260, y - 2)  # or slightly to the right, e.g. -20
            text.setDefaultTextColor(Qt.GlobalColor.red)
            text.setZValue(100)  # high Z
            font = text.font()
            font.setPointSize(12)
            font.setBold(True)
            text.setFont(font)
            print(f"[ChipItem] PD{i} text at ({text.pos().x()}, {text.pos().y()})")

            conn = ConnectorItem(self)
            conn.setPos(290, y)

            self.pin_ellipses_D[i] = ellipse
            self.pin_labels_D[i] = text
            self.pin_connectors_D[i] = conn

    def update_pin_colors(self, ddrB, portB, ddrD, portD):
        for ellipses, ddr, port in (
            (self.pin_ellipses_B, ddrB, portB),
            (self.pin_ellipses_D, ddrD, portD),
        ):
            for i in range(8):
                kind = pin_kind(bit(ddr, i), bit(port, i))
                ellipses[i].setBrush(QBrush(PIN_COLORS[kind]))
