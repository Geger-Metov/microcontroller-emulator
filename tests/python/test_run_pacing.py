"""RunPacer: converting real time into MCU cycles (pure logic)."""

import pytest

from gui.run_pacing import DEFAULT_CLOCK_HZ, RunPacer


def test_one_second_at_clock_frequency():
    p = RunPacer(clock_hz=1_000_000, max_frame_s=10)
    p.start(0.0)
    assert p.cycles_since_last(1.0) == 1_000_000


def test_first_call_without_start_returns_zero_and_starts_clock():
    p = RunPacer(clock_hz=1000, max_frame_s=10)
    assert p.cycles_since_last(5.0) == 0
    assert p.cycles_since_last(5.5) == 500


def test_speed_scales_cycles():
    p = RunPacer(clock_hz=1_000_000, speed=0.1)
    p.start(0.0)
    assert p.cycles_since_last(0.05) == 5_000


def test_long_pause_is_clamped_to_max_frame():
    p = RunPacer(clock_hz=1_000_000, max_frame_s=0.1)
    p.start(0.0)
    assert p.cycles_since_last(30.0) == 100_000  # not 30 million
    assert p.cycles_since_last(30.016) == 16_000  # then it goes on as usual


def test_fractional_cycles_are_carried_over():
    p = RunPacer(clock_hz=10, max_frame_s=10)  # 10 Hz: 0.25 s = 2.5 cycles
    p.start(0.0)
    total = sum(p.cycles_since_last(0.25 * i) for i in range(1, 9))  # 2 s in total
    assert total == 20


def test_time_going_backwards_gives_zero():
    p = RunPacer(clock_hz=1000)
    p.start(10.0)
    assert p.cycles_since_last(9.0) == 0


def test_default_clock_is_arduino_uno():
    assert DEFAULT_CLOCK_HZ == 16_000_000


@pytest.mark.parametrize("speed", [1.0, 0.1, 0.01])
def test_average_speed_is_exact_over_many_frames(speed):
    p = RunPacer(clock_hz=16_000_000, speed=speed)
    p.start(0.0)
    total = sum(p.cycles_since_last(0.016 * i) for i in range(1, 626))  # 10 s
    assert abs(total - 16_000_000 * speed * 10) <= 1
