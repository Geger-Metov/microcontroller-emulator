from PySide6.QtWidgets import QGraphicsPathItem
from PySide6.QtGui import QPainterPath, QPen, QColor

class WireItem(QGraphicsPathItem):
    def __init__(self, start_conn, end_conn, parent=None):
        super().__init__(parent)
        self.start_conn = start_conn
        self.end_conn = end_conn
        pen = QPen(QColor(0, 150, 0), 2)  # тёмно-зелёный, толщина 2
        self.setPen(pen)
        self.setZValue(999)
        self.update_path()

    def update_path(self):
        path = QPainterPath()
        if self.start_conn and self.end_conn:
            path.moveTo(self.start_conn.scene_center())
            path.lineTo(self.end_conn.scene_center())
        self.setPath(path)

    def update_position(self):
        self.update_path()
