#include <cstdio>
#include <stdexcept>

#include "AVRExecutionContext.h"
#include "AVR_CPU.h"
#include "avr_instructions.h"

AVR_CPU::AVR_CPU(std::unique_ptr<IDecoder> decoder)
    : decoder_(std::move(decoder)) {}

void AVR_CPU::reset() {
  if (!regfile_)
    throw std::runtime_error("AVR_CPU: register file not attached");
  if (!sreg_)
    throw std::runtime_error("AVR_CPU: SREG device not attached");
  if (!data_bus_)
    throw std::runtime_error("AVR_CPU: data bus not attached");
  if (!code_bus_)
    throw std::runtime_error("AVR_CPU: code bus not attached");

  pc_ = 0;
  sp_ = initial_sp_;
  total_cycles_ = 0;
  last_instruction_cycles_ = 0;
  extra_cycles_ = 0;
  pc_modified_ = false;
  irq_inhibit_ = false;
  sleeping_ = false;
  pending_interrupts_.clear();
}

void AVR_CPU::commit_cycles(uint16_t cycles) {
  last_instruction_cycles_ = cycles;
  total_cycles_ += cycles;
}

void AVR_CPU::step() {
  extra_cycles_ = 0;
  irq_inhibit_ = false;

  // SLEEP: ядро стоит (периферия продолжает тикать — режим Idle), пока не
  // появится запрос прерывания. Источник должен быть разрешён (запрос в
  // pending_interrupts_ уже учитывает маску периферии).
  if (sleeping_) {
    if (pending_interrupts_.empty()) {
      commit_cycles(1);
      return;
    }
    sleeping_ = false;
    if (get_flag(7)) { // прерывание принимается сразу, возврат — после SLEEP
      commit_cycles(0);
      take_pending_interrupt();
      return;
    }
    // I сброшен: просто просыпаемся и выполняем следующую инструкцию.
  }

  // 1. FETCH
  uint32_t current_pc = pc_;
  uint16_t first_word = read_program_word(current_pc);

  // 2. INITIAL DECODE
  avr::DecodedInstruction decoded{};
  const avr::InstructionDesc *desc = decoder_->decode(first_word, decoded);

  if (!desc) {
    fprintf(stderr, "[CPU] Unknown instruction at PC=0x%X, opcode=0x%04X\n",
            current_pc, first_word);
    throw std::runtime_error("Illegal AVR instruction");
  }

  // 3. FETCH SECOND WORD IF NEEDED
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

  // 4. EXECUTE
  pc_modified_ = false;
  AVRExecutionContext ctx(*this, *regfile_, *sreg_, *data_bus_);
  desc->execute(ctx, decoded);

  // 5. DEFAULT PC ADVANCE
  if (!pc_modified_) {
    pc_ = current_pc + desc->length;
  }

  // 6. CYCLES: базовая стоимость + доп. такты, добавленные инструкцией
  commit_cycles(static_cast<uint16_t>(desc->cycles + extra_cycles_));

  // 7. INTERRUPTS
  // Прерывания принимаются между инструкциями. Если инструкция была SEI/RETI,
  // следующая должна выполниться до обработки прерывания.
  if (!pending_interrupts_.empty() && get_flag(7) && !irq_inhibit_)
    take_pending_interrupt();
}

void AVR_CPU::take_pending_interrupt() {
  uint16_t vector = *pending_interrupts_.begin(); // наивысший приоритет
  service_interrupt(vector, pc_);

  // Вход в прерывание: 2 такта на push PC + 2 такта на переход к вектору.
  constexpr uint16_t interrupt_cycles = 4;
  last_instruction_cycles_ += interrupt_cycles;
  total_cycles_ += interrupt_cycles;
}

uint8_t AVR_CPU::instruction_length_at(uint32_t pc) const {
  uint16_t op = read_program_word(pc);
  avr::DecodedInstruction decoded;
  const auto *desc = decoder_->decode(op, decoded);
  // Нераспознанное слово (данные) при пропуске считаем однословным.
  return desc ? desc->length : 1;
}

void AVR_CPU::tick(uint64_t) {
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

void AVR_CPU::push_pc(uint32_t pc) {
  // Как на реальном AVR: сначала младший байт, потом старший. В памяти старший
  // байт оказывается по меньшему адресу (SP+1), младший — по большему.
  for (uint8_t i = 0; i < pc_width_bytes_; ++i) {
    push8(static_cast<uint8_t>((pc >> (8 * i)) & 0xFF));
  }
}

uint32_t AVR_CPU::pop_pc() {
  uint32_t pc = 0;
  for (int i = pc_width_bytes_ - 1; i >= 0; --i) {
    pc |= static_cast<uint32_t>(pop8()) << (8 * i);
  }
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
  pending_interrupts_.insert(vector);
}

void AVR_CPU::clear_interrupt(uint16_t vector) {
  pending_interrupts_.erase(vector);
}

void AVR_CPU::set_interrupt_ack_handler(uint16_t vector,
                                        std::function<void()> on_ack) {
  interrupt_ack_[vector] = std::move(on_ack);
}

void AVR_CPU::service_interrupt(uint16_t vector, uint32_t return_pc) {
  pending_interrupts_.erase(vector);

  // Аппаратный сброс флага прерывания (например TOV0) при входе в вектор.
  if (auto it = interrupt_ack_.find(vector); it != interrupt_ack_.end())
    it->second();

  push_pc(return_pc);
  set_flag(7, false); // clear I flag
  pc_ = vector;
  pc_modified_ = true;
}
