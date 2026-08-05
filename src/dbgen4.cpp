
#include "appl.hpp"
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <logger/logger.hpp>
#include <logger/logger_config.hpp>
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
namespace fs = std::filesystem;

int main(int argc, char** argv, char** env)
{
  /// Logger is built once here and handed down by reference to everything
  /// that needs to log - see logger/logger.hpp's class comment. create() has
  /// already logged to stderr why, if cfg's sinks could not be built.
  auto log_ptr = logger::Logger::create(logger::load_logger_config());
  if (! log_ptr) return -1;
  logger::Logger& log = **log_ptr;
  log.setup_terminate_handler();
  log.setup_signal_handler();

  try
  {
    const auto* program_name = argv[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const fs::path p(program_name);
    log.info("Program {} started", p.filename().string());
    /// application initialization
    dbgen4::appl app(log);
    /// start with command line parsing and generating
    auto sts = app.exec(argc, argv, env);
    log.info("Program is finished. return code '{}' return status '{}'", ME::enum_integer(sts), ME::enum_name(sts));
    log.flush();
    return ME::enum_integer(sts);
  }
  catch (const std::exception& e)
  {
    log.critical("Unexpected error during application execution");
    log.log_exception_with_chain(e);
    log.flush();

    // return ME::enum_integer<dbgen4::parser_err_enum>(parser_err_enum::unhandled_exception);
    return -1;
  }
  catch (...)
  {
    log.critical("Unexpected error during application execution (non-std::exception)");
    log.flush();
    return -1;
  }
}
