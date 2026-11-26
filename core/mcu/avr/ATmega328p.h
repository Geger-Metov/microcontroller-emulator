#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "interfaces/Interfaces.h"

// Forward declaration
class MCUContext;
class EmulatorFacade;

// Assembly of a concrete ATmega328P microcontroller
class ATmega328P {
public:
  // Create a fully configured emulator
  static std::unique_ptr<IEmulatorFacade> create();

  // Load Intel HEX into flash. A format/overflow error is an exception.
  // Returns warnings (data outside flash, EEPROM is not emulated).
  static std::vector<std::string> load_hex(IEmulatorFacade &facade,
                                           std::istream &hex_stream);

private:
  // Helper methods for creating devices
  static std::unique_ptr<MCUContext> build_context();
  static void build_memory_map(MCUContext &ctx);
  static void connect_cpu(MCUContext &ctx);
  static void connect_external_pins(MCUContext &ctx);
};

/*
Design note: a more scalable approach (for the future)
If you plan to support many different AVR microcontrollers or even
other architectures, a static method in every assembly class leads to
duplication. The loading logic can be moved into a separate service
parameterized by the memory layout:

cpp
class HexLoaderService {
public:
    static void load(std::istream& hex_stream,
                     EmulatorFacade& facade,
                     const AVRMemoryLayout& layout) {
        HexParser parser;
        auto hex = parser.parse(hex_stream);
        AVRProgramLoader loader(layout);
        auto prog = loader.load(hex);
        facade.load_flash_bytes(0, prog.flash.data(), prog.flash.size());
    }
};
Then ATmega328P only has to call the service with the right layout. This
removes the duplication entirely and leaves the assembly class with
configuration only.
*/
