from gui.controller import DDRB, DDRD, PINB, PORTB, PORTD, SimController


def test_load_hex_loads_then_resets_and_notifies(controller, fake_emu):
    calls = []
    controller.set_gui_callback(lambda: calls.append("update"))
    resets_before = fake_emu.resets

    assert controller.load_hex("firmware.hex") is True

    assert fake_emu.loaded == ["firmware.hex"]
    assert fake_emu.resets == resets_before + 1  # reset after loading
    assert calls == ["update"]
    assert controller.last_error is None


def test_load_hex_error_is_reported_not_raised(controller, fake_emu):
    fake_emu.fail_load = RuntimeError("bad checksum")
    errors, updates = [], []
    controller.set_error_callback(errors.append)
    controller.set_gui_callback(lambda: updates.append(1))

    assert controller.load_hex("broken.hex") is False

    assert controller.last_error is not None
    assert "bad checksum" in controller.last_error
    assert errors == [controller.last_error]
    assert updates == []  # on an error the GUI is not updated as on success


def test_step_error_returns_false_and_keeps_message(controller, fake_emu):
    fake_emu.fail_step = RuntimeError("Illegal AVR instruction")
    errors = []
    controller.set_error_callback(errors.append)

    assert controller.step() is False
    assert "Illegal AVR instruction" in errors[0]


def test_success_clears_previous_error(controller, fake_emu):
    fake_emu.fail_step = RuntimeError("boom")
    assert controller.step() is False
    fake_emu.fail_step = None
    assert controller.step() is True
    assert controller.last_error is None


def test_error_without_callback_does_not_crash(controller, fake_emu):
    fake_emu.fail_step = RuntimeError("boom")
    assert controller.step() is False  # no callback set — just False


def test_run_cycles_passes_count_and_notifies(controller, fake_emu):
    calls = []
    controller.set_gui_callback(lambda: calls.append(1))
    assert controller.run_cycles(1000) is True
    assert fake_emu.cycles == 1000
    assert calls == [1]


def test_get_state_reports_cpu_fields(controller, fake_emu):
    controller.step()
    state = controller.get_state()
    assert state["pc"] == 1
    assert state["cycles"] == 1
    assert len(state["regs"]) == 32


def test_port_registers_read_the_right_addresses(controller, fake_emu):
    fake_emu.memory.update({PINB: 0x11, DDRB: 0x22, PORTB: 0x33})
    fake_emu.memory.update({DDRD: 0x44, PORTD: 0x55})

    regs = controller.get_port_registers()

    assert (regs["B"].pin, regs["B"].ddr, regs["B"].port) == (0x11, 0x22, 0x33)
    assert (regs["D"].ddr, regs["D"].port) == (0x44, 0x55)


def test_register_addresses_match_atmega328p_datasheet():
    # I/O address + 0x20: PINB=0x03, DDRB=0x04, PORTB=0x05, PIND=0x09, DDRD=0x0A, PORTD=0x0B
    assert (PINB, DDRB, PORTB) == (0x03 + 0x20, 0x04 + 0x20, 0x05 + 0x20)
    assert (DDRD, PORTD) == (0x0A + 0x20, 0x0B + 0x20)


def test_legacy_gpio_state_has_port_b_bits(controller, fake_emu):
    fake_emu.memory.update({DDRB: 0x20, PORTB: 0x20})
    state = controller.get_gpio_state()
    assert state[5] == {"ddr": 1, "port": 1}
    assert state[4] == {"ddr": 0, "port": 0}


def test_default_emulator_is_imported_lazily():
    # Without an argument the controller creates a real Emulator; if the module is not built —
    # an import error, not a silent substitution.
    try:
        import emulator_backend  # noqa: F401
    except ImportError:
        import pytest

        with pytest.raises(ImportError):
            SimController()
    else:
        assert SimController().get_state()["pc"] == 0


def test_get_listing_returns_listing(controller, fake_emu):
    fake_emu.program = [
        {"addr": 0, "size": 1, "words": [0], "text": "nop", "known": True}
    ]
    listing = controller.get_listing()
    assert listing is not None
    assert [r.text for r in listing.rows] == ["nop"]
    assert controller.last_error is None


def test_get_listing_error_is_reported_not_raised(controller, fake_emu):
    errors = []
    controller.set_error_callback(errors.append)

    def boom(word_addr, count):
        raise RuntimeError("flash is broken")

    fake_emu.disassemble = boom
    assert controller.get_listing() is None
    assert "flash is broken" in (controller.last_error or "")
    assert errors and "flash is broken" in errors[0]


def test_get_listing_does_not_trigger_gui_update(controller, fake_emu):
    calls = []
    controller.set_gui_callback(lambda: calls.append(1))
    controller.get_listing()
    assert calls == []


def test_set_pin_level_goes_to_the_emulator(controller, fake_emu):
    assert controller.set_pin_level("D", 2, "low") is True
    assert fake_emu.pin_levels[("D", 2)] == "low"


def test_set_pin_level_reports_errors(controller, fake_emu, monkeypatch):
    def boom(port, bit, level):
        raise ValueError("Pin number must be 0..7")

    monkeypatch.setattr(fake_emu, "set_pin_level", boom)
    assert controller.set_pin_level("B", 9, "low") is False
    assert "Pin number" in controller.last_error
