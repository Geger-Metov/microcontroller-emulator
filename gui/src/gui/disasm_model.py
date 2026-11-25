"""Данные для окна дизассемблера (чистая логика, без Qt).

Сам дизассемблер — в ядре (`Emulator.disassemble`), чтобы его вывод точно
совпадал с тем, что исполняет эмулятор.
"""

from bisect import bisect_right
from collections.abc import Mapping, Sequence
from typing import Any, NamedTuple, Protocol

FLASH_WORDS = 0x4000  # ATmega328P: 32 КиБ = 16 К слов
ERASED_WORD = 0xFFFF


class Instruction(NamedTuple):
    addr: int  # адрес первого слова, в словах
    size: int  # 1 или 2 слова
    words: tuple[int, ...]
    text: str
    known: bool

    @property
    def byte_addr(self) -> int:
        """Адрес в байтах — так адреса печатают objdump и HEX-файл."""
        return self.addr * 2

    @property
    def is_erased(self) -> bool:
        return all(w == ERASED_WORD for w in self.words)


class DisassemblerLike(Protocol):
    def disassemble(
        self, word_addr: int, count: int
    ) -> Sequence[Mapping[str, Any]]: ...


class Listing:
    """Дизассемблированная программа: строки по возрастанию адреса."""

    def __init__(self, rows: list[Instruction]) -> None:
        self.rows = rows
        self._addrs = [row.addr for row in rows]  # для бинарного поиска по PC

    def __len__(self) -> int:
        return len(self.rows)

    def row_for_pc(self, pc: int) -> int | None:
        """Индекс строки, на которую указывает PC (в словах), или None.

        PC внутри второго слова двухсловной инструкции относится к ней же. За
        пределами программы (стёртый хвост) строки нет.
        """
        index = bisect_right(self._addrs, pc) - 1
        if index < 0:
            return None
        row = self.rows[index]
        return index if pc < row.addr + row.size else None


def load_listing(emu: DisassemblerLike) -> Listing:
    """Дизассемблирует всю программу; стёртый «хвост» flash (0xFFFF) отбрасывается."""
    rows = [
        Instruction(
            addr=r["addr"],
            size=r["size"],
            words=tuple(r["words"]),
            text=r["text"],
            known=r["known"],
        )
        for r in emu.disassemble(0, FLASH_WORDS)
    ]
    end = len(rows)
    while end > 0 and rows[end - 1].is_erased:
        end -= 1
    return Listing(rows[:end])


def format_words(row: Instruction) -> str:
    """Машинный код строки: '940E 0080'."""
    return " ".join(f"{w:04X}" for w in row.words)
