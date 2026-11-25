"""Модель дизассемблера: перевод строк модуля в листинг и поиск строки по PC."""

from gui.disasm_model import FLASH_WORDS, Instruction, format_words, load_listing


def row(addr, text, *words, known=True):
    return {
        "addr": addr,
        "size": len(words),
        "words": list(words),
        "text": text,
        "known": known,
    }


PROGRAM = [
    row(0, "jmp 0x0004", 0x940C, 0x0002),  # слова 0–1
    row(2, "nop", 0x0000),
    row(3, "ldi r16, 0x20", 0xE200),
    row(4, ".word 0xFFFF", 0xFFFF, known=False),  # стёртый хвост
    row(5, ".word 0xFFFF", 0xFFFF, known=False),
]


class Emu:
    def __init__(self, rows):
        self.rows = rows
        self.calls = []

    def disassemble(self, word_addr, count):
        self.calls.append((word_addr, count))
        return self.rows


def test_listing_requests_the_whole_flash():
    emu = Emu(PROGRAM)
    load_listing(emu)
    assert emu.calls == [(0, FLASH_WORDS)]


def test_erased_tail_is_trimmed_but_inner_gaps_stay():
    rows = [PROGRAM[0], row(2, ".word 0xFFFF", 0xFFFF, known=False), PROGRAM[2]]
    listing = load_listing(Emu(rows + PROGRAM[3:]))
    assert [r.text for r in listing.rows] == [
        "jmp 0x0004",
        ".word 0xFFFF",
        "ldi r16, 0x20",
    ]


def test_empty_flash_gives_empty_listing():
    listing = load_listing(Emu([row(0, ".word 0xFFFF", 0xFFFF, known=False)]))
    assert len(listing) == 0
    assert listing.row_for_pc(0) is None


def test_instruction_fields():
    listing = load_listing(Emu(PROGRAM))
    first = listing.rows[0]
    assert isinstance(first, Instruction)
    assert first.byte_addr == 0
    assert listing.rows[2].byte_addr == 6  # слово 3 -> байт 6
    assert format_words(first) == "940C 0002"


def test_row_for_pc_maps_words_to_rows_including_second_word():
    listing = load_listing(Emu(PROGRAM))
    assert listing.row_for_pc(0) == 0
    assert listing.row_for_pc(1) == 0  # второе слово JMP — та же инструкция
    assert listing.row_for_pc(2) == 1
    assert listing.row_for_pc(3) == 2


def test_row_for_pc_outside_program_is_none():
    listing = load_listing(Emu(PROGRAM))
    assert listing.row_for_pc(4) is None  # стёртая область после программы
    assert listing.row_for_pc(0x3FFF) is None
