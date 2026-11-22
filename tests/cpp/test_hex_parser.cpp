// Тесты Intel HEX: парсер и загрузчик во flash.
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "avr_loader.h"
#include "hex_parser.h"
#include "test_framework.h"

namespace {

// Собирает строку Intel HEX с корректной контрольной суммой.
std::string rec(uint16_t addr, uint8_t type, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> b = {static_cast<uint8_t>(data.size()),
                            static_cast<uint8_t>(addr >> 8),
                            static_cast<uint8_t>(addr & 0xFF), type};
  b.insert(b.end(), data.begin(), data.end());
  unsigned sum = 0;
  for (uint8_t v : b)
    sum += v;
  b.push_back(static_cast<uint8_t>(-static_cast<int>(sum)));

  std::string s = ":";
  char buf[3];
  for (uint8_t v : b) {
    std::snprintf(buf, sizeof buf, "%02X", v);
    s += buf;
  }
  return s;
}

HexImage parse(const std::string &text) {
  std::istringstream in(text);
  HexParser p;
  return p.parse(in);
}

const std::string EOF_REC = ":00000001FF";

} // namespace

TEST(parses_data_record) {
  HexImage img =
      parse(rec(0x0010, 0, {0x11, 0x22, 0x33}) + "\n" + EOF_REC + "\n");
  CHECK(!img.records.empty());
  CHECK_EQ(img.records[0].address, 0x0010u);
  CHECK_EQ(img.records[0].data.size(), 3u);
  CHECK_EQ(img.records[0].data[2], 0x33);
}

TEST(accepts_crlf_and_lowercase) {
  std::string text = rec(0, 0, {0xAB, 0xCD}) + "\r\n" + EOF_REC + "\r\n";
  for (char &c : text)
    if (c >= 'A' && c <= 'F')
      c = static_cast<char>(c - 'A' + 'a');
  HexImage img = parse(text);
  CHECK_EQ(img.records[0].data[0], 0xAB);
  CHECK_EQ(img.records[0].data[1], 0xCD);
}

TEST(extended_linear_address_is_applied) {
  HexImage img = parse(rec(0, 4, {0x00, 0x01}) + "\n" + rec(0x0002, 0, {0x55}) +
                       "\n" + EOF_REC + "\n");
  bool found = false;
  for (const HexRecord &r : img.records)
    if (r.type == HexRecordType::Data) {
      CHECK_EQ(r.address, 0x00010002u);
      found = true;
    }
  CHECK(found);
}

TEST(bad_checksum_throws) {
  std::string line = rec(0, 0, {0x01, 0x02});
  line.back() = line.back() == '0' ? '1' : '0';
  CHECK_THROWS(parse(line + "\n" + EOF_REC + "\n"));
}

TEST(missing_eof_record_throws) {
  CHECK_THROWS(parse(rec(0, 0, {0x01}) + "\n"));
}

TEST(loader_places_bytes_in_flash) {
  HexImage img =
      parse(rec(0x0004, 0, {0xDE, 0xAD, 0xBE, 0xEF}) + "\n" + EOF_REC + "\n");
  AVRProgramLoader loader(AVRMemoryLayout{32768, 1024, 0, 0});
  ProgramImage prog = loader.load(img);
  CHECK_EQ(prog.flash.size(), 32768u);
  CHECK_EQ(prog.flash[3], 0xFF); // нетронутый flash = 0xFF
  CHECK_EQ(prog.flash[4], 0xDE);
  CHECK_EQ(prog.flash[7], 0xEF);
  CHECK_EQ(prog.flash[8], 0xFF);
}

TEST(loader_rejects_overflowing_record) {
  HexImage img = parse(rec(0x7FFE, 0, {1, 2, 3, 4}) + "\n" + EOF_REC + "\n");
  AVRProgramLoader loader(AVRMemoryLayout{32768, 1024, 0, 0});
  CHECK_THROWS(loader.load(img));
}
