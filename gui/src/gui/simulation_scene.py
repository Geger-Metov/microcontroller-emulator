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
        )  # временно белый для отладки

        self.chip_item = None  # чипа изначально нет
        self.connect_mode = False
        self.first_connector = None
        self.wires = []

    def show_chip(self):
        """Добавить чип на сцену, если его ещё нет."""
        if self.chip_item is None:
            self.chip_item = ChipItem(self.controller)
            self.addItem(self.chip_item)
            print(f"[Scene] Chip added, total items: {len(self.items())}")

    def hide_chip(self):
        """Убрать чип со сцены."""
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
        # Блокируем перемещение всех MovableItem в режиме соединения
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
                        )  # подсветка первого коннектора
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
            # Клик мимо коннектора – сброс
            if self.first_connector:
                self.first_connector.setBrush(QBrush(Qt.GlobalColor.yellow))
                self.first_connector = None
            event.ignore()
        else:
            super().mousePressEvent(event)

    def _pin_of(self, connector):
        """Ключ пина чипа ("B", 5) для коннектора или None, если это не пин."""
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
        """Пары (пин чипа, светодиод) по всем проводам сцены."""
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
        """Обновить цвета пинов чипа и состояние подключённой периферии."""
        if not self.controller or not self.chip_item:
            return

        ports = self.controller.get_port_registers()
        b, d = ports["B"], ports["D"]

        # Индикаторы пинов на корпусе чипа
        self.chip_item.update_pin_colors(b.ddr, b.port, d.ddr, d.port)

        # Светодиоды: горят, если соединены с выходным пином с единицей
        outputs = output_levels({"B": (b.ddr, b.port), "D": (d.ddr, d.port)})
        for led, on in led_states(self.led_links(), outputs).items():
            led.set_state(on)
        # Другие устройства (например, кнопка) пока не управляются.

        # Обновляем позиции проводов (на случай перемещения элементов)
        for w in self.wires:
            w.update_position()
