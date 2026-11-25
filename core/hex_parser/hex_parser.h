#pragma once

#include <cstdint>
#include <istream>
#include <vector>

/**
 * Тип записи Intel HEX
 */
enum class HexRecordType : uint8_t {
  Data = 0x00,
  EndOfFile = 0x01,
  ExtendedSegmentAddr = 0x02,
  StartSegmentAddr = 0x03,
  ExtendedLinearAddr = 0x04,
  StartLinearAddr = 0x05
};

/**
 * Одна запись HEX файла (после парсинга строки)
 */
struct HexRecord {
  HexRecordType type;
  uint32_t address; // уже с учётом extended address
  std::vector<uint8_t> data;
};

/**
 * Результат парсинга HEX
 */
struct HexImage {
  std::vector<HexRecord> records;
};

/**
 * Парсер Intel HEX.
 *
 * Любая ошибка формата — std::runtime_error с номером строки и причиной.
 * Допустимы пробелы/табы по краям строки, CRLF, пустые строки, строчные
 * hex-цифры. Поддерживаются записи 00–05; адреса из записей 02 (сегмент * 16) и
 * 04 (линейный, старшие 16 бит) учитываются в HexRecord::address.
 * Разбор останавливается на первой записи EndOfFile (остаток файла
 * игнорируется).
 */
class HexParser {
public:
  ~HexParser() = default;
  HexImage parse(std::istream &input);
};
