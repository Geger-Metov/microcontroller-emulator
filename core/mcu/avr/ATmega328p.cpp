#include "ATmega328p.h"
#include "AVR_CPU.h"
#include "AVR_GPIO.h"
#include "BasicMemoryDevices.h"
#include "CodeBus.h"
#include "EmulatorFacade.h"
#include "MCUContext.h"
#include "MMIORegister.h"
#include "MemoryBus.h"
#include "RegisterFileDevice.h"
#include "SREGDevice.h"
#include "StackPointerDevice.h"
#include "Timer0.h"
#include "UnimplementedIODevice.h"
#include "avr_decoder.h"
#include "avr_loader.h"
#include "hex_parser.h"

std::unique_ptr<IEmulatorFacade> ATmega328P::create() {
  auto ctx = build_context();
  build_memory_map(*ctx);
  connect_cpu(*ctx);
  // connect_external_pins(*ctx);

  auto facade = std::make_unique<EmulatorFacade>();

  // Сырые указатели берём ДО передачи владения: после std::move() unique_ptr
  // в контексте пуст, и регистрация для reset молча пропускалась (баг D-04).
  RegisterFileDevice *regfile = ctx->regfile.get();
  SREGDevice *sreg = ctx->sreg.get();
  SRAMDevice *sram = ctx->sram.get();
  AVR_GPIO *gpioB = ctx->gpioB.get();
  AVR_GPIO *gpioD = ctx->gpioD.get();
  Timer0 *timer0 = ctx->timer0.get();

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
  facade->add_memory_device(std::move(ctx->sp_device));
  facade->add_memory_device(std::move(ctx->io_fallback));

  // Карта пространства данных (для отладчика/GUI); границы включительно.
  facade->add_memory_region("Registers", 0x0000, 0x001F, true, true);
  facade->add_memory_region("I/O", 0x0020, 0x00FF, true, true);
  facade->add_memory_region("SRAM", 0x0100, 0x08FF, true, true);

  // временная заглушка
  facade->add_memory_device(std::move(ctx->eearl));
  facade->add_memory_device(std::move(ctx->eearh));
  facade->add_memory_device(std::move(ctx->eecr));

  // Timer0 не отображён в память целиком (регистры маппятся по отдельности),
  // поэтому владеет им фасад через список «владеемых» устройств.
  facade->add_owned_device(std::move(ctx->timer0));

  // reset: power-on состояние для всех устройств с состоянием
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
  // Время таймеру сообщает шина (его регистры в карте лежат по отдельности)
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

  // временная заглушка
  // Регистры I/O, которых ещё нет в модели: RAZ/WI вместо исключения
  ctx->io_fallback = std::make_unique<UnimplementedIODevice>(0x100 - 0x20);

  ctx->eearl = std::make_unique<MMIORegister>();
  ctx->eearh = std::make_unique<MMIORegister>();
  ctx->eecr = std::make_unique<MMIORegister>();

  return ctx;
}

void ATmega328P::build_memory_map(MCUContext &ctx) {
  // Адреса — в пространстве данных (I/O-адрес + 0x20).
  ctx.data_bus->map(0x0000, 0x001F, ctx.regfile.get());
  ctx.data_bus->map(0x0023, 0x0025, ctx.gpioB.get());
  ctx.data_bus->map(0x0029, 0x002B, ctx.gpioD.get());
  ctx.data_bus->map(0x005F, 0x005F, ctx.sreg.get());
  ctx.data_bus->map(0x0100, 0x08FF, ctx.sram.get());

  // Timer0 (I/O-адреса: TIFR0 0x15, TCCR0A/B 0x24/0x25, TCNT0 0x26,
  // OCR0A/B 0x27/0x28; TIMSK0 — расширенный I/O 0x6E)
  ctx.data_bus->map(0x0035, 0x0035, &ctx.timer0->TIFR0);
  ctx.data_bus->map(0x0044, 0x0044, &ctx.timer0->TCCR0A);
  ctx.data_bus->map(0x0045, 0x0045, &ctx.timer0->TCCR0B);
  ctx.data_bus->map(0x0046, 0x0046, &ctx.timer0->TCNT0);
  ctx.data_bus->map(0x0047, 0x0047, &ctx.timer0->OCR0A);
  ctx.data_bus->map(0x0048, 0x0048, &ctx.timer0->OCR0B);
  ctx.data_bus->map(0x006E, 0x006E, &ctx.timer0->TIMSK0);

  ctx.data_bus->map(0x005D, 0x005E, ctx.sp_device.get()); // SPL, SPH

  // временная заглушка
  ctx.data_bus->map(0x0041, 0x0041, ctx.eearl.get());
  ctx.data_bus->map(0x0042, 0x0042, ctx.eearh.get());
  ctx.data_bus->map(0x003F, 0x003F, ctx.eecr.get());

  // Всё остальное в области I/O (0x20–0xFF): RAZ/WI, а не исключение.
  ctx.data_bus->set_fallback(0x0020, 0x00FF, ctx.io_fallback.get());

  ctx.code_bus->map(0x0000, 0x3FFF, ctx.flash.get());
}

std::vector<std::string> ATmega328P::load_hex(IEmulatorFacade &facade,
                                              std::istream &hex_stream) {
  // 1. Парсим HEX
  HexParser parser;
  HexImage hex = parser.parse(hex_stream);

  // 2. Раскладываем под ATmega328P: flash 32 КиБ, EEPROM 1 КиБ
  AVRMemoryLayout layout = {32768, 1024, AVR_HEX_FLASH_BASE,
                            AVR_HEX_EEPROM_BASE};
  AVRProgramLoader loader(layout);
  ProgramImage prog = loader.load(hex);

  // 3. Записываем во flash через фасад
  facade.load_flash_bytes(0, prog.flash.data(), prog.flash.size());

  // EEPROM пока не эмулируется — сообщаем, если в файле были её данные
  for (uint8_t b : prog.eeprom) {
    if (b != 0xFF) {
      prog.warnings.push_back(
          "HEX contains EEPROM data, but EEPROM is not emulated");
      break;
    }
  }
  return prog.warnings;
}
