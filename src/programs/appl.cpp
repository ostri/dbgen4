#include "appl.hpp"
// #include "spdlog/spdlog.h" // IWYU pragma: export
// #include <ranges>

namespace dbgen4
{
  appl::appl() = default;

  appl::~appl() { l->flush(); };

  int appl::exec(int argc, char** argv, char** env)
  {
    l->info("!!!+++~~~--- Application initialized. ---~~~+++!!!");
    auto sts = p_.load_parameters(argc, argv, env);
    raw_command_line(argc, argv);
    l->warn("Application exit code '{}'", sts);
    return sts;
  }

  void appl::raw_command_line(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::vector<std::string> vec(argv, argv + argc);
    auto                     cmd_line = join(vec, " ");
    l->debug("Raw command line: {}", cmd_line);
  }


}; // namespace dbgen4