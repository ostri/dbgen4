
#include "appl.hpp"
// #include "log.hpp" // NOLINT(unused-includes)
// #include "inja.hpp"
#include "magic_enum_config.hpp"
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
namespace fs = std::filesystem;

int main(int argc, char** argv, char** env)
{
  try
  {
    const auto* program_name = argv[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    fs::path    p(program_name);
    log::get()->info("Program {} started", p.filename().string());
    /// application initialization
    dbgen4::appl app;
    /// start with command line parsing and generating
    auto sts = app.exec(argc, argv, env);
    log::get()->info("Program is finished. return code '{}' return status '{}'", ME::enum_integer(sts), ME::enum_name(sts));
    log::get()->flush();
    return ME::enum_integer(sts);
  }
  catch (...)
  {
    const auto* msg = "Unexpected error during application execution";
    log::get()->critical(msg);
    log::get()->flush();
    log::instance().log_current_exception_with_chain();

    // return ME::enum_integer<dbgen4::parser_err_enum>(parser_err_enum::unhandled_exception);
    return -1;
  }
  return 255; // NOLINT
}
