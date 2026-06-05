from PySide6.QtWidgets import QGraphicsItem, QGraphicsRectItem, QGraphicsTextItem, QGraphicsEllipseItem
from PySide6.QtCore import Qt, QRectF, QPointF
from PySide6.QtGui import QPen, QBrush, QColor
from interactive_items import ConnectorItem

class ChipItem(QGraphicsRectItem):
    def __init__(self, controller, parent=None):
        super().__init__(QRectF(0, 0, 300, 240), parent)
        self.controller = controller
        self.setPos(100, 80)
        self.setPen(QPen(Qt.black, 2))
        self.setBrush(QBrush(QColor(200, 200, 200)))
        self.setFlag(QGraphicsItem.ItemIsMovable, True)
        self.setFlag(QGraphicsItem.ItemSendsGeometryChanges, True)

        print(f"[ChipItem] Created at pos ({self.pos().x()}, {self.pos().y()})")

        # Надпись чипа
        label = QGraphicsTextItem("ATmega328P", self)
        label.setPos(90, 100)
        label.setDefaultTextColor(Qt.black)
        label.setZValue(10)

        # Порт B
        self.pin_ellipses_B = {}
        self.pin_connectors_B = {}
        for i in range(8):
            y = 20 + i * 25
            ellipse = QGraphicsEllipseItem(QRectF(-5, y-5, 10, 10), self)
            ellipse.setPen(QPen(Qt.black, 1))
            ellipse.setBrush(QBrush(Qt.gray))

            text = QGraphicsTextItem(f"PB{i}", self)
            text.setPos(-25, y-2)          # или чуть правее, например -20
            text.setDefaultTextColor(Qt.red)
            text.setZValue(100)            # высокий Z
            font = text.font()
            font.setPointSize(12)
            font.setBold(True)
            text.setFont(font)
            print(f"[ChipItem] PB{i} text at ({text.pos().x()}, {text.pos().y()})")

            conn = ConnectorItem(self)
            conn.setPos(10, y)

            self.pin_ellipses_B[i] = ellipse
            self.pin_connectors_B[i] = conn

        # Порт D
        self.pin_ellipses_D = {}
        self.pin_connectors_D = {}
        for i in range(8):
            y = 20 + i * 25
            ellipse = QGraphicsEllipseItem(QRectF(295, y-5, 10, 10), self)
            ellipse.setPen(QPen(Qt.black, 1))
            ellipse.setBrush(QBrush(Qt.gray))

            text = QGraphicsTextItem(f"PB{i}", self)
            text.setPos(260, y-2)          # или чуть правее, например -20
            text.setDefaultTextColor(Qt.red)
            text.setZValue(100)            # высокий Z
            font = text.font()
            font.setPointSize(12)
            font.setBold(True)
            text.setFont(font)
            print(f"[ChipItem] PD{i} text at ({text.pos().x()}, {text.pos().y()})")

            conn = ConnectorItem(self)
            conn.setPos(290, y)

            self.pin_ellipses_D[i] = ellipse
            self.pin_connectors_D[i] = conn

    def update_pin_colors(self, ddrB, portB, ddrD, portD):
        for i in range(8):
            ddr_bit = (ddrB >> i) & 1
            port_bit = (portB >> i) & 1
            color = Qt.red if (ddr_bit and port_bit) else (QColor(0,128,0) if ddr_bit else (Qt.yellow if port_bit else Qt.gray))
            self.pin_ellipses_B[i].setBrush(QBrush(color))

        for i in range(8):
            ddr_bit = (ddrD >> i) & 1
            port_bit = (portD >> i) & 1
            color = Qt.red if (ddr_bit and port_bit) else (QColor(0,128,0) if ddr_bit else (Qt.yellow if port_bit else Qt.gray))
            self.pin_ellipses_D[i].setBrush(QBrush(color))
