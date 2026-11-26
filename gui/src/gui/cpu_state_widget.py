from PySide6.QtWidgets import QGroupBox, QLabel, QTextEdit, QVBoxLayout, QWidget


class CpuStateWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMaximumWidth(350)  # a bit wider, but not critical

        main_layout = QVBoxLayout(self)

        # Main registers
        cpu_group = QGroupBox("CPU")
        cpu_layout = QVBoxLayout(cpu_group)
        self.pc_label = QLabel("PC: 0x0000")
        self.sp_label = QLabel("SP: 0x0000")
        self.sreg_label = QLabel("SREG: 0x00")
        cpu_layout.addWidget(self.pc_label)
        cpu_layout.addWidget(self.sp_label)
        cpu_layout.addWidget(self.sreg_label)
        main_layout.addWidget(cpu_group)

        # Registers R0-R31 in a compact form
        regs_group = QGroupBox("General Registers")
        regs_layout = QVBoxLayout(regs_group)
        self.regs_text = QTextEdit()
        self.regs_text.setReadOnly(True)
        self.regs_text.setMaximumHeight(120)
        self.regs_text.setMinimumWidth(280)
        self.regs_text.setFontFamily("monospace")
        regs_layout.addWidget(self.regs_text)
        main_layout.addWidget(regs_group)

        main_layout.addStretch()

    def update_state(self, state: dict):
        self.pc_label.setText(f"PC: 0x{state['pc']:04X}")
        self.sp_label.setText(f"SP: 0x{state['sp']:04X}")
        self.sreg_label.setText(f"SREG: 0x{state['sreg']:02X}")

        regs = state.get("regs", [0] * 32)
        lines = []
        for i in range(0, 32, 8):
            line = "  ".join(f"R{i + j:02d}:{regs[i + j]:02X}" for j in range(8))
            lines.append(line)
        self.regs_text.setPlainText("\n".join(lines))
