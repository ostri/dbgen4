
#include "appl.hpp"
#ifndef NDEBUG
#  include <sanitizer/lsan_interface.h>
#endif
int main(int argc, char** argv, char** env)
{
  dbgen4::appl app;
  // NOLINT
  // const int len = 2000;
  // int*      x   = new int[len]; // NOLINT
  // x[0]          = 1;            // NOLINT
  // x[len + 1]    = 1000;         // NOLINT
  //  delete[] x;                   // NOLINT
  auto sts = app.exec(argc, argv, env);
#ifndef NDEBUG
  __lsan_do_recoverable_leak_check();
#endif
  return sts;
}
