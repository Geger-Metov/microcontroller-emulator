"""Сколько тактов выполнять за кадр, чтобы эмулятор шёл в реальном времени.

Чистая логика без Qt: время передаётся параметром, поэтому проверяется тестами
без таймеров.
"""

DEFAULT_CLOCK_HZ = 16_000_000  # ATmega328P на плате Arduino Uno


class RunPacer:
    """Переводит прошедшее реальное время в число тактов МК.

    * `speed` — множитель (0.1 = в десять раз медленнее реального времени).
    * `max_frame_s` — потолок времени, засчитываемого за один кадр. Если GUI
      «подвис» или ядро не успевает, эмуляция просто идёт медленнее, а не
      пытается нагнать упущенное и не зависает окончательно.
    * Дробная часть такта переносится на следующий кадр, поэтому на
      длинной дистанции средняя скорость точная.
    """

    def __init__(
        self,
        clock_hz: int = DEFAULT_CLOCK_HZ,
        speed: float = 1.0,
        max_frame_s: float = 0.1,
    ) -> None:
        self.clock_hz = clock_hz
        self.speed = speed
        self.max_frame_s = max_frame_s
        self._last: float | None = None
        self._carry = 0.0

    def start(self, now: float) -> None:
        """Начать отсчёт с момента `now` (секунды, монотонные часы)."""
        self._last = now
        self._carry = 0.0

    def cycles_since_last(self, now: float) -> int:
        """Такты, которые нужно выполнить с прошлого вызова до `now`."""
        if self._last is None:
            self.start(now)
            return 0
        dt = min(max(now - self._last, 0.0), self.max_frame_s)
        self._last = now
        self._carry += dt * self.clock_hz * self.speed
        # 1e-6 такта — защита от ошибки float (0.016 * 1e6 = 15999.99999…)
        cycles = int(self._carry + 1e-6)
        self._carry -= cycles
        return cycles
