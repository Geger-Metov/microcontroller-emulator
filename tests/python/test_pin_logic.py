from gui.pin_logic import bit, led_states, output_levels, pin_kind


def test_bit_extracts_single_bit():
    assert bit(0b0010_0000, 5) == 1
    assert bit(0b0010_0000, 4) == 0


def test_pin_kind_covers_all_four_modes():
    assert pin_kind(1, 1) == "output_high"
    assert pin_kind(1, 0) == "output_low"
    assert pin_kind(0, 1) == "input_pullup"
    assert pin_kind(0, 0) == "input"


def test_output_levels_contains_only_outputs():
    # DDRB=0x21 (PB0, PB5 — выходы), PORTB=0x20 (PB5 = 1); порт D — все входы
    levels = output_levels({"B": (0x21, 0x20), "D": (0x00, 0xFF)})
    assert levels == {("B", 0): 0, ("B", 5): 1}


def test_output_levels_handles_both_ports():
    levels = output_levels({"B": (0x01, 0x01), "D": (0x80, 0x00)})
    assert levels == {("B", 0): 1, ("D", 7): 0}


def test_led_on_only_when_output_is_high():
    outputs = {("B", 5): 1, ("B", 4): 0}
    states = led_states([(("B", 5), "led1"), (("B", 4), "led2")], outputs)
    assert states == {"led1": True, "led2": False}


def test_led_on_input_pin_is_left_unchanged():
    # пин не выход — состояние светодиода не должно попасть в результат
    states = led_states([(("B", 3), "led")], {("B", 5): 1})
    assert states == {}


def test_led_with_several_wires_is_on_if_any_pin_high():
    outputs = {("B", 0): 0, ("B", 1): 1}
    states = led_states([(("B", 0), "led"), (("B", 1), "led")], outputs)
    assert states == {"led": True}
    # порядок проводов не должен влиять
    states = led_states([(("B", 1), "led"), (("B", 0), "led")], outputs)
    assert states == {"led": True}
