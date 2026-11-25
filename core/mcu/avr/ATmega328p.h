#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "interfaces/Interfaces.h"

// Forward declaration
class MCUContext;
class EmulatorFacade;

// Сборка конкретного микроконтроллера ATmega328P
class ATmega328P {
public:
  // Создать полностью сконфигурированный эмулятор
  static std::unique_ptr<IEmulatorFacade> create();

  // Загрузить Intel HEX во flash. Ошибка формата/переполнения — исключение.
  // Возвращает предупреждения (данные вне flash, EEPROM не эмулируется).
  static std::vector<std::string> load_hex(IEmulatorFacade &facade,
                                           std::istream &hex_stream);

private:
  // Вспомогательные методы для создания устройств
  static std::unique_ptr<MCUContext> build_context();
  static void build_memory_map(MCUContext &ctx);
  static void connect_cpu(MCUContext &ctx);
  static void connect_external_pins(MCUContext &ctx);
};

/*
3. Альтернативный, более масштабируемый подход (для будущего)
Если вы планируете поддерживать много разных AVR-микроконтроллеров или даже
других архитектур, статический метод в каждом классе-сборщике приведёт к
дублированию. Можно вынести логику загрузки в отдельный сервис, параметризуемый
раскладкой памяти:

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
Тогда в ATmega328P останется только вызов сервиса с правильным layout. Это
полностью убирает дублирование и оставляет классу-сборщику лишь конфигурацию.
*/
