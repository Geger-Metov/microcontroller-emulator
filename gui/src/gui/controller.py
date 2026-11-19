from emulator_backend import Emulator


class SimController:
    def __init__(self):
        self.emu = Emulator()
        self.emu.reset()
        self._gui_callback = None

    def set_gui_callback(self, callback):
        self._gui_callback = callback

    def _notify_gui(self):
        if self._gui_callback:
            self._gui_callback()

    def load_hex(self, filepath: str):
        """Загрузить прошивку из .hex файла."""
        self.emu.load_hex(filepath)
        self.emu.reset()  # после загрузки сброс CPU
        self._notify_gui()

    def step(self):
        """Выполнить одну инструкцию."""
        self.emu.step()
        self._notify_gui()

    def run_cycles(self, count: int):
        """Выполнить указанное число циклов."""
        self.emu.run(count)
        self._notify_gui()

    def reset(self):
        self.emu.reset()
        self._notify_gui()

    def get_state(self):
        """Вернуть словарь с состоянием CPU для отображения."""
        regs = self.emu.get_registers()
        return {
            "pc": self.emu.get_pc(),
            "sp": self.emu.get_sp(),
            "sreg": self.emu.get_sreg(),
            "regs": regs,
            "cycles": self.emu.get_cycle_count(),
        }

    def get_gpio_state(self):
        # Адреса регистров GPIOB (в пространстве данных)
        # PINB = 0x23
        DDRB = 0x24
        PORTB = 0x25
        states = {}
        for i in range(8):
            # Читаем целые байты, затем извлекаем бит
            # (можно оптимизировать, читая один раз)
            ddr = (self.emu.read_memory(DDRB) >> i) & 1
            port = (self.emu.read_memory(PORTB) >> i) & 1
            # pin = (self.emu.read_memory(PINB) >> i) & 1  # не используется для цвета, но можно
            states[i] = {"ddr": ddr, "port": port}
        return states

    def step_and_get_state(self):
        self.emu.step()
        return self.get_state()
