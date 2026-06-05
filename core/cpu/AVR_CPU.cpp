#include "AVR_CPU.h"

#include <stdexcept>

#include "../decoder/avr_decoder.h"
#include "AVRExecutionContext.h"

AVR_CPU::AVR_CPU(std::unique_ptr<IDecoder> decoder)
    : decoder_(std::move(decoder)) {}

void AVR_CPU::reset() {
  if (!regfile_)
    throw std::runtime_error("AVR_CPU: register file not attached");
  if (!sreg_) throw std::runtime_error("AVR_CPU: SREG device not attached");
  if (!data_bus_) throw std::runtime_error("AVR_CPU: data bus not attached");
  if (!code_bus_) throw std::runtime_error("AVR_CPU: code bus not attached");

  pc_ = 0;
  sp_ = initial_sp_;
  total_cycles_ = 0;
  skip_next_ = false;
  interrupt_pending_ = false;
}

void AVR_CPU::step() {
  // 1. HANDLE PENDING SKIP
  if (skip_next_) {
    skip_next_ = false;
    uint8_t skipped_len = instruction_length_at(pc_);

    // AVR:
    // skip consumes:
    // +1 cycle for 16-bit instruction
    // +2 cycles for 32-bit instruction
    //
    // because pipeline fetch is discarded.

    pc_ += skipped_len;
    last_instruction_cycles_ = (skipped_len == 2) ? 2 : 1;
    add_cycles(last_instruction_cycles_);
    return;
  }

  // 2. FETCH
  uint32_t current_pc = pc_;
  uint16_t first_word = read_program_word(current_pc);

  // 3. INITIAL DECODE
  avr::DecodedInstruction decoded{};
  const avr::InstructionDesc* desc = decoder_->decode(first_word, decoded);

  if (!desc) {
    fprintf(stderr, "[CPU] Unknown instruction at PC=0x%X, opcode=0x%04X\n",
            current_pc, first_word);
    throw std::runtime_error("Illegal AVR instruction");
  }

  // 4. FETCH SECOND WORD IF NEEDED
  uint32_t full_opcode = first_word;

  if (desc->length == 2) {
    uint16_t second_word = read_program_word(current_pc + 1);
    full_opcode |= (static_cast<uint32_t>(second_word) << 16);
    decoded = {};
    desc = decoder_->decode(full_opcode, decoded);

    if (!desc) {
      throw std::runtime_error("Illegal AVR 32-bit instruction");
    }
  }

  // 5. PREPARE EXECUTION
  pc_modified_ = false;
  AVRExecutionContext ctx(*this, *regfile_, *sreg_, *data_bus_);

  // 6. EXECUTE
  desc->execute(ctx, decoded);

  // 7. DEFAULT PC ADVANCE
  if (!pc_modified_) {
    pc_ = current_pc + desc->length;
  }

  // 8. CYCLES
  last_instruction_cycles_ = desc->cycles;
  add_cycles(last_instruction_cycles_);

  // 9. INTERRUPTS

  // Simplified AVR interrupt model:
  // interrupts are accepted between instructions.
  if (interrupt_pending_ && get_flag(7)) {
    service_interrupt(pc_);

    // AVR interrupt entry consumes cycles.
    // Real AVR timing depends on core.
    // MVP approximation:
    constexpr uint8_t interrupt_cycles = 4;
    last_instruction_cycles_ += interrupt_cycles;
    add_cycles(interrupt_cycles);
  }
}

uint8_t AVR_CPU::instruction_length_at(uint32_t pc) const {
  uint16_t op = read_program_word(pc);
  avr::DecodedInstruction decoded;
  const auto* desc = decoder_->decode(op, decoded);
  if (!desc) throw std::runtime_error("Illegal AVR instruction skip decode");
  return desc->length;
}

void AVR_CPU::tick(uint64_t cycles) {
  // В данной архитектуре CPU сам продвигает время через step()
  // Метод оставлен для совместимости с ITickable, можно оставить пустым
}

void AVR_CPU::push8(uint8_t val) {
  data_bus_->write(sp_, val);
  --sp_;
}

uint8_t AVR_CPU::pop8() {
  ++sp_;
  return data_bus_->read(sp_);
}

void AVR_CPU::push16(uint16_t value) {
  push8(static_cast<uint8_t>((value >> 8) & 0xFF));
  push8(static_cast<uint8_t>(value & 0xFF));
}

uint16_t AVR_CPU::pop16() {
  uint16_t hi = pop8();
  uint16_t lo = pop8();
  return (hi << 8) | lo;
}

void AVR_CPU::push32(uint32_t val) {
  push8(static_cast<uint8_t>(val & 0xFF));
  push8(static_cast<uint8_t>((val >> 8) & 0xFF));
  push8(static_cast<uint8_t>((val >> 16) & 0xFF));
  push8(static_cast<uint8_t>((val >> 24) & 0xFF));
}

uint32_t AVR_CPU::pop32() {
  uint32_t b3 = pop8();
  uint32_t b2 = pop8();
  uint32_t b1 = pop8();
  uint32_t b0 = pop8();

  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

void AVR_CPU::push_pc(uint32_t pc) {
  // нужна ли эта строка?
  // uint32_t byte_pc = pc * pc_width_bytes_;
  for (int i = pc_width_bytes_ - 1; i >= 0; --i) {
    uint8_t byte = static_cast<uint8_t>(
        (pc >> (8 * i)) & 0xFF);  // использовать ли byte_pc вместо pc?
    push8(byte);
  }
}

uint32_t AVR_CPU::pop_pc() {
  uint32_t pc = 0;
  for (uint8_t i = 0; i < pc_width_bytes_; ++i) {
    pc |= static_cast<uint32_t>(pop8()) << (8 * i);
  }
  // return pc / pc_width_bytes_
  return pc;
}

void AVR_CPU::set_flag(uint8_t flag, bool value) {
  uint8_t v = sreg_->value();
  if (value)
    v |= (1 << flag);
  else
    v &= ~(1 << flag);
  sreg_->set(v);
}

bool AVR_CPU::get_flag(uint8_t flag) const {
  return (sreg_->value() >> flag) & 1;
}

void AVR_CPU::raise_interrupt(uint16_t vector) {
  interrupt_pending_ = true;
  pending_interrupt_vector_ = vector;
}

void AVR_CPU::service_interrupt(uint32_t return_pc) {
  interrupt_pending_ = false;
  push_pc(return_pc);
  set_flag(7, false);  // clear I flag
  pc_ = pending_interrupt_vector_;
  pc_modified_ = true;
}
