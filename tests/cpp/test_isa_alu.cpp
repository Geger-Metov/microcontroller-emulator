// Исчерпывающие проверки АЛУ: результат и ВЕСЬ регистр SREG сверяются с
// независимым оракулом на широкой арифметике (int), а не с теми же битовыми
// формулами, что в реализации.
#include <string>

#include "test_framework.h"
#include "test_util.h"

using namespace testutil;

namespace {

constexpr int C = 0, Z = 1, N = 2, V = 3, S = 4, H = 5, T = 6, I = 7;

struct Result {
  uint8_t value = 0;
  uint8_t sreg = 0;
};

struct Fx {
  std::unique_ptr<IEmulatorFacade> emu;
  uint16_t opcode;

  explicit Fx(uint16_t op) : emu(make_emu({{0, {op, rjmp(-2)}}})), opcode(op) {}

  void set_op(uint16_t op) {
    emu->program_flash(0, {op, rjmp(-2)});
    opcode = op;
  }

  // Выполняет одну инструкцию с заданными r16, r17 и SREG. Возвращает r16,
  // SREG.
  Result run(uint8_t a, uint8_t b, uint8_t sreg_in) {
    emu->write_data(16, a, 1);
    emu->write_data(17, b, 1);
    emu->write_data(SREG, sreg_in, 1);
    emu->step_instruction(); // сама инструкция
    Result r;
    r.value = static_cast<uint8_t>(emu->read_data(16, 1));
    r.sreg = static_cast<uint8_t>(emu->read_data(SREG, 1));
    emu->step_instruction(); // rjmp .-2 — вернуться на слово 0
    return r;
  }
};

int bit(int v, int n) { return (v >> n) & 1; }
int s8(int v) { return static_cast<int8_t>(v); }

// Меняет в sreg только биты из mask на значения из flags.
uint8_t merge(uint8_t sreg_in, uint8_t mask, uint8_t flags) {
  return static_cast<uint8_t>((sreg_in & ~mask) | (flags & mask));
}

uint8_t pack(int c, int z, int n, int v, int h) {
  int s = n ^ v;
  return static_cast<uint8_t>((c << C) | (z << Z) | (n << N) | (v << V) |
                              (s << S) | (h << H));
}

constexpr uint8_t ALL_ARITH =
    (1 << C) | (1 << Z) | (1 << N) | (1 << V) | (1 << S) | (1 << H);
constexpr uint8_t NZVS = (1 << Z) | (1 << N) | (1 << V) | (1 << S);

// Оракул сложения/вычитания. sub=false: a + b + cin, sub=true: a - b - cin.
// sticky_z: Z = (res==0) && старый Z.
struct Expect {
  uint8_t value;
  uint8_t sreg;
};

Expect oracle_addsub(bool sub, int a, int b, int cin, uint8_t sreg_in,
                     bool sticky_z, bool write_back = true) {
  int sa = s8(a), sb = s8(b);
  int wide = sub ? a - b - cin : a + b + cin;
  int swide = sub ? sa - sb - cin : sa + sb + cin;
  int nib = sub ? (a & 15) - (b & 15) - cin : (a & 15) + (b & 15) + cin;
  int res = wide & 0xFF;

  int c = sub ? (wide < 0) : (wide > 255);
  int h = sub ? (nib < 0) : (nib > 15);
  int v = (swide < -128 || swide > 127);
  int n = bit(res, 7);
  int z = (res == 0) && (!sticky_z || bit(sreg_in, Z));

  Expect e;
  e.value = static_cast<uint8_t>(write_back ? res : a);
  e.sreg = merge(sreg_in, ALL_ARITH, pack(c, z, n, v, h));
  return e;
}

std::string hex(int v) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "0x%02X", v & 0xFF);
  return buf;
}

void fail(const char *what, int a, int b, int sreg_in, int got_v, int exp_v,
          int got_s, int exp_s) {
  throw mctest::Failure{std::string(what) + ": a=" + hex(a) + " b=" + hex(b) +
                        " sreg_in=" + hex(sreg_in) + " value got " +
                        hex(got_v) + " exp " + hex(exp_v) + ", sreg got " +
                        hex(got_s) + " exp " + hex(exp_s)};
}

void expect_eq(const char *what, int a, int b, uint8_t sreg_in,
               const Result &got, const Expect &exp) {
  if (got.value != exp.value || got.sreg != exp.sreg)
    fail(what, a, b, sreg_in, got.value, exp.value, got.sreg, exp.sreg);
}

