from PySide6.QtGui import QBrush, QColor
from PySide6.QtWidgets import (
    QAbstractItemView,
    QHeaderView,
    QTableWidget,
    QTableWidgetItem,
    QWidget,
)

from gui.disasm_model import Listing, format_words

CURRENT_ROW_COLOR = QColor(255, 235, 130)
UNKNOWN_COLOR = QColor(150, 150, 150)


class DisassemblyWidget(QTableWidget):
    """Листинг программы: адрес (байтовый), машинный код, инструкция.

    Строку, на которую указывает PC, подсвечивает и прокручивает в видимую часть.
    """

    COLUMNS = ("Адрес", "Код", "Инструкция")

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(0, len(self.COLUMNS), parent)
        self.setHorizontalHeaderLabels(list(self.COLUMNS))
        self.verticalHeader().setVisible(False)
        self.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.setShowGrid(False)
        header = self.horizontalHeader()
        header.setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        header.setStretchLastSection(True)
        self.listing: Listing | None = None
        self._current: int | None = None

    def set_listing(self, listing: Listing | None) -> None:
        self.listing = listing
        self._current = None
        self.setRowCount(0)
        if listing is None:
            return
        self.setUpdatesEnabled(False)
        try:
            self.setRowCount(len(listing))
            for i, row in enumerate(listing.rows):
                self.setItem(i, 0, QTableWidgetItem(f"0x{row.byte_addr:04X}"))
                self.setItem(i, 1, QTableWidgetItem(format_words(row)))
                text = QTableWidgetItem(row.text)
                if not row.known:
                    text.setForeground(QBrush(UNKNOWN_COLOR))
                self.setItem(i, 2, text)
        finally:
            self.setUpdatesEnabled(True)

    def highlight_pc(self, pc: int) -> None:
        """Подсветить строку, на которую указывает PC (в словах)."""
        if self.listing is None:
            return
        row = self.listing.row_for_pc(pc)
        if row == self._current:
            return
        self._paint_row(self._current, None)
        self._paint_row(row, CURRENT_ROW_COLOR)
        self._current = row
        item = self.item(row, 0) if row is not None else None
        if item is not None:
            self.scrollToItem(item, QAbstractItemView.ScrollHint.EnsureVisible)

    def current_row(self) -> int | None:
        return self._current

    def _paint_row(self, row: int | None, color: QColor | None) -> None:
        if row is None:
            return
        brush = QBrush(color) if color is not None else QBrush()
        for col in range(self.columnCount()):
            item = self.item(row, col)
            if item is not None:
                item.setBackground(brush)
