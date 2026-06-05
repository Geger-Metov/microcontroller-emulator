#include "ATmega328p.h"

#include <stdexcept>

#include "../../bus/BasicMemoryDevices.h"
#include "../../bus/CodeBus.h"
#include "../../bus/MemoryBus.h"
#include "../../cpu/AVR_CPU.h"
#include "../../decoder/avr_decoder.h"
#include "../../facade/EmulatorFacade.h"
#include "../../hex_parser/hex_parser.h"
#include "../../loader/avr_loader.h"
#include "../MCUContext.h"
#include "../peripherals/MMIORegister.h"
#include "../peripherals/RegisterFileDevice.h"
#include "../peripherals/SREGDevice.h"
#include "../peripherals/StackPointerDevice.h"
#include "../peripherals/Timer0.h"
#include "AVR_GPIO.h"

std::unique_ptr<IEmulatorFacade> ATmega328P::create() {
  // Почему auto, а не std::unique_ptr<MCUContext>
  auto ctx = build_context();
  build_memory_map(*ctx);
  connect_cpu(*ctx);
  // connect_external_pins(*ctx);

  // Почему auto, а не std::unique_ptr<EmulatorFacade>
  auto facade = std::make_unique<EmulatorFacade>();

  // === передаём ownership ===
  facade->set_cpu(std::move(ctx->cpu));
  facade->set_data_bus(std::move(ctx->data_bus));
  facade->set_code_bus(std::move(ctx->code_bus));
  facade->set_flash_device(ctx->flash.get());

  // === devices ownership ===
  facade->add_memory_device(std::move(ctx->flash));
  facade->add_memory_device(std::move(ctx->sram));
  facade->add_memory_device(std::move(ctx->regfile));
  facade->add_memory_device(std::move(ctx->sreg));
  facade->add_memory_device(std::move(ctx->gpioB));
  facade->add_memory_device(std::move(ctx->gpioD));
  facade->add_memory_device(std::move(ctx->timer0));

  facade->add_memory_device(std::move(ctx->sp_device));

  // временная заглушка
  facade->add_memory_device(std::move(ctx->eearl));
  facade->add_memory_device(std::move(ctx->eearh));
  facade->add_memory_device(std::move(ctx->eecr));

  facade->register_reset_device(ctx->regfile.get());
  facade->register_reset_device(ctx->sreg.get());
  facade->register_reset_device(ctx->gpioB.get());
  facade->add_owned_device(std::move(ctx->timer0));

  return facade;
}

void ATmega328P::connect_cpu(MCUContext& ctx) {
  ctx.cpu->attach_data_bus(ctx.data_bus.get());
  ctx.cpu->attach_code_bus(ctx.code_bus.get());
  ctx.cpu->set_register_file(ctx.regfile.get());
  ctx.cpu->set_sreg_device(ctx.sreg.get());
  ctx.timer0->attach_interrupt_sink(ctx.cpu.get());

  ctx.cpu->reset();
}

std::unique_ptr<MCUContext> ATmega328P::build_context() {
  // Почему auto, а не std::unique_ptr<MCUContext>
  auto ctx = std::make_unique<MCUContext>();

  // === memory ===
  ctx->flash = std::make_unique<FlashDevice>(32768);
  ctx->sram = std::make_unique<SRAMDevice>(2048);

  // === peripherals ===
  ctx->regfile = std::make_unique<RegisterFileDevice>();
  ctx->sreg = std::make_unique<SREGDevice>();
  ctx->gpioB = std::make_unique<AVR_GPIO>();
  ctx->gpioD = std::make_unique<AVR_GPIO>();
  ctx->timer0 = std::make_unique<Timer0>();

  // === buses ===
  ctx->data_bus = std::make_unique<MemoryBus>();
  ctx->code_bus = std::make_unique<CodeBus>();

  // === cpu ===
  // Почему auto, а не std::unique_ptr<IDecoder>
  auto decoder = std::make_unique<AVRDecoder>(
      avr::avre_plus_instruction_table, avr::avre_plus_instruction_table_size);
  ctx->cpu = std::make_unique<AVR_CPU>(std::move(decoder));
  ctx->cpu->set_initial_sp(0x08FF);  // RAMEND

  //
  ctx->sp_device = std::make_unique<StackPointerDevice>(ctx->cpu.get());

  // временная заглушка
  ctx->eearl = std::make_unique<MMIORegister>();
  ctx->eearh = std::make_unique<MMIORegister>();
  ctx->eecr = std::make_unique<MMIORegister>();

  return ctx;
}

