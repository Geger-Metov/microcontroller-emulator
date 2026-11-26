"""How many cycles to execute per frame so that the emulator runs in real time.

Pure logic without Qt: the time is passed in as a parameter, so it is tested
without timers.
"""

DEFAULT_CLOCK_HZ = 16_000_000  # ATmega328P on an Arduino Uno board


class RunPacer:
    """Converts elapsed real time into a number of MCU cycles.

    ~* `speed` — a multiplier (0.1 = ten times slower than real time).
    ~* `max_frame_s` — a cap on the time counted for one frame. If the GUI
      ~  stalls or the core cannot keep up, the emulation simply runs slower instead of
      ~  trying to catch up and freezing for good.
    ~* The fractional part of a cycle carries over to the next frame, so over
      ~  a long run the average speed is exact.
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
        """Start counting from `now` (seconds, a monotonic clock)."""
        self._last = now
        self._carry = 0.0

    def cycles_since_last(self, now: float) -> int:
        """The cycles to execute from the previous call up to `now`."""
        if self._last is None:
            self.start(now)
            return 0
        dt = min(max(now - self._last, 0.0), self.max_frame_s)
        self._last = now
        self._carry += dt * self.clock_hz * self.speed
        # 1e-6 of a cycle guards against float error (0.016 * 1e6 = 15999.99999…)
        cycles = int(self._carry + 1e-6)
        self._carry -= cycles
        return cycles
