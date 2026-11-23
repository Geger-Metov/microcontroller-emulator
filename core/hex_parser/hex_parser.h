#pragma once

#include <cstdint>
#include <istream>
#include <string>
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

class HexParser {
public:
  ~HexParser() = default;
  HexImage parse(std::istream &input);

private:
  bool verifyChecksum(const std::string &line);
  HexRecord parseLine(const std::string &line, uint32_t upper_linear_base);
  uint8_t hexByte(const std::string &str, size_t pos);
};
