from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QPen
from PySide6.QtWidgets import (
    QGraphicsEllipseItem,
    QGraphicsItem,
    QGraphicsObject,
    QGraphicsRectItem,
    QGraphicsTextItem,
)


class ConnectorItem(QGraphicsEllipseItem):
    def __init__(self, parent_item=None):
        super().__init__(QRectF(-5, -5, 10, 10), parent_item)
        self.setPen(QPen(Qt.GlobalColor.blue, 1.5))
        self.setBrush(QBrush(Qt.GlobalColor.yellow))
        self.setZValue(10)
        self.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, True)
        self.owner = parent_item

    def scene_center(self) -> QPointF:
        return self.scenePos()


class MovableItem(QGraphicsObject):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, True)
        self.setFlag(QGraphicsItem.GraphicsItemFlag.ItemSendsGeometryChanges, True)
        self.connectors = []

    def add_connector(self, offset: QPointF) -> ConnectorItem:
        c = ConnectorItem(self)
        c.setPos(offset)
        self.connectors.append(c)
        return c

    def paint(self, painter, option, widget=None):
        pass  # дочерние элементы рисуют себя сами

    def boundingRect(self) -> QRectF:
        return QRectF(-50, -50, 100, 100)  # переопределяется


class LEDItem(MovableItem):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.body = QGraphicsEllipseItem(QRectF(-10, -10, 20, 20), self)
        self.body.setPen(QPen(Qt.GlobalColor.black, 1))
        self.body.setBrush(QBrush(Qt.GlobalColor.gray))
        self.label = QGraphicsTextItem("LED", self)
        self.label.setPos(-10, 12)
        self.conn_anode = self.add_connector(QPointF(0, -12))

    def set_state(self, on: bool):
        self.body.setBrush(QBrush(Qt.GlobalColor.red if on else Qt.GlobalColor.gray))

    def boundingRect(self):
        return QRectF(-15, -15, 30, 40)


class ButtonItem(MovableItem):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.body = QGraphicsRectItem(QRectF(-12, -12, 24, 24), self)
        self.body.setPen(QPen(Qt.GlobalColor.black, 1))
        self.body.setBrush(QBrush(QColor(180, 180, 180)))
        self.label = QGraphicsTextItem("BTN", self)
        self.label.setPos(-10, 14)
        self.conn1 = self.add_connector(QPointF(-8, -12))
        self.conn2 = self.add_connector(QPointF(8, -12))
        self.pressed = False

    def set_pressed(self, pressed: bool):
        self.pressed = pressed
        self.body.setBrush(
            QBrush(QColor(100, 100, 100) if pressed else QColor(180, 180, 180))
        )

    def boundingRect(self):
        return QRectF(-15, -15, 30, 35)
