#include "appl.hpp"
#include <spdlog/common.h>

namespace dbgen4
{
  appl::appl()
  {
    l->info("!!!+++---~~~ Application initialized. ~~~---+++!!!");
  }

  appl::~appl() { l->flush(); };

  int appl::exec(int argc, char** argv, char** env)
  {
    auto sts = p_.load_parameters(argc, argv, env);
    if ((sts == 0) && p_.is_verbose())
    {
      l->set_level(spdlog::level::debug);
      raw_command_line(argc, argv);
    };
    l->info("Application exit code '{}'", sts);
    return sts;
  }

  void appl::raw_command_line(int argc, char** argv)
  {
    str_t                    cmd_line{};
    std::vector<const char*> args(
      argv,
      argv + // NOLINT (cppcoreguidelines-pro-bounds-pointer-arithmetic)
        argc);
    for (const char* arg : args) cmd_line += str_t(arg) + ' ';
    cmd_line.pop_back(); // remove last space;
    l->debug("Raw command line: {}", cmd_line);
  }


}; // namespace dbgen4