from PySide6.QtCore import QPointF, QRectF, Qt, Signal
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
        pass  # child items draw themselves

    def boundingRect(self) -> QRectF:
        return QRectF(-50, -50, 100, 100)  # overridden


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
    """A push button: held while the left mouse button is down (Ctrl+drag moves it).

    It connects a pin to ground, so `toggled(True)` means the pin is pulled low.
    """

    toggled = Signal(bool)

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
        self.setToolTip("Hold the left mouse button to press; Ctrl+drag to move")

    def set_pressed(self, pressed: bool):
        changed = pressed != self.pressed
        self.pressed = pressed
        self.body.setBrush(
            QBrush(QColor(100, 100, 100) if pressed else QColor(180, 180, 180))
        )
        if changed:
            self.toggled.emit(pressed)

    def mousePressEvent(self, event):
        ctrl = bool(event.modifiers() & Qt.KeyboardModifier.ControlModifier)
        if event.button() == Qt.MouseButton.LeftButton and not ctrl:
            self.set_pressed(True)
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        if self.pressed:
            self.set_pressed(False)
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def boundingRect(self):
        return QRectF(-15, -15, 30, 35)