void ATmega328P::build_memory_map(MCUContext& ctx) {
  ctx.data_bus->map(0x0000, 0x001F, ctx.regfile.get());
  ctx.data_bus->map(0x0023, 0x0025, ctx.gpioB.get());
  ctx.data_bus->map(0x0029, 0x002B, ctx.gpioD.get());
  ctx.data_bus->map(0x005F, 0x005F, ctx.sreg.get());
  ctx.data_bus->map(0x0100, 0x08FF, ctx.sram.get());

  ctx.data_bus->map(0x0045, 0x0045, &ctx.timer0->TCCR0B);
  ctx.data_bus->map(0x0046, 0x0046, &ctx.timer0->TCNT0);
  ctx.data_bus->map(0x006E, 0x006E, &ctx.timer0->TIMSK0);
  ctx.data_bus->map(0x0035, 0x0035, &ctx.timer0->TIFR0);

  ctx.data_bus->map(0x005D, 0x005E, ctx.sp_device.get());  // SPL, SPH

  ctx.code_bus->map(0x0000, 0x3FFF, ctx.flash.get());
}

void ATmega328P::build_memory_map(MCUContext& ctx) {
  ctx.data_bus->map(0x0000, 0x001F, ctx.regfile.get());
  ctx.data_bus->map(0x0023, 0x0025, ctx.gpioB.get());
  ctx.data_bus->map(0x0029, 0x002B, ctx.gpioD.get());
  ctx.data_bus->map(0x005F, 0x005F, ctx.sreg.get());
  ctx.data_bus->map(0x0100, 0x08FF, ctx.sram.get());

  ctx.data_bus->map(0x0045, 0x0045, &ctx.timer0->TCCR0B);
  ctx.data_bus->map(0x0046, 0x0046, &ctx.timer0->TCNT0);
  ctx.data_bus->map(0x006E, 0x006E, &ctx.timer0->TIMSK0);  // ext I/O
  ctx.data_bus->map(0x0035, 0x0035, &ctx.timer0->TIFR0);   // обычный I/O +0x20
  // или так правильно?
  // ctx.data_bus->map(0x0055, 0x0055, &ctx.timer0->TIFR0);

  ctx.data_bus->map(0x005D, 0x005E, ctx.sp_device.get());  // SPL, SPH

  // временная заглушка
  ctx.data_bus->map(0x0041, 0x0041, ctx.eearl.get());
  ctx.data_bus->map(0x0042, 0x0042, ctx.eearh.get());
  ctx.data_bus->map(0x003F, 0x003F, ctx.eecr.get());

  ctx.code_bus->map(0x0000, 0x3FFF, ctx.flash.get());
}

void ATmega328P::load_hex(IEmulatorFacade& facade, std::istream& hex_stream) {
  // 1. Парсим HEX
  HexParser parser;
  HexImage hex = parser.parse(hex_stream);

  // 2. Загружаем под конкретный AVR (ATmega328P)
  AVRMemoryLayout layout = {32768, 1024, 0, 0};  // flash 32 KiB, eeprom 1 KiB
  AVRProgramLoader loader(layout);
  ProgramImage prog = loader.load(hex);

  // 3. Записываем во flash через фасад
  facade.load_flash_bytes(0, prog.flash.data(), prog.flash.size());

  // EEPROM в MVP не используется, но при необходимости можно аналогично
}