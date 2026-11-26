# Decision log

Format: decision — reason — what was rejected. Add new entries at the top.

### Timer0 is not `IMemoryMapped`; its registers are separate `MMIORegister`s

Timer0 registers are not contiguous (TIFR0 `0x35`, TCCR0A–OCR0B `0x44–0x48`, TIMSK0 `0x6E`), while
`IMemoryMapped` assumes one continuous range. The timer gets time through `MemoryBus::add_tickable`,
and the registers are mapped one by one. Rejected: one wide range `0x35–0x6E` with offset decoding
(it would overlap other registers and the fallback for unknown I/O).

### Time comes from a single cycle counter

`AVR_CPU::commit_cycles` is the only place where cycles accumulate; it also provides the number passed to
`MemoryBus::tick`. Previously the extra cycles (taken branches, skips) went only to the total counter, and
the peripherals "lived" about 25 % slower.

### Interrupts: request level + a set of pending vectors

A peripheral raises `flag & mask` as a level; the CPU keeps a `std::set` of vectors (lower address = higher
priority). Rejected: a single `pending_vector` slot (a second interrupt overwrote the first).

### Decoder: a lookup table over all 65536 opcodes

The (mask/pattern) table is expanded at construction time; `decode()` is one array access. The
"first match wins" semantics are preserved. The lookup is shared by all decoders using the same table.
Rejected: a linear scan of the table for every instruction.

### Unknown I/O is RAZ/WI, not an exception

Real programs (Arduino `init()`) write to registers that are absent from the model. The range `0x20–0xFF`
outside known devices reads as zero and ignores writes. Addresses beyond the data memory still throw
(that is a program error, not a missing peripheral).

### Stack as in hardware

`push_pc`: the low byte first (at the higher address), then the high byte. This lets programs that read the
stack directly, and debuggers, see the familiar order.

### A custom test framework instead of GoogleTest

The development environment had no network access to download GoogleTest; the framework is minimal
(`test_framework.h`). It can be replaced with GoogleTest/Catch2 later: the tests only use `TEST`, `CHECK`,
`CHECK_EQ`, `CHECK_THROWS`.

### ALU verification: an oracle on wide arithmetic

The implementation computes flags with bit formulas from the manual, while the tests use `int` arithmetic
(overflow, borrow, signed range). Two independent implementations; the test iterates over all operands.
Rejected: hand-written value tables (they cover only a small part).

### Build normalization: CMake presets, C++20, `-Werror`

The code needs C++20 (designated initializers); C++23 adds nothing significant. Presets (CMake ≥ 3.21):
`debug` (ASan+UBSan, `-g3 -O0`), `release` (`-O2`), `release-python` (+ the module for the GUI).
`CMakeUserPresets.json` is personal (absolute paths) and is not committed.

### Stopping `run()` and the "busy" state

`pause()`/`stop()` set an atomic flag checked between instructions; the flag is cleared at the start of every
`run_for_cycles()`, so a stale request does not cancel the next run. The rest of the API does not block while
`run()` is active (that would deadlock with the GIL); it throws `EmulatorBusyError` instead.

### Free-threaded Python is not used

For a responsive GUI it is enough to release the GIL during `run()` (`py::gil_scoped_release`). Free-threading
is a separate interpreter build, requires support in every extension, and `Emulator` is not thread-safe.
