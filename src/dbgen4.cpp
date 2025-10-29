
#include "appl.hpp"
#ifndef NDEBUG
// #  include <sanitizer/lsan_interface.h>
#endif
int main(int argc, char** argv, char** env)
{
  // Inicializacija: sinhrono, app "moj_program", konzola od warn naprej
  log::init("dbgen4", log::Mode::Sync, spdlog::level::warn);
  log::get()->info("Program started");
  dbgen4::appl app;
  // Spremeni nivo konzole v teku
  log::set_console_level(spdlog::level::info); // zdaj vidiš tudi info
  // NOLINT
  // const int len = 2000;
  // int*      x   = new int[len]; // NOLINT
  // x[0]          = 1;            // NOLINT
  // x[len + 1]    = 1000;         // NOLINT
  //  delete[] x;                   // NOLINT
  auto sts = app.exec(argc, argv, env);
  // #ifndef NDEBUG
  //   __lsan_do_recoverable_leak_check();
  // #endif
  return sts;
}
