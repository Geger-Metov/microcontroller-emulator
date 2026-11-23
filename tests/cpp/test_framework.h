// Минимальный тестовый каркас без внешних зависимостей.
// Нужен, чтобы тесты собирались и запускались офлайн; при переходе на
// GoogleTest макросы TEST/CHECK_EQ переносятся почти один-в-один.
#pragma once

#include <cstdio>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace mctest {

struct Failure {
  std::string message;
};

struct Case {
  const char *name;
  void (*fn)();
};

inline std::vector<Case> &registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(const char *name, void (*fn)()) {
    registry().push_back({name, fn});
  }
};

inline int run_all() {
  int failed = 0;
  for (const Case &c : registry()) {
    try {
      c.fn();
      std::printf("[ OK   ] %s\n", c.name);
    } catch (const Failure &f) {
      ++failed;
      std::printf("[ FAIL ] %s\n         %s\n", c.name, f.message.c_str());
    } catch (const std::exception &e) {
      ++failed;
      std::printf("[ FAIL ] %s\n         unexpected exception: %s\n", c.name,
                  e.what());
    }
  }
  std::printf("%zu tests, %d failed\n", registry().size(), failed);
  return failed == 0 ? 0 : 1;
}

template <typename A, typename B>
inline void check_eq(const A &a, const B &b, const char *ea, const char *eb,
                     const char *file, int line) {
  if (!(a == b)) {
    std::ostringstream os;
    os << file << ":" << line << ": CHECK_EQ(" << ea << ", " << eb
       << ") failed: " << +a << " != " << +b;
    throw Failure{os.str()};
  }
}

} // namespace mctest

#define TEST(name)                                                             \
  static void name();                                                          \
  static mctest::Registrar registrar_##name(#name, &name);                     \
  static void name()

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::ostringstream os_;                                                  \
      os_ << __FILE__ << ":" << __LINE__ << ": CHECK(" #cond ") failed";       \
      throw mctest::Failure{os_.str()};                                        \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b) mctest::check_eq((a), (b), #a, #b, __FILE__, __LINE__)

#define CHECK_THROWS(expr)                                                     \
  do {                                                                         \
    bool threw_ = false;                                                       \
    try {                                                                      \
      (void)(expr);                                                            \
    } catch (const std::exception &) {                                         \
      threw_ = true;                                                           \
    }                                                                          \
    if (!threw_) {                                                             \
      std::ostringstream os_;                                                  \
      os_ << __FILE__ << ":" << __LINE__                                       \
          << ": CHECK_THROWS(" #expr ") did not throw";                        \
      throw mctest::Failure{os_.str()};                                        \
    }                                                                          \
  } while (0)
