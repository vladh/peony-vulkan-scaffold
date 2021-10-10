#pragma once

#include <stdarg.h>
#include "types.hpp"

namespace logs {
  static constexpr const char *KNRM = "\x1B[0m";
  static constexpr const char *KRED = "\x1B[31m";
  static constexpr const char *KGRN = "\x1B[32m";
  static constexpr const char *KYEL = "\x1B[33m";
  static constexpr const char *KBLU = "\x1B[34m";
  static constexpr const char *KMAG = "\x1B[35m";
  static constexpr const char *KCYN = "\x1B[36m";
  static constexpr const char *KWHT = "\x1B[37m";
  void fatal(const char *format, ...);
  void error(const char *format, ...);
  void warning(const char *format, ...);
  void info(const char *format, ...);
  void print_newline();
  void print_m4(m4 *t);
  void print_v2(v2 *t);
  void print_v3(v3 *t);
  void print_v4(v4 *t);
}
