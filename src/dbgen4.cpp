
#include "appl.hpp"
#include <magic_enum.hpp>
#ifndef NDEBUG
// #  include <sanitizer/lsan_interface.h>
#endif
int main(int argc, char** argv, char** env)
{
  try
  {
    // Log initialization
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    log::init_from_json(std::getenv("LOG_CONFIG"));
    log::setup_terminate_handler();
    log::setup_signal_handler();
    log::get()->info("Program started");
    dbgen4::appl app;
    // Spremeni nivo konzole v teku
    auto sts = app.exec(argc, argv, env);
    return sts;
  }
  catch (...)
  {
    // const auto* msg = "Unexpected error during application execution";
    //  Uporabimo novo javno metodo razreda log
    log::log_current_exception_with_chain();

    // return ME::enum_integer<dbgen4::parser_err_enum>(parser_err_enum::unhandled_exception);
    return -1;
  }
  return 255; // NOLINT
}
