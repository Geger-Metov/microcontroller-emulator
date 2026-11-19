# simulation_scene.py
from PySide6.QtCore import Qt
from PySide6.QtGui import QBrush
from PySide6.QtWidgets import QGraphicsScene

from gui.chip_item import ChipItem
from gui.interactive_items import ButtonItem, ConnectorItem, LEDItem, MovableItem
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

    def refresh_pins(self):
        """Обновить цвета пинов чипа и состояние подключённой периферии."""
        if not self.controller or not self.chip_item:
            return

        # Читаем регистры портов B и D
        DDRB = 0x24
        PORTB = 0x25
        DDRD = 0x2A
        PORTD = 0x2B

        ddrB = self.controller.emu.read_memory(DDRB)
        portB = self.controller.emu.read_memory(PORTB)
        ddrD = self.controller.emu.read_memory(DDRD)
        portD = self.controller.emu.read_memory(PORTD)

        # Обновляем индикаторы пинов на корпусе чипа
        self.chip_item.update_pin_colors(ddrB, portB, ddrD, portD)

        # Собираем выходные уровни для пинов (только если пин настроен как выход)
        pin_output_level = {}
        for i in range(8):
            if (ddrB >> i) & 1:
                pin_output_level[("B", i)] = (portB >> i) & 1
            if (ddrD >> i) & 1:
                pin_output_level[("D", i)] = (portD >> i) & 1

        # Обновляем состояние периферии по соединённым проводам
        for wire in self.wires:
            start = wire.start_conn
            end = wire.end_conn
            pin_conn = None
            peri_conn = None
            pin_port = None
            pin_num = None

            # Проверяем, принадлежит ли один из коннекторов пину чипа
            for port_char, connectors_dict in [
                ("B", self.chip_item.pin_connectors_B),
                ("D", self.chip_item.pin_connectors_D),
            ]:
                for num, conn in connectors_dict.items():
                    if conn is start:
                        pin_conn = start
                        peri_conn = end
                        pin_port = port_char
                        pin_num = num
                        break
                    elif conn is end:
                        pin_conn = end
                        peri_conn = start
                        pin_port = port_char
                        pin_num = num
                        break
                if pin_conn is not None:
                    break

            if pin_conn is None or (pin_port, pin_num) not in pin_output_level:
                continue  # не выходной пин или не соединён с чипом

            level = pin_output_level[(pin_port, pin_num)]

            # Если периферийный коннектор принадлежит светодиоду, меняем его цвет
            if isinstance(peri_conn.owner, LEDItem):
                peri_conn.owner.set_state(level == 1)
            # Сюда можно добавить обработку других устройств (например, кнопка не управляется)

        # Обновляем позиции проводов (на случай перемещения элементов)
        for w in self.wires:
            w.update_position()
