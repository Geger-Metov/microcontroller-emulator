import sys
from PySide6.QtWidgets import (
    QStackedWidget, QMainWindow, QWidget, QVBoxLayout, QSplitter, QTextBrowser, 
    QListWidget, QApplication, QLabel, QPlainTextEdit, QStatusBar, QTabWidget, 
    QPushButton, QMenu, QToolBar
    )
from PySide6.QtCore import Qt, Signal, QTimer
from PySide6.QtGui import QAction

from sim_view import SimulatorView
from controller import SimController
from cpu_state_widget import CpuStateWidget

# Временные заглушки для неготовых виджетов
class PlaceholderWidget(QWidget):
    def __init__(self, text="В разработке", parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        label = QLabel(text)
        layout.addWidget(label)

# Заглушки для конкретных виджетов
class CodeEditor(PlaceholderWidget):
    def __init__(self):
        super().__init__("Редактор кода\n(в разработке)")
        text_edit = QPlainTextEdit()
        text_edit.setPlaceholderText("Здесь будет редактор кода...")
        self.layout().addWidget(text_edit)
        

class ComponentsPanel(PlaceholderWidget):
    def __init__(self):
        super().__init__("Панель компонентов\n(в разработке)")

class DocsViewer(PlaceholderWidget):
    def __init__(self):
        super().__init__("Просмотр документации\n(в разработке)")


class MainWindow(QMainWindow):
    # Сигналы для связи между виджетами
    component_selected = Signal(str)
    doc_requested = Signal(str)

    def __init__(self):
        super().__init__()
        self.setWindowTitle("MCU SImulator v0.1")
        self.resize(1200, 700)

        # Создание виджетов
        self.central_tabs = None
        self.left_panel = None
        self.right_panel = None
        self.bottom_panel = None

        self.controller = SimController()
        self.controller.set_gui_callback(self.on_emulator_updated)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.timer_tick)

        # Инициализация UI
        self.init_ui()
        self.connect_signals()

    def timer_tick(self):
        state = self.controller.step_and_get_state()
        self.cpu_widget.update_state(state)
        if self.sim_view.chip_scene:
            self.sim_view.chip_scene.refresh_pins()
        self.bottom_panel.append(f"PC=0x{state['pc']:04X} PB5={state['regs'][5]:02X}")

    def init_ui(self):
        # Центральный виджет
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # Главный layout - ВЕРТИКАЛЬНЫЙ (сверху вниз)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(2, 2, 2, 2)
        main_layout.setSpacing(2)

        # ГОРИЗОНТАЛЬНЫЙ сплиттер для лево-центр-право
        horizontal_splitter = QSplitter(Qt.Orientation.Horizontal)

        # 1.
        self.central_tabs = self.create_central_tabs()
        # 2.
        self.left_panel = self.create_left_panel()
        # 3.
        self.right_panel = self.create_right_panel()
        # 4.
        self.bottom_panel = self.create_bottom_panel()

        self.cpu_widget = CpuStateWidget()
        self.right_stack.addWidget(self.cpu_widget)
        self.right_panel_menu.addAction("CPU", lambda: self.switch_right_panel(3))

        # Собираем горизонтальный сплиттер
        horizontal_splitter.addWidget(self.left_panel)
        horizontal_splitter.addWidget(self.central_tabs)
        horizontal_splitter.addWidget(self.right_panel)

        # Начальные размеры панелей
        horizontal_splitter.setSizes([120, 800, 280])
        # не уверен, что это нужно. Это вроде запрещает изменять размер
        # horizontal_splitter.setChildrenCollapsible(False)

        # Добавляем в вертикальный layout
        main_layout.addWidget(horizontal_splitter, 3)
        main_layout.addWidget(self.bottom_panel, 1)

        # Статус бар (что и зачем?)
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Готов к работе")

        self.sim_view.set_controller(self.controller)

    def on_emulator_updated(self):
        """Вызывается контроллером после каждого действия."""
        state = self.controller.get_state()
        self.cpu_widget.update_state(state)

        # обновляем чип
        if self.sim_view.chip_scene is not None:
            self.sim_view.chip_scene.refresh_pins()

        # Пишем в терминал краткую информацию
        msg = f"Step executed. PC=0x{state['pc']:04X}, SREG=0x{state['sreg']:02X}"
        self.bottom_panel.append(msg)

    def create_left_panel(self):
        panel = QListWidget()
        # Не уверен, что мне это нужно
        # panel.setMaximumWidth(150)

        # Стиль, наверное, лучше вынести в отдельные файлы
        # Так как еще хочу добавить смену светлой и темной темы

        # nav.setStyleSheet("""
        #    QListWidget {
        #        background-color: #2b2b2b;
        #        color: #e0e0e0;
        #        border: none;
        #        font-size: 12px;
        #    }
        #    QListWidget::item {
        #        padding: 12px 8px;
        #        border-bottom: 1px solid #3a3a3a;
        #    }
        #    QListWidget::item:hover {
        #        background-color: #3a3a3a;
        #    }
        #    QListWidget::item:selected {
        #        background-color: #4CAF50;
        #        color: white;
        #        border-left: 4px solid #2196F3;
        #    }
        #""")

        panel.addItems([
            "Симулятор",
            "Код",
            "Компоненты",
            "Документация"
        ])
        panel.setCurrentRow(0)

        # Связываем выбор с вкладками
        panel.currentRowChanged.connect(self.central_tabs.setCurrentIndex)

        return panel
    
    def create_central_tabs(self):
        # Центральная панель с вкладками
        tab_widget = QTabWidget()
        # Зачем и почему?
        tab_widget.setDocumentMode(True)
        tab_widget.setMovable(True)
        tab_widget.setTabsClosable(False)

        # Создаём содержимое вкладок
        self.sim_view = SimulatorView()
        self.sim_view.update_callback = self.on_emulator_updated
        self.code_editor = CodeEditor()
        self.components_view = ComponentsPanel()
        self.docs_viewer = DocsViewer()
        

        # Добавление вкладок
        # Интересно, как тут и во всем приложении смену 
        # языка сделать анг/рус
        tab_widget.addTab(self.sim_view, "Симулятор")
        tab_widget.addTab(self.code_editor, "Редактор кода")
        tab_widget.addTab(self.components_view, "Компоненты")
        tab_widget.addTab(self.docs_viewer, "Документация")

        # Настройка стиля вкладок
        #tab_widget.setStyleSheet("""
        #    QTabWidget::pane {
        #        border: 1px solid #ccc;
        #        background-color: white;
        #    }
        #    QTabBar::tab {
        #        background-color: #f0f0f0;
        #        padding: 8px 16px;
        #        margin-right: 2px;
        #        border: 1px solid #ccc;
        #        border-bottom: none;
        #    }
        #    QTabBar::tab:selected {
        #        background-color: white;
        #        font-weight: bold;
        #    }
        #    QTabBar::tab:hover {
        #        background-color: #e0e0e0;
        #    }
        #""")

        return tab_widget
    
    def create_right_panel(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)

        # Заголовок панели с выпадающим меню
        self.right_panel_title = QPushButton("ТЕСТ")
        #self.right_panel_title.setStyleSheet("""
        #    QPushButton {
        #        background-color: #4CAF50;
        #        color: white;
        #        font-weight: bold;
        #        padding: 10px;
        #        border: none;
        #        text-align: left;
        #        font-size: 13px;
        #    }
        #    QPushButton:hover {
        #        background-color: #45a049;
        #    }
        #    QPushButton::menu-indicator {
        #        image: none;
        #    }
        #""")

        # Меню для переключения режимов правой панели
        self.right_panel_menu = QMenu(self)
        # Добавление действий в меню
        self.right_panel_menu.addAction("Компоненты", lambda: self.switch_right_panel(0))
        self.right_panel_menu.addAction("Документация", lambda: self.switch_right_panel(1))
        self.right_panel_menu.addAction("Свойства", lambda: self.switch_right_panel(2))
        
        # Устанавливаем меню для кнопки
        self.right_panel_title.setMenu(self.right_panel_menu)
        
        # Стек виджетов для разного контента
        self.right_stack = QStackedWidget()

        # 1. Список компонентов
        self.component_list = QListWidget()
        #self.component_list.setStyleSheet("""
        #    QListWidget {
        #        border: none;
        #        background-color: white;
        #    }
        #    QListWidget::item {
        #        padding: 8px;
        #        border-bottom: 1px solid #eee;
        #    }
        #    QListWidget::item:hover {
        #        background-color: #f5f5f5;
        #    }
        #    QListWidget::item:selected {
        #        background-color: #e3f2fd;
        #        color: #1565c0;
        #    }
        #""")

        # Примерные компоненты
        components = [
            "ATmega328P", "LED", "Button"
        ]
        self.component_list.addItems(components)

        # 2. Список документации
        self.docs_list = QListWidget()
        # self.docs_list.setStyleSheet(self.component_list.styleSheet())
        docs = [
            "📘 Руководство по ATmega328P",
            "📗 Справочник по регистрам",
            "📙 Примеры кода для начинающих",
            "📕 Протоколы связи: UART, I2C, SPI"
        ]
        self.docs_list.addItems(docs)

        # Не уверен, что это нужно 
        # 3.
        self.properties_view = QTextBrowser()
        #self.properties_view.setStyleSheet("""
        #    QTextBrowser {
        #        background-color: white;
        #        border: none;
        #        padding: 10px;
        #        font-size: 12px;
        #    }
        #""")
        #self.properties_view.setHtml("""
        #    <h3 style='color: #4CAF50;'>Свойства</h3>
        #    <p>Выберите компонент для просмотра свойств</p>
        #""")

        # Добавляем виджеты в стек
        self.right_stack.addWidget(self.component_list)
        self.right_stack.addWidget(self.docs_list)
        self.right_stack.addWidget(self.properties_view)

        # Сборка layout
        layout.addWidget(self.right_panel_title)
        layout.addWidget(self.right_stack)

        return panel
    
    def create_bottom_panel(self):
        panel = QTextBrowser()
        #panel.setMaximumHeight(200)
        #panel.setMinimumHeight(150)
        #panel.setStyleSheet("""
        #    QTextBrowser {
        #        background-color: #1e1e1e;
        #        color: #d4d4d4;
        #        font-family: 'Consolas', 'Monaco', monospace;
        #        font-size: 11px;
        #        border-top: 2px solid #4CAF50;
        #        padding: 5px;
        #    }
        #""")

                # Заголовок и начальное сообщение
        panel.append("<span style='color:#4CAF50'>╔═══════════════════════════════════════════════════════╗</span>")
        panel.append("<span style='color:#4CAF50'>║</span><b>  MCU SIMULATOR TERMINAL v0.1</b>".ljust(53) + "<span style='color:#4CAF50'>║</span>")
        panel.append("<span style='color:#4CAF50'>╚═══════════════════════════════════════════════════════╝</span>")
        panel.append("")
        panel.append("<span style='color:#4da6ff'>[INFO]</span> Симулятор инициализирован успешно")
        panel.append("<span style='color:#4da6ff'>[INFO]</span> Готов к работе")
        panel.append("<span style='color:#ffcc00'>[WARN]</span> Некоторые модули находятся в стадии разработки")
        panel.append("<span style='color:#4da6ff'>[INFO]</span> Используйте левую панель для навигации")

        return panel
    
    # перенести в sim_view, не всем вкладкам нужно
    def create_toolbar(self):
        toolbar = QToolBar("Главная панель")
        # Что это?
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        # Кнопки управления симулятором
        actions = [
            ("▶️", "Старт симуляции", self.start_simulation),
            ("⏸️", "Пауза", self.pause_simulation),
            ("⏹️", "Стоп", self.stop_simulation),
            ("🔄", "Сброс", self.reset_simulation),
            ("", "", None),  # Разделитель
            ("💾", "Сохранить проект", self.save_project),
            ("📂", "Открыть проект", self.open_project),
            ("", "", None),
            ("⚙️", "Настройки", self.open_settings),
            ("❓", "Справка", self.open_help)
        ]

        for icon, text, callback in actions:
            if icon == "" and text == "":
                toolbar.addSeparator()
            else:
                action = QAction(f"{icon} {text}", self)
                if callback:
                    action.triggered.connect(callback)
                toolbar.addAction(action)

    def connect_signals(self):
        """Соединяем сигналы между виджетами"""
        # При переключении центральных вкладок меняем правую панель
        self.central_tabs.currentChanged.connect(self.on_central_tab_changed)

        # При выборе компонента
        self.component_list.itemClicked.connect(self.on_component_selected)
        self.docs_list.itemClicked.connect(self.on_doc_selected)
    
    def on_central_tab_changed(self, index):
        """При смене вкладки меняем содержимое правой панели"""
        titles = ["📋 Компоненты", "📚 Документация", "📚 Документация", "⚙️ Свойства"]
        panel_index = [0, 1, 0, 2]  # Какой виджет показывать для каждой вкладки
        
        self.right_panel_title.setText(titles[index])
        self.right_stack.setCurrentIndex(panel_index[index])
        
        # Обновляем статус бар
        tab_names = ["Симулятор", "Редактор кода", "Компоненты", "Документация"]
        self.status_bar.showMessage(f"Активная вкладка: {tab_names[index]}")
    
    def switch_right_panel(self, index):
        self.right_stack.setCurrentIndex(index)
        titles = ["📋 Компоненты", "📚 Документация", "⚙️ Свойства", "🖥️ CPU"]
        if index < len(titles):
            self.right_panel_title.setText(titles[index])
    
    def on_component_selected(self, item):
        component = item.text()
        if component == "ATmega328P":
            self.sim_view.show_chip()
        elif component == "LED":
            self.sim_view.add_led()
        elif component == "Button":
            self.sim_view.add_button()
        else:
            # Для других компонентов – возможно скрываем чип или ничего
            pass
        
        # Показываем в терминале
        self.bottom_panel.append(f"<span style='color:#4da6ff'>[COMPONENT]</span> Выбран компонент: {component[2:]}")

        # Обновляем свойства
        self.properties_view.setHtml(f"""
            <h3 style='color: #4CAF50;'>Свойства компонента</h3>
            <p><b>Название:</b> {component[2:]}</p>
            <p><b>Тип:</b> {self.get_component_type(component)}</p>
            <p><b>Статус:</b> <span style='color:green'>Доступен</span></p>
            <p><b>Описание:</b> Компонент готов к добавлению в симуляцию</p>
            <hr>
            <p style='color: #666;'><i>Детальные свойства будут доступны после реализации модуля</i></p>
        """)
        
        # Переключаем на вкладку свойств
        self.right_stack.setCurrentIndex(2)
        self.right_panel_title.setText("⚙️ Свойства")
    
    def get_component_type(self, component_name):
        mapping = {
            "ATmega328P": "Микроконтроллер (AVR)",
            "LED": "Светодиод",
            "Button": "Кнопка"
        }
        return mapping.get(component_name, "Неизвестный тип")
    
    def on_doc_selected(self, item):
        """Обработка выбора документации"""
        doc = item.text()
        self.bottom_panel.append(f"<span style='color:#ffcc00'>[DOCS]</span> Запрошен документ: {doc[2:]}")
    
    def start_simulation(self):
        """Запуск симуляции"""
        self.bottom_panel.append("<span style='color:#4da6ff'>[SIM]</span> Запуск симуляции...")
        self.status_bar.showMessage("Симуляция выполняется")
    
    def pause_simulation(self):
        """Пауза симуляции"""
        self.bottom_panel.append("<span style='color:#ffcc00'>[SIM]</span> Симуляция на паузе")
    
    def stop_simulation(self):
        """Остановка симуляции"""
        self.bottom_panel.append("<span style='color:#ff6600'>[SIM]</span> Симуляция остановлена")
    
    def reset_simulation(self):
        """Сброс симуляции"""
        self.bottom_panel.append("<span style='color:#4da6ff'>[SIM]</span> Симуляция сброшена")
    
    def save_project(self):
        """Сохранение проекта"""
        self.bottom_panel.append("<span style='color:#4da6ff'>[FILE]</span> Запрос на сохранение проекта")
    
    def open_project(self):
        """Открытие проекта"""
        self.bottom_panel.append("<span style='color:#4da6ff'>[FILE]</span> Запрос на открытие проекта")
    
    def open_settings(self):
        """Открытие настроек"""
        self.bottom_panel.append("<span style='color:#9966ff'>[SYS]</span> Открытие настроек")
    
    def open_help(self):
        """Открытие справки"""
        self.bottom_panel.append("<span style='color:#9966ff'>[SYS]</span> Открытие справки")


if __name__ == '__main__':
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    app.exec()
