#include "hex_parser.h"

#include "../common/errors.h"

#include <cctype>
#include <cstdio>

namespace {

[[noreturn]] void fail(int line_no, const std::string &what) {
  throw HexFormatError("Invalid HEX, line " + std::to_string(line_no) + ": " +
                       what);
}

int hex_digit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

std::string hex2(unsigned v) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "0x%02X", v & 0xFF);
  return buf;
}

// Убирает пробельные символы (включая '\r') с обоих краёв.
std::string trim(const std::string &s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
    ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return s.substr(b, e - b);
}

// Строка (без ':') -> байты. Проверяет чётность длины и допустимость символов.
std::vector<uint8_t> decode_bytes(const std::string &line, int line_no) {
  const size_t digits = line.size() - 1; // без ':'
  if (digits % 2 != 0)
    fail(line_no, "odd number of hex digits");

  std::vector<uint8_t> bytes;
  bytes.reserve(digits / 2);
  for (size_t i = 1; i < line.size(); i += 2) {
    const int hi = hex_digit(line[i]);
    const int lo = hex_digit(line[i + 1]);
    if (hi < 0)
      fail(line_no, "invalid character '" + std::string(1, line[i]) +
                        "' at column " + std::to_string(i + 1));
    if (lo < 0)
      fail(line_no, "invalid character '" + std::string(1, line[i + 1]) +
                        "' at column " + std::to_string(i + 2));
    bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return bytes;
}

void require_size(const std::vector<uint8_t> &data, size_t n, const char *name,
                  int line_no) {
  if (data.size() != n)
    fail(line_no, std::string(name) + " record must have " + std::to_string(n) +
                      " data bytes, has " + std::to_string(data.size()));
}

} // namespace

HexImage HexParser::parse(std::istream &input) {
  HexImage image;
  uint32_t base = 0; // база адреса из записи 02 или 04
  int line_no = 0;

  std::string raw;
  while (std::getline(input, raw)) {
    ++line_no;
    const std::string line = trim(raw);
    if (line.empty())
      continue;
    if (line[0] != ':')
      fail(line_no, "line must start with ':'");

    // Минимум: счётчик, адрес (2), тип, контрольная сумма = 5 байт.
    const std::vector<uint8_t> bytes = decode_bytes(line, line_no);
    if (bytes.size() < 5)
      fail(line_no, "record is too short");

    const size_t count = bytes[0];
    if (bytes.size() != count + 5)
      fail(line_no, "record declares " + std::to_string(count) +
                        " data bytes, but the line holds " +
                        std::to_string(bytes.size() - 5));

    unsigned sum = 0;
    for (size_t i = 0; i + 1 < bytes.size(); ++i)
      sum += bytes[i];
    const unsigned expected = (0x100u - (sum & 0xFFu)) & 0xFFu;
    if (bytes.back() != expected)
      fail(line_no, "checksum mismatch (file has " + hex2(bytes.back()) +
                        ", expected " + hex2(expected) + ")");

    HexRecord rec;
    rec.type = static_cast<HexRecordType>(bytes[3]);
    rec.data.assign(bytes.begin() + 4, bytes.begin() + 4 + count);
    const uint16_t addr16 = static_cast<uint16_t>((bytes[1] << 8) | bytes[2]);
    rec.address = base + addr16;

    switch (rec.type) {
    case HexRecordType::Data:
      image.records.push_back(std::move(rec));
      break;
    case HexRecordType::EndOfFile:
      require_size(rec.data, 0, "EndOfFile", line_no);
      return image;
    case HexRecordType::ExtendedSegmentAddr:
      require_size(rec.data, 2, "ExtendedSegmentAddr", line_no);
      base = ((static_cast<uint32_t>(rec.data[0]) << 8) | rec.data[1]) << 4;
      break;
    case HexRecordType::ExtendedLinearAddr:
      require_size(rec.data, 2, "ExtendedLinearAddr", line_no);
      base = ((static_cast<uint32_t>(rec.data[0]) << 8) | rec.data[1]) << 16;
      break;
    case HexRecordType::StartSegmentAddr:
      require_size(rec.data, 4, "StartSegmentAddr", line_no);
      break; // для AVR точка входа не нужна
    case HexRecordType::StartLinearAddr:
      require_size(rec.data, 4, "StartLinearAddr", line_no);
      break;
    default:
      fail(line_no, "unknown record type " + hex2(bytes[3]));
    }
  }
  throw HexFormatError("Invalid HEX: no EndOfFile record (:00000001FF)");
}
