#include <signal.h>
#include <stdio.h>
#include "logs.hpp"
#include "constants.hpp"


void logs::fatal(const char *format, ...) {
  va_list vargs;
  fprintf(stderr, "%sfatal%s | ", KMAG, KNRM);
  va_start(vargs, format);
  vfprintf(stderr, format, vargs);
  fprintf(stderr, "\n");
  va_end(vargs);
  #if PLATFORM & PLATFORM_WINDOWS
    __debugbreak();
  #elif PLATFORM & PLATFORM_POSIX
    raise(SIGABRT);
  #endif
}


void logs::error(const char *format, ...) {
  va_list vargs;
  fprintf(stderr, "%serror%s | ", KRED, KNRM);
  va_start(vargs, format);
  vfprintf(stderr, format, vargs);
  fprintf(stderr, "\n");
  va_end(vargs);
}


void logs::warning(const char *format, ...) {
  va_list vargs;
  fprintf(stderr, "%swarn%s  | ", KYEL, KNRM);
  va_start(vargs, format);
  vfprintf(stderr, format, vargs);
  fprintf(stderr, "\n");
  va_end(vargs);
}


void logs::info(const char *format, ...) {
  va_list vargs;
  fprintf(stdout, "%sinfo%s  | ", KBLU, KNRM);
  va_start(vargs, format);
  vfprintf(stdout, format, vargs);
  fprintf(stdout, "\n");
  va_end(vargs);
}


void logs::print_newline() {
  fprintf(stdout, "\n");
}


void logs::print_m4(m4 *t) {
  info("(%f, %f, %f, %f)", (*t)[0][0], (*t)[1][0], (*t)[2][0], (*t)[3][0]);
  info("(%f, %f, %f, %f)", (*t)[0][1], (*t)[1][1], (*t)[2][1], (*t)[3][1]);
  info("(%f, %f, %f, %f)", (*t)[0][2], (*t)[1][2], (*t)[2][2], (*t)[3][2]);
  info("(%f, %f, %f, %f)", (*t)[0][3], (*t)[1][3], (*t)[2][3], (*t)[3][3]);
}


void logs::print_v2(v2 *t) {
  info("(%f, %f)", (*t)[0], (*t)[1]);
}


void logs::print_v3(v3 *t) {
  info("(%f, %f, %f)", (*t)[0], (*t)[1], (*t)[2]);
}


void logs::print_v4(v4 *t) {
  info("(%f, %f, %f, %f)", (*t)[0], (*t)[1], (*t)[2], (*t)[3]);
}
