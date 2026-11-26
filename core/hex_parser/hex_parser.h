#pragma once

#include <cstdint>
#include <istream>
#include <vector>

/**
 * Intel HEX record type
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
 * A single record of a HEX file (after parsing a line)
 */
struct HexRecord {
  HexRecordType type;
  uint32_t address; // already accounting for the extended address
  std::vector<uint8_t> data;
};

/**
 * The result of parsing a HEX file
 */
struct HexImage {
  std::vector<HexRecord> records;
};

/**
 * An Intel HEX parser.
 *
 * Any format error is a HexFormatError with the line number and the reason.
 * Spaces/tabs at the ends of a line, CRLF, empty lines and lowercase
 * hex digits are allowed. Record types 00–05 are supported; addresses from
 * records 02 (segment * 16) and 04 (linear, the upper 16 bits) are accounted
 * for in HexRecord::address. Parsing stops at the first EndOfFile record (the
 * rest of the file is ignored).
 */
class HexParser {
public:
  ~HexParser() = default;
  HexImage parse(std::istream &input);
};
