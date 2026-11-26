#include "mcu/avr/ATmega328p.h"
#include "bus/BasicMemoryDevices.h"
#include "bus/CodeBus.h"
#include "bus/MemoryBus.h"
#include "cpu/AVR_CPU.h"
#include "decoder/avr_decoder.h"
#include "facade/EmulatorFacade.h"
#include "hex_parser/hex_parser.h"
#include "loader/avr_loader.h"
#include "mcu/MCUContext.h"
#include "mcu/avr/AVR_GPIO.h"
#include "mcu/peripherals/MMIORegister.h"
#include "mcu/peripherals/RegisterFileDevice.h"
#include "mcu/peripherals/SREGDevice.h"
#include "mcu/peripherals/StackPointerDevice.h"
#include "mcu/peripherals/Timer0.h"
#include "mcu/peripherals/UnimplementedIODevice.h"

std::unique_ptr<IEmulatorFacade> ATmega328P::create() {
  auto ctx = build_context();
  build_memory_map(*ctx);
  connect_cpu(*ctx);
  // connect_external_pins(*ctx);

  auto facade = std::make_unique<EmulatorFacade>();

  // Take the raw pointers BEFORE ownership is transferred: after std::move()
  // the unique_ptr in the context is empty, and registration for reset used to
  // be silently skipped.
  RegisterFileDevice *regfile = ctx->regfile.get();
  SREGDevice *sreg = ctx->sreg.get();
  SRAMDevice *sram = ctx->sram.get();
  AVR_GPIO *gpioB = ctx->gpioB.get();
  AVR_GPIO *gpioD = ctx->gpioD.get();
  Timer0 *timer0 = ctx->timer0.get();

  // === transfer ownership ===
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
  facade->add_memory_device(std::move(ctx->sp_device));
  facade->add_memory_device(std::move(ctx->io_fallback));

  // Ports whose pins the outside world (the GUI) can drive.
  facade->add_pin_port('B', gpioB);
  facade->add_pin_port('D', gpioD);

  // Data-space map (for the debugger/GUI); bounds are inclusive.
  facade->add_memory_region("Registers", 0x0000, 0x001F, true, true);
  facade->add_memory_region("I/O", 0x0020, 0x00FF, true, true);
  facade->add_memory_region("SRAM", 0x0100, 0x08FF, true, true);

  // temporary stub
  facade->add_memory_device(std::move(ctx->eearl));
  facade->add_memory_device(std::move(ctx->eearh));
  facade->add_memory_device(std::move(ctx->eecr));

  // Timer0 is not mapped into memory as a whole (its registers are mapped
  // separately), so the facade owns it through the list of "owned" devices.
  facade->add_owned_device(std::move(ctx->timer0));

  // reset: the power-on state for all devices that have state
  facade->register_reset_device(regfile);
  facade->register_reset_device(sreg);
  facade->register_reset_device(sram);
  facade->register_reset_device(gpioB);
  facade->register_reset_device(gpioD);
  facade->register_reset_device(timer0);

  return facade;
}

void ATmega328P::connect_cpu(MCUContext &ctx) {
  ctx.cpu->attach_data_bus(ctx.data_bus.get());
  ctx.cpu->attach_code_bus(ctx.code_bus.get());
  ctx.cpu->set_register_file(ctx.regfile.get());
  ctx.cpu->set_sreg_device(ctx.sreg.get());
  ctx.timer0->attach_interrupt_sink(ctx.cpu.get());
  // The bus delivers time to the timer (its registers are mapped separately)
  ctx.data_bus->add_tickable(ctx.timer0.get());

  ctx.cpu->reset();
}

std::unique_ptr<MCUContext> ATmega328P::build_context() {
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
  auto decoder = std::make_unique<AVRDecoder>(
      avr::avre_plus_instruction_table, avr::avre_plus_instruction_table_size);
  ctx->cpu = std::make_unique<AVR_CPU>(std::move(decoder));
  ctx->cpu->set_initial_sp(0x08FF); // RAMEND

  //
  ctx->sp_device = std::make_unique<StackPointerDevice>(ctx->cpu.get());

  // temporary stub
  // I/O registers that are not in the model yet: RAZ/WI instead of an exception
  ctx->io_fallback = std::make_unique<UnimplementedIODevice>(0x100 - 0x20);

  ctx->eearl = std::make_unique<MMIORegister>();
  ctx->eearh = std::make_unique<MMIORegister>();
  ctx->eecr = std::make_unique<MMIORegister>();

  return ctx;
}

void ATmega328P::build_memory_map(MCUContext &ctx) {
  // Addresses are in the data space (I/O address + 0x20).
  ctx.data_bus->map(0x0000, 0x001F, ctx.regfile.get());
  ctx.data_bus->map(0x0023, 0x0025, ctx.gpioB.get());
  ctx.data_bus->map(0x0029, 0x002B, ctx.gpioD.get());
  ctx.data_bus->map(0x005F, 0x005F, ctx.sreg.get());
  ctx.data_bus->map(0x0100, 0x08FF, ctx.sram.get());

  // Timer0 (I/O addresses: TIFR0 0x15, TCCR0A/B 0x24/0x25, TCNT0 0x26,
  // OCR0A/B 0x27/0x28; TIMSK0 — extended I/O 0x6E)
  ctx.data_bus->map(0x0035, 0x0035, &ctx.timer0->TIFR0);
  ctx.data_bus->map(0x0044, 0x0044, &ctx.timer0->TCCR0A);
  ctx.data_bus->map(0x0045, 0x0045, &ctx.timer0->TCCR0B);
  ctx.data_bus->map(0x0046, 0x0046, &ctx.timer0->TCNT0);
  ctx.data_bus->map(0x0047, 0x0047, &ctx.timer0->OCR0A);
  ctx.data_bus->map(0x0048, 0x0048, &ctx.timer0->OCR0B);
  ctx.data_bus->map(0x006E, 0x006E, &ctx.timer0->TIMSK0);

  ctx.data_bus->map(0x005D, 0x005E, ctx.sp_device.get()); // SPL, SPH

  // temporary stub
  ctx.data_bus->map(0x0041, 0x0041, ctx.eearl.get());
  ctx.data_bus->map(0x0042, 0x0042, ctx.eearh.get());
  ctx.data_bus->map(0x003F, 0x003F, ctx.eecr.get());

  // Everything else in the I/O area (0x20–0xFF): RAZ/WI, not an exception.
  ctx.data_bus->set_fallback(0x0020, 0x00FF, ctx.io_fallback.get());

  ctx.code_bus->map(0x0000, 0x3FFF, ctx.flash.get());
}

std::vector<std::string> ATmega328P::load_hex(IEmulatorFacade &facade,
                                              std::istream &hex_stream) {
  // 1. Parse the HEX
  HexParser parser;
  HexImage hex = parser.parse(hex_stream);

  // 2. Lay it out for the ATmega328P: 32 KiB flash, 1 KiB EEPROM
  AVRMemoryLayout layout = {32768, 1024, AVR_HEX_FLASH_BASE,
                            AVR_HEX_EEPROM_BASE};
  AVRProgramLoader loader(layout);
  ProgramImage prog = loader.load(hex);

  // 3. Write to flash through the facade
  facade.load_flash_bytes(0, prog.flash.data(), prog.flash.size());

  // EEPROM is not emulated yet — warn if the file had EEPROM data
  for (uint8_t b : prog.eeprom) {
    if (b != 0xFF) {
      prog.warnings.push_back(
          "HEX contains EEPROM data, but EEPROM is not emulated");
      break;
    }
  }
  return prog.warnings;
}
