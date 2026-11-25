import time
from collections.abc import Callable

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QBrush, QColor, QPainter
from PySide6.QtWidgets import (
    QComboBox,
    QFileDialog,
    QGraphicsView,
    QHBoxLayout,
    QMessageBox,
    QPushButton,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from gui.run_pacing import RunPacer
from gui.simulation_scene import SimulationScene

FRAME_MS = 16  # ≈60 кадров/с
# Скорость относительно реального времени (текст в списке → множитель)
SPEEDS = ("1×", 1.0), ("0.1×", 0.1), ("0.01×", 0.01)


class SimulatorView(QWidget):
    def __init__(self, controller=None):
        super().__init__()
        self.controller = controller
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        layout = QVBoxLayout(self)

        # Панель инструментов
        toolbar = QHBoxLayout()
        self.btn_load = QPushButton("Load HEX")
        self.btn_run = QPushButton("Run")
        self.btn_step = QPushButton("Step")
        self.btn_reset = QPushButton("Reset")
        self.speed_box = QComboBox()
        for label, speed in SPEEDS:
            self.speed_box.addItem(label, speed)
        self.speed_box.setToolTip("Скорость относительно реального времени")
        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setCheckable(True)

        toolbar.addWidget(self.btn_load)
        toolbar.addWidget(self.btn_run)
        toolbar.addWidget(self.btn_step)
        toolbar.addWidget(self.btn_reset)
        toolbar.addWidget(self.speed_box)
        toolbar.addWidget(self.btn_connect)
        toolbar.addStretch()
        layout.addLayout(toolbar)

        # Графическая сцена
        self.graphics_view = QGraphicsView()
        self.chip_scene = SimulationScene(self.controller)
        self.graphics_view.setScene(self.chip_scene)

        # === Критически важные настройки ===
        self.graphics_view.setRenderHint(QPainter.RenderHint.Antialiasing)
        self.graphics_view.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
        )  # растягиваем на всё доступное пространство
        self.graphics_view.setHorizontalScrollBarPolicy(
            Qt.ScrollBarPolicy.ScrollBarAlwaysOff
        )
        self.graphics_view.setVerticalScrollBarPolicy(
            Qt.ScrollBarPolicy.ScrollBarAlwaysOff
        )
        self.graphics_view.setSceneRect(
            self.chip_scene.sceneRect()
        )  # явно задаём прямоугольник сцены
        self.graphics_view.fitInView(
            self.chip_scene.sceneRect(), Qt.AspectRatioMode.KeepAspectRatio
        )
        self.graphics_view.setBackgroundBrush(
            QBrush(QColor(200, 200, 200))
        )  # серый фон для контраста
        # =================================

        layout.addWidget(self.graphics_view)

        # Отладка: временная жёлтая надпись (потом удалим)
        # test_label = QLabel("IF YOU SEE THIS, GRAPHICS VIEW IS BROKEN")
        # test_label.setStyleSheet("background: yellow; font-size: 20px;")
        # layout.addWidget(test_label)

        # Таймер
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._timer_tick)
        self.pacer = RunPacer()
        self._now = time.perf_counter  # подменяется в тестах

        # Сигналы
        self.btn_load.clicked.connect(self._on_load)
        self.btn_step.clicked.connect(self._on_step)
        self.btn_run.clicked.connect(self._on_run)
        self.btn_reset.clicked.connect(self._on_reset)
        self.btn_connect.toggled.connect(self._on_toggle_connect)
        self.speed_box.currentIndexChanged.connect(self._on_speed_changed)

        # Колбэки GUI
        self.update_callback: Callable[[], None] | None = None
        # вызывается после успешной загрузки HEX
        self.program_loaded_callback: Callable[[], None] | None = None

        if controller is not None:
            controller.set_error_callback(self._on_error)

    def resizeEvent(self, event):
        """Автоматически подгонять масштаб сцены при изменении размера виджета."""
        super().resizeEvent(event)
        if self.chip_scene:
            self.graphics_view.fitInView(
                self.chip_scene.sceneRect(), Qt.AspectRatioMode.KeepAspectRatio
            )

    def set_controller(self, ctrl):
        self.controller = ctrl
        if ctrl is not None:
            ctrl.set_error_callback(self._on_error)
        if self.chip_scene:
            self.chip_scene.controller = ctrl

    def _stop_run(self):
        self.timer.stop()
        self.btn_run.setText("Run")

    def _on_error(self, message):
        """Ошибка эмулятора: остановить автозапуск и показать причину."""
        self._stop_run()
        self.show_error(message)

    def show_error(self, message):
        QMessageBox.critical(self, "Ошибка эмулятора", message)

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
        if not self.controller:
            return
        path, _ = QFileDialog.getOpenFileName(
            self, "Open HEX file", "", "Intel HEX (*.hex);;All Files (*)"
        )
        if path and self.controller.load_hex(path) and self.program_loaded_callback:
            self.program_loaded_callback()

    def _on_step(self):
        if self.controller and self.controller.step() and self.update_callback:
            self.update_callback()

    def _on_run(self):
        if not self.controller:
            return
        if self.timer.isActive():
            self._stop_run()
        else:
            self.pacer.clock_hz = self.controller.clock_hz
            self.pacer.start(self._now())
            self.timer.start(FRAME_MS)
            self.btn_run.setText("Stop")

    def _on_speed_changed(self, _index):
        self.pacer.speed = self.speed_box.currentData()

    def _on_reset(self):
        if self.controller:
            self.controller.reset()
            if self.update_callback:
                self.update_callback()

    def _timer_tick(self):
        """Один кадр: столько тактов, сколько прошло в реальном времени."""
        if not self.controller:
            return
        cycles = self.pacer.cycles_since_last(self._now())
        if cycles and self.controller.run_cycles(cycles) and self.update_callback:
            self.update_callback()
