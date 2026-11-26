"""Data for the disassembler window (pure logic, no Qt).

The disassembler itself lives in the core (`Emulator.disassemble`), so that its output exactly
matches what the emulator executes.
"""

from bisect import bisect_right
from collections.abc import Mapping, Sequence
from typing import Any, NamedTuple, Protocol

FLASH_WORDS = 0x4000  # ATmega328P: 32 KiB = 16 K words
ERASED_WORD = 0xFFFF


class Instruction(NamedTuple):
    addr: int  # the address of the first word, in words
    size: int  # 1 or 2 words
    words: tuple[int, ...]
    text: str
    known: bool

    @property
    def byte_addr(self) -> int:
        """The address in bytes — the way objdump and the HEX file print addresses."""
        return self.addr * 2

    @property
    def is_erased(self) -> bool:
        return all(w == ERASED_WORD for w in self.words)


class DisassemblerLike(Protocol):
    def disassemble(
        self, word_addr: int, count: int
    ) -> Sequence[Mapping[str, Any]]: ...


class Listing:
    """A disassembled program: rows in ascending address order."""

    def __init__(self, rows: list[Instruction]) -> None:
        self.rows = rows
        self._addrs = [row.addr for row in rows]  # for a binary search by PC

    def __len__(self) -> int:
        return len(self.rows)

    def row_for_pc(self, pc: int) -> int | None:
        """The index of the row PC points to (in words), or None.

        ~A PC inside the second word of a two-word instruction belongs to that instruction.
        ~Beyond the program (the erased tail) there is no row.
        """
        index = bisect_right(self._addrs, pc) - 1
        if index < 0:
            return None
        row = self.rows[index]
        return index if pc < row.addr + row.size else None


def load_listing(emu: DisassemblerLike) -> Listing:
    """Disassembles the whole program; the erased flash "tail" (0xFFFF) is dropped."""
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
    """The row's machine code: '940E 0080'."""
    return " ".join(f"{w:04X}" for w in row.words)
