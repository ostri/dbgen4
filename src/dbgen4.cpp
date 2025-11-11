
#include "appl.hpp"
// #include "log.hpp" // NOLINT(unused-includes)
// #include "inja.hpp"
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
namespace fs = std::filesystem;

int main(int argc, char** argv, char** env)
{
  try
  {
    // Log initialization
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const auto* config_file = std::getenv("LOG_CONFIG");
    log::init_from_json(config_file != nullptr ? config_file : "");
    log::setup_terminate_handler();
    log::setup_signal_handler();
    const auto* program_name = argv[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    fs::path    p(program_name);
    log::get()->info("Program {} started", p.filename().string());
    /// application initialization
    dbgen4::appl app;
    /// start with parsing and generating
    auto sts = app.exec(argc, argv, env);
    log::get()->info("Program is finished. return code '{}' return status '{}'",
                     ME::enum_integer(sts),
                     ME::enum_name(sts));
    log::get()->flush();
    // spdlog::shutdown();
    return ME::enum_integer(sts);
  }
  catch (...)
  {
    const auto* msg = "Unexpected error during application execution";
    //  Uporabimo novo javno metodo razreda log
    log::get()->critical(msg);
    log::get()->flush();
    log::log_current_exception_with_chain();

    // return ME::enum_integer<dbgen4::parser_err_enum>(parser_err_enum::unhandled_exception);
    return -1;
  }
  return 255; // NOLINT
}
