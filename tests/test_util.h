// SPDX-License-Identifier: MIT
#pragma once

#include <iostream>
#include <string>

namespace ipsuip::test {

inline int &failures() {
  static int count = 0;
  return count;
}

template <typename T>
void print_value(std::ostream &os, const T &value) {
  if constexpr (requires { os << value; })
    os << value;
  else
    os << "<не выводится>";
}

template <typename A, typename B>
void check_equal(const A &actual, const B &expected, const char *expr,
                 const char *file, int line) {
  if (actual == expected) return;
  ++failures();
  std::cerr << file << ':' << line << ": FAIL " << expr << "\n  actual:   ";
  print_value(std::cerr, actual);
  std::cerr << "\n  expected: ";
  print_value(std::cerr, expected);
  std::cerr << '\n';
}

inline void check(bool condition, const char *expr, const char *file, int line) {
  if (condition) return;
  ++failures();
  std::cerr << file << ':' << line << ": FAIL " << expr << '\n';
}

inline int report(const char *suite) {
  if (failures() == 0) {
    std::cout << suite << ": OK\n";
    return 0;
  }
  std::cerr << suite << ": " << failures() << " failure(s)\n";
  return 1;
}

} // namespace ipsuip::test

#define CHECK(expr) ::ipsuip::test::check((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected)                                            \
  ::ipsuip::test::check_equal((actual), (expected), #actual " == " #expected,  \
                              __FILE__, __LINE__)
#define TEST_REPORT(name) return ::ipsuip::test::report(name)