// Фоновый шаблон для «чужих» флагов: чтобы поймать лишние/потерянные записи.
uint8_t background(int a, int b) {
  return static_cast<uint8_t>(((a * 7 + b * 13) & 1) ? 0xFF : 0x00);
}

// Перебор a, b и входных C/Z. Для SREG берём фон, подменяя C и Z.
template <typename OracleFn>
void sweep_two_regs(const char *name, uint16_t opcode, bool use_cin,
                    bool use_z_in, OracleFn oracle) {
  Fx fx(opcode);
  for (int a = 0; a < 256; ++a) {
    for (int b = 0; b < 256; ++b) {
      for (int cin = 0; cin < (use_cin ? 2 : 1); ++cin) {
        for (int zin = 0; zin < (use_z_in ? 2 : 1); ++zin) {
          uint8_t in = background(a, b);
          in = merge(in, (1 << C) | (1 << Z),
                     static_cast<uint8_t>((cin << C) | (zin << Z)));
          Result got =
              fx.run(static_cast<uint8_t>(a), static_cast<uint8_t>(b), in);
          expect_eq(name, a, b, in, got, oracle(a, b, cin, in));
        }
      }
    }
  }
}

} // namespace

TEST(alu_add_matches_oracle_exhaustively) {
  sweep_two_regs("ADD", rd_rr(0x0C00, 16, 17), false, false,
                 [](int a, int b, int, uint8_t in) {
                   return oracle_addsub(false, a, b, 0, in, false);
                 });
}

TEST(alu_adc_matches_oracle_exhaustively) {
  // Z у ADC НЕ липкий — именно здесь раньше была ошибка.
  sweep_two_regs("ADC", rd_rr(0x1C00, 16, 17), true, true,
                 [](int a, int b, int cin, uint8_t in) {
                   return oracle_addsub(false, a, b, cin, in, false);
                 });
}

TEST(alu_sub_matches_oracle_exhaustively) {
  sweep_two_regs("SUB", rd_rr(0x1800, 16, 17), false, false,
                 [](int a, int b, int, uint8_t in) {
                   return oracle_addsub(true, a, b, 0, in, false);
                 });
}

TEST(alu_sbc_matches_oracle_exhaustively) {
  sweep_two_regs("SBC", rd_rr(0x0800, 16, 17), true, true,
                 [](int a, int b, int cin, uint8_t in) {
                   return oracle_addsub(true, a, b, cin, in, true);
                 });
}

TEST(alu_cp_matches_oracle_exhaustively) {
  sweep_two_regs("CP", rd_rr(0x1400, 16, 17), false, false,
                 [](int a, int b, int, uint8_t in) {
                   return oracle_addsub(true, a, b, 0, in, false, false);
                 });
}

TEST(alu_cpc_matches_oracle_exhaustively) {
  sweep_two_regs("CPC", rd_rr(0x0400, 16, 17), true, true,
                 [](int a, int b, int cin, uint8_t in) {
                   return oracle_addsub(true, a, b, cin, in, true, false);
                 });
}

namespace {
// Формы с константой: b — это K, кодируется в опкод.
template <typename OracleFn>
void sweep_imm(const char *name, uint16_t base, bool use_cin, OracleFn oracle) {
  Fx fx(rd_k(base, 16, 0));
  for (int k = 0; k < 256; ++k) {
    fx.set_op(rd_k(base, 16, k));
    for (int a = 0; a < 256; ++a) {
      for (int cin = 0; cin < (use_cin ? 2 : 1); ++cin) {
        for (int zin = 0; zin < (use_cin ? 2 : 1); ++zin) {
          uint8_t in = merge(background(a, k), (1 << C) | (1 << Z),
                             static_cast<uint8_t>((cin << C) | (zin << Z)));
          Result got = fx.run(static_cast<uint8_t>(a), 0, in);
          expect_eq(name, a, k, in, got, oracle(a, k, cin, in));
        }
      }
    }
  }
}
} // namespace

