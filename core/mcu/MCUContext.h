#pragma once

#include <memory>

class MemoryBus;
class CodeBus;
class AVR_CPU;
class FlashDevice;
class SRAMDevice;
class RegisterFileDevice;
class SREGDevice;
class AVR_GPIO;
class Timer0;
class StackPointerDevice;
class MMIORegister;
class UnimplementedIODevice;

struct MCUContext {
  // ===== buses =====
  std::unique_ptr<MemoryBus> data_bus;
  std::unique_ptr<CodeBus> code_bus;

  // ===== cpu =====
  std::unique_ptr<AVR_CPU> cpu;

  // ===== memory =====
  std::unique_ptr<FlashDevice> flash;
  std::unique_ptr<SRAMDevice> sram;

  // ===== peripherals =====
  std::unique_ptr<RegisterFileDevice> regfile;
  std::unique_ptr<SREGDevice> sreg;
  std::unique_ptr<AVR_GPIO> gpioB;
  std::unique_ptr<AVR_GPIO> gpioD;
  std::unique_ptr<Timer0> timer0;

  //
  std::unique_ptr<StackPointerDevice> sp_device;

  // регистры I/O, которых ещё нет в модели (RAZ/WI)
  std::unique_ptr<UnimplementedIODevice> io_fallback;

  // временная заглушка
  std::unique_ptr<MMIORegister> eearl;
  std::unique_ptr<MMIORegister> eearh;
  std::unique_ptr<MMIORegister> eecr;
};
