#include "hex_parser.h"

#include <stdexcept>

HexImage HexParser::parse(std::istream &input) {
  HexImage image;
  uint32_t upper_linear_base = 0; // старшие 16 бит базового адреса
  int line_no = 0;

  std::string line;
  while (std::getline(input, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r')
      line.pop_back(); // Windows CR

    if (line.empty())
      continue;
    if (line[0] != ':')
      throw std::runtime_error("Invalid HEX line " + std::to_string(line_no) +
                               ": missing ':'");

    if (!verifyChecksum(line))
      throw std::runtime_error("Checksum error at line " +
                               std::to_string(line_no));

    HexRecord rec = parseLine(line, upper_linear_base);

    switch (rec.type) {
    case HexRecordType::Data:
      image.records.push_back(std::move(rec));
      break;
    case HexRecordType::EndOfFile:
      return image; // завершаем, даже если дальше есть строки
    case HexRecordType::ExtendedLinearAddr:
      if (rec.data.size() < 2)
        throw std::runtime_error("Invalid ExtendedLinearAddr at line " +
                                 std::to_string(line_no));
      upper_linear_base =
          (static_cast<uint32_t>(rec.data[0]) << 8) | rec.data[1];
      break;
    case HexRecordType::StartLinearAddr:
      // Для AVR точка входа не используется, просто игнорируем
      // (можно сохранить в будущем, если потребуется)
      break;
    default:
      // ExtendedSegmentAddr и StartSegmentAddr (x86 real mode) игнорируем
      break;
    }
  }
  throw std::runtime_error("Unexpected end of file: no EndOfFile record");
}

bool HexParser::verifyChecksum(const std::string &line) {
  if (line.size() < 11)
    return false; // минимальная длина ":BBAAAATTCC"
  uint8_t sum = 0;
  for (size_t i = 1; i < line.size(); i += 2) {
    sum += hexByte(line, i);
  }
  return sum == 0;
}

HexRecord HexParser::parseLine(const std::string &line,
                               uint32_t upper_linear_base) {
  HexRecord rec;
  size_t idx = 1; // после ':'

  uint8_t byte_cnt = hexByte(line, idx);
  idx += 2;
  uint16_t addr_low =
      (static_cast<uint16_t>(hexByte(line, idx)) << 8) | hexByte(line, idx + 2);
  idx += 4;
  uint8_t rec_type = hexByte(line, idx);
  idx += 2;
  rec.type = static_cast<HexRecordType>(rec_type);
  rec.data.resize(byte_cnt);
  for (uint8_t i = 0; i < byte_cnt; ++i) {
    rec.data[i] = hexByte(line, idx);
    idx += 2;
  }
  // checksum уже проверен, не сохраняем

  // Вычисляем полный адрес
  rec.address = (upper_linear_base << 16) | addr_low;

  return rec;
}

uint8_t HexParser::hexByte(const std::string &str, size_t pos) {
  auto hexChar = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    throw std::invalid_argument("Invalid hex character");
  };
  return (hexChar(str[pos]) << 4) | hexChar(str[pos + 1]);
}