TEST(alu_subi_cpi_sbci_match_oracle_exhaustively) {
  sweep_imm("SUBI", 0x5000, false, [](int a, int k, int, uint8_t in) {
    return oracle_addsub(true, a, k, 0, in, false);
  });
  sweep_imm("CPI", 0x3000, false, [](int a, int k, int, uint8_t in) {
    return oracle_addsub(true, a, k, 0, in, false, false);
  });
  sweep_imm("SBCI", 0x4000, true, [](int a, int k, int cin, uint8_t in) {
    return oracle_addsub(true, a, k, cin, in, true);
  });
}

namespace {
Expect oracle_logic(int res, uint8_t in) {
  int n = bit(res, 7);
  Expect e;
  e.value = static_cast<uint8_t>(res);
  e.sreg = merge(in, NZVS, pack(0, res == 0, n, 0, 0));
  return e;
}
} // namespace

TEST(alu_logic_ops_match_oracle_exhaustively) {
  sweep_two_regs(
      "AND", rd_rr(0x2000, 16, 17), false, false,
      [](int a, int b, int, uint8_t in) { return oracle_logic(a & b, in); });
  sweep_two_regs(
      "OR", rd_rr(0x2800, 16, 17), false, false,
      [](int a, int b, int, uint8_t in) { return oracle_logic(a | b, in); });
  sweep_two_regs(
      "EOR", rd_rr(0x2400, 16, 17), false, false,
      [](int a, int b, int, uint8_t in) { return oracle_logic(a ^ b, in); });
  sweep_imm("ANDI", 0x7000, false, [](int a, int k, int, uint8_t in) {
    return oracle_logic(a & k, in);
  });
  sweep_imm("ORI", 0x6000, false, [](int a, int k, int, uint8_t in) {
    return oracle_logic(a | k, in);
  });
}

namespace {
template <typename OracleFn>
void sweep_unary(const char *name, uint16_t base, OracleFn oracle) {
  Fx fx(unary(base, 16));
  for (int a = 0; a < 256; ++a) {
    for (int cin = 0; cin < 2; ++cin) {
      uint8_t in = merge(background(a, 0), 1 << C, static_cast<uint8_t>(cin));
      Result got = fx.run(static_cast<uint8_t>(a), 0, in);
      expect_eq(name, a, 0, in, got, oracle(a, cin, in));
    }
  }
}
} // namespace

TEST(alu_unary_ops_match_oracle_exhaustively) {
  sweep_unary("INC", 0x9403, [](int a, int, uint8_t in) {
    int res = (a + 1) & 0xFF;
    int v = (a == 127);
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg = merge(in, NZVS, pack(0, res == 0, bit(res, 7), v, 0));
    return e;
  });
  sweep_unary("DEC", 0x940A, [](int a, int, uint8_t in) {
    int res = (a - 1) & 0xFF;
    int v = (a == 128);
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg = merge(in, NZVS, pack(0, res == 0, bit(res, 7), v, 0));
    return e;
  });
  sweep_unary("COM", 0x9400, [](int a, int, uint8_t in) {
    int res = (~a) & 0xFF;
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg = merge(in, NZVS | (1 << C), pack(1, res == 0, bit(res, 7), 0, 0));
    return e;
  });
  sweep_unary("NEG", 0x9401, [](int a, int, uint8_t in) {
    int res = (0 - a) & 0xFF;
    // Через арифметику: 0 - a; H — заём из бита 3; V — переполнение знакового.
    int h = (0 - (a & 15)) < 0;
    int v = (0 - s8(a)) > 127;
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg = merge(in, ALL_ARITH, pack(a != 0, res == 0, bit(res, 7), v, h));
    return e;
  });
  sweep_unary("SWAP", 0x9402, [](int a, int, uint8_t in) {
    Expect e;
    e.value = static_cast<uint8_t>(((a & 15) << 4) | (a >> 4));
    e.sreg = in; // флаги не меняются
    return e;
  });
  sweep_unary("LSR", 0x9406, [](int a, int, uint8_t in) {
    int res = a >> 1, c = a & 1;
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg = merge(in, NZVS | (1 << C), pack(c, res == 0, 0, c, 0));
    return e;
  });
  sweep_unary("ASR", 0x9405, [](int a, int, uint8_t in) {
    int res = s8(a) >> 1, c = a & 1; // арифметический сдвиг через знаковый int
    int n = bit(res, 7);
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg =
        merge(in, NZVS | (1 << C), pack(c, (res & 0xFF) == 0, n, n ^ c, 0));
    return e;
  });
  sweep_unary("ROR", 0x9407, [](int a, int cin, uint8_t in) {
    int res = ((a >> 1) | (cin << 7)) & 0xFF, c = a & 1;
    int n = bit(res, 7);
    Expect e;
    e.value = static_cast<uint8_t>(res);
    e.sreg = merge(in, NZVS | (1 << C), pack(c, res == 0, n, n ^ c, 0));
    return e;
  });
}

