// Тесты Intel HEX: парсер и загрузчик во flash.
#include <cstdio>
#include <sstream>
#include <stdexcept>
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

// ---- D-17: устойчивость к плохим файлам и типы записей --------------------

namespace {
// Сообщение об ошибке парсинга (или пустая строка, если ошибки нет).
std::string parse_error(const std::string &text) {
  try {
    parse(text);
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  return "";
}
bool contains(const std::string &s, const char *part) {
  return s.find(part) != std::string::npos;
}
} // namespace

TEST(extended_segment_address_is_applied) {
  // сегмент 0x1000 -> база 0x10000
  HexImage img = parse(rec(0, 2, {0x10, 0x00}) + "\n" + rec(0x0004, 0, {0x77}) +
                       "\n" + EOF_REC + "\n");
  CHECK_EQ(img.records.size(), 1u);
  CHECK_EQ(img.records[0].address, 0x10004u);
}

TEST(segment_and_linear_bases_replace_each_other) {
  HexImage img =
      parse(rec(0, 4, {0x00, 0x02}) + "\n" + rec(0, 2, {0x00, 0x10}) + "\n" +
            rec(0x0001, 0, {0x01}) + "\n" + EOF_REC + "\n");
  CHECK_EQ(img.records[0].address, 0x101u); // 0x0010 << 4 + 1, не 0x20000
}

TEST(trailing_and_leading_whitespace_is_ignored) {
  HexImage img =
      parse("  " + rec(0, 0, {0x42}) + " \t \n\n   \n" + EOF_REC + "  \r\n");
  CHECK_EQ(img.records.size(), 1u);
  CHECK_EQ(img.records[0].data[0], 0x42);
}

TEST(all_errors_are_runtime_error_with_line_number) {
  const std::string ok = rec(0, 0, {1}) + "\n";
  // каждая плохая строка стоит второй в файле
  const std::vector<std::string> bad = {
      "garbage",         // нет ':'
      ":0",              // слишком короткая
      ":0000000",        // нечётное число цифр
      ":00000001FG",     // не hex-символ
      ":00 00 00 01 FF", // пробелы внутри
      ":01000000",       // нет данных и контрольной суммы
      ":0200000000FE",   // заявлено 2 байта данных, их нет
      ":00000099"
      "67",            // неизвестный тип
      ":0100000102FC", // EOF с данными
      ":0200000401"
      "FB", // ExtendedLinear с 1 байтом данных
  };
  for (const std::string &line : bad) {
    std::string msg = parse_error(ok + line + "\n" + EOF_REC + "\n");
    CHECK(!msg.empty());
    CHECK(contains(msg, "line 2"));
  }
}

TEST(declared_length_must_match_line) {
  // заявлено 4 байта данных, в строке 2
  std::string msg = parse_error(":0400000011220000"
                                "\n" +
                                EOF_REC + "\n");
  CHECK(contains(msg, "declares 4"));
}

TEST(checksum_error_names_expected_value) {
  std::string line = rec(0, 0, {0x01, 0x02}); // :0200000001 02 FB
  line.replace(line.size() - 2, 2, "00");
  std::string msg = parse_error(line + "\n" + EOF_REC + "\n");
  CHECK(contains(msg, "checksum"));
  CHECK(contains(msg, "0xFB"));
}

TEST(empty_input_and_missing_eof_throw) {
  CHECK_THROWS(parse(""));
  CHECK_THROWS(parse("\n\n"));
}

TEST(truncated_lines_never_read_out_of_bounds) {
  // Любой префикс корректной строки должен давать исключение, а не UB (ASan).
  const std::string full = rec(0x0100, 0, {1, 2, 3, 4, 5, 6, 7, 8});
  for (size_t n = 1; n < full.size(); ++n)
    CHECK_THROWS(parse(full.substr(0, n) + "\n" + EOF_REC + "\n"));
}

// ---- D-18: области EEPROM / flash и предупреждения ------------------------

TEST(loader_places_eeprom_at_0x810000) {
  HexImage img = parse(rec(0, 4, {0x00, 0x81}) + "\n" +
                       rec(0x0002, 0, {0xAA, 0xBB}) + "\n" + EOF_REC + "\n");
  AVRProgramLoader loader(
      AVRMemoryLayout{32768, 1024, AVR_HEX_FLASH_BASE, AVR_HEX_EEPROM_BASE});
  ProgramImage prog = loader.load(img);
  CHECK_EQ(prog.eeprom[1], 0xFF);
  CHECK_EQ(prog.eeprom[2], 0xAA);
  CHECK_EQ(prog.eeprom[3], 0xBB);
  CHECK_EQ(prog.flash[2], 0xFF); // flash не тронут
  CHECK(prog.warnings.empty());
}

TEST(loader_rejects_eeprom_overflow) {
  HexImage img = parse(rec(0, 4, {0x00, 0x81}) + "\n" + rec(0x03FF, 0, {1, 2}) +
                       "\n" + EOF_REC + "\n");
  AVRProgramLoader loader(
      AVRMemoryLayout{32768, 1024, AVR_HEX_FLASH_BASE, AVR_HEX_EEPROM_BASE});
  CHECK_THROWS(loader.load(img));
}

TEST(loader_warns_about_data_outside_flash_and_eeprom) {
  // fuse-байты по 0x820000
  HexImage img =
      parse(rec(0, 4, {0x00, 0x82}) + "\n" + rec(0, 0, {0xFF, 0xD9, 0x07}) +
            "\n" + rec(0, 4, {0x00, 0x00}) + "\n" + rec(0, 0, {0x11}) + "\n" +
            EOF_REC + "\n");
  AVRProgramLoader loader(
      AVRMemoryLayout{32768, 1024, AVR_HEX_FLASH_BASE, AVR_HEX_EEPROM_BASE});
  ProgramImage prog = loader.load(img);
  CHECK_EQ(prog.warnings.size(), 1u);
  CHECK(contains(prog.warnings[0], "0x820000"));
  CHECK_EQ(prog.flash[0], 0x11); // обычные данные загрузились
}

TEST(loader_data_in_flash_is_not_confused_with_eeprom) {
  // раньше eepromBase == flashBase == 0, и ветка EEPROM была недостижима
  HexImage img = parse(rec(0, 0, {0x5A}) + "\n" + EOF_REC + "\n");
  AVRProgramLoader loader(
      AVRMemoryLayout{32768, 1024, AVR_HEX_FLASH_BASE, AVR_HEX_EEPROM_BASE});
  ProgramImage prog = loader.load(img);
  CHECK_EQ(prog.flash[0], 0x5A);
  CHECK_EQ(prog.eeprom[0], 0xFF);
}
