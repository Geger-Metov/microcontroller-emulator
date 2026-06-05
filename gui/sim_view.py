# sim_view.py
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QGraphicsView, QFileDialog, QSizePolicy
)
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QBrush, QColor, QPainter
from simulation_scene import SimulationScene

class SimulatorView(QWidget):
    def __init__(self, controller=None):
        super().__init__()
        self.controller = controller
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        layout = QVBoxLayout(self)

        # Панель инструментов
        toolbar = QHBoxLayout()
        self.btn_load = QPushButton("Load HEX")
        self.btn_run = QPushButton("Run")
        self.btn_step = QPushButton("Step")
        self.btn_reset = QPushButton("Reset")
        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setCheckable(True)

        toolbar.addWidget(self.btn_load)
        toolbar.addWidget(self.btn_run)
        toolbar.addWidget(self.btn_step)
        toolbar.addWidget(self.btn_reset)
        toolbar.addWidget(self.btn_connect)
        toolbar.addStretch()
        layout.addLayout(toolbar)

        # Графическая сцена
        self.graphics_view = QGraphicsView()
        self.chip_scene = SimulationScene(self.controller)
        self.graphics_view.setScene(self.chip_scene)

        # === Критически важные настройки ===
        self.graphics_view.setRenderHint(QPainter.Antialiasing)
        self.graphics_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)   # растягиваем на всё доступное пространство
        self.graphics_view.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.graphics_view.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.graphics_view.setSceneRect(self.chip_scene.sceneRect())     # явно задаём прямоугольник сцены
        self.graphics_view.fitInView(self.chip_scene.sceneRect(), Qt.KeepAspectRatio)
        self.graphics_view.setBackgroundBrush(QBrush(QColor(200, 200, 200)))   # серый фон для контраста
        # =================================

        layout.addWidget(self.graphics_view)

        # Отладка: временная жёлтая надпись (потом удалим)
        # test_label = QLabel("IF YOU SEE THIS, GRAPHICS VIEW IS BROKEN")
        # test_label.setStyleSheet("background: yellow; font-size: 20px;")
        # layout.addWidget(test_label)

        # Таймер
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._timer_tick)

        # Сигналы
        self.btn_load.clicked.connect(self._on_load)
        self.btn_step.clicked.connect(self._on_step)
        self.btn_run.clicked.connect(self._on_run)
        self.btn_reset.clicked.connect(self._on_reset)
        self.btn_connect.toggled.connect(self._on_toggle_connect)

        # Колбэк обновления GUI
        self.update_callback = None

    def resizeEvent(self, event):
        """Автоматически подгонять масштаб сцены при изменении размера виджета."""
        super().resizeEvent(event)
        if self.chip_scene:
            self.graphics_view.fitInView(self.chip_scene.sceneRect(), Qt.KeepAspectRatio)

    def set_controller(self, ctrl):
        self.controller = ctrl
        if self.chip_scene:
            self.chip_scene.controller = ctrl

    def show_chip(self):
        if self.chip_scene:
            self.chip_scene.show_chip()

    def hide_chip(self):
        if self.chip_scene:
            self.chip_scene.hide_chip()

    def add_led(self):
        if self.chip_scene:
            self.chip_scene.add_led()

    def add_button(self):
        if self.chip_scene:
            self.chip_scene.add_button()

    def _on_toggle_connect(self, checked):
        if self.chip_scene:
            self.chip_scene.set_connect_mode(checked)

    def _on_load(self):
        if not self.controller: return
        path, _ = QFileDialog.getOpenFileName(self, "Open HEX file", "", "Intel HEX (*.hex);;All Files (*)")
        if path:
            self.controller.load_hex(path)

    def _on_step(self):
        if self.controller:
            self.controller.step()
            if self.update_callback:
                self.update_callback()

    def _on_run(self):
        if not self.controller: return
        if self.timer.isActive():
            self.timer.stop()
            self.btn_run.setText("Run")
        else:
            self.timer.start(50)
            self.btn_run.setText("Stop")

    def _on_reset(self):
        if self.controller:
            self.controller.reset()
            if self.update_callback:
                self.update_callback()

    def _timer_tick(self):
        if self.controller:
            self.controller.step()
            if self.update_callback:
                self.update_callback()