TEST(alu_adiw_sbiw_match_oracle) {
  const int ks[] = {0, 1, 2, 15, 16, 31, 32, 62, 63};
  for (int sub = 0; sub < 2; ++sub) {
    for (int k : ks) {
      auto emu =
          make_emu({{0, {sbiw_adiw(sub ? 0x9700 : 0x9600, 24, k), rjmp(-2)}}});
      for (int val = 0; val < 65536; ++val) {
        uint8_t in = background(val & 0xFF, val >> 8);
        emu->write_data(24, val & 0xFF, 1);
        emu->write_data(25, val >> 8, 1);
        emu->write_data(SREG, in, 1);
        emu->step_instruction();

        int wide = sub ? val - k : val + k;
        int res = wide & 0xFFFF;
        int c = sub ? (wide < 0) : (wide > 0xFFFF);
        int sval = static_cast<int16_t>(val);
        int swide = sub ? sval - k : sval + k;
        int v = (swide < -32768 || swide > 32767);
        int n = bit(res, 15);
        uint8_t exp_sreg =
            merge(in, (1 << C) | NZVS, pack(c, res == 0, n, v, 0));

        uint16_t got = static_cast<uint16_t>(emu->read_data(24, 1) |
                                             (emu->read_data(25, 1) << 8));
        uint8_t got_sreg = static_cast<uint8_t>(emu->read_data(SREG, 1));
        if (got != res || got_sreg != exp_sreg)
          fail(sub ? "SBIW" : "ADIW", val, k, in, got, res, got_sreg, exp_sreg);
        emu->step_instruction(); // rjmp
      }
    }
  }
}

TEST(alu_mul_family_matches_oracle_exhaustively) {
  struct Op {
    const char *name;
    uint16_t opcode;
    int (*product)(int, int); // знаковое/беззнаковое произведение
    bool frac;
  };
  const Op ops[] = {
      {"MUL", rd_rr(0x9C00, 16, 17), [](int a, int b) { return a * b; }, false},
      {"MULS", 0x0200 | ((16 - 16) << 4) | (17 - 16),
       [](int a, int b) { return s8(a) * s8(b); }, false},
      {"MULSU", 0x0300 | ((16 - 16) << 4) | (17 - 16),
       [](int a, int b) { return s8(a) * b; }, false},
      {"FMUL", 0x0308 | ((16 - 16) << 4) | (17 - 16),
       [](int a, int b) { return a * b; }, true},
      {"FMULS", 0x0380 | ((16 - 16) << 4) | (17 - 16),
       [](int a, int b) { return s8(a) * s8(b); }, true},
      {"FMULSU", 0x0388 | ((16 - 16) << 4) | (17 - 16),
       [](int a, int b) { return s8(a) * b; }, true},
  };
  for (const Op &op : ops) {
    auto emu = make_emu({{0, {op.opcode, rjmp(-2)}}});
    for (int a = 0; a < 256; ++a) {
      for (int b = 0; b < 256; ++b) {
        uint8_t in = background(a, b);
        emu->write_data(16, a, 1);
        emu->write_data(17, b, 1);
        emu->write_data(SREG, in, 1);
        uint64_t c0 = emu->get_cycle_count();
        emu->step_instruction();
        CHECK_EQ(emu->get_cycle_count() - c0, 2u); // все MUL* — 2 такта

        int p = op.product(a, b) & 0xFFFF; // произведение как 16-битное
        int c = bit(p, 15);
        int res = op.frac ? (p << 1) & 0xFFFF : p;
        uint8_t exp_sreg =
            merge(in, (1 << C) | (1 << Z), pack(c, res == 0, 0, 0, 0));
        int got = static_cast<int>(emu->read_data(0, 1) |
                                   (emu->read_data(1, 1) << 8));
        uint8_t got_sreg = static_cast<uint8_t>(emu->read_data(SREG, 1));
        if (got != res || got_sreg != exp_sreg)
          fail(op.name, a, b, in, got, res, got_sreg, exp_sreg);
        emu->step_instruction(); // rjmp .-2
      }
    }
  }
}
