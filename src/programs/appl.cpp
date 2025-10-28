#include "appl.hpp"
#include "../parser/parser.hpp"
#include <magic_enum.hpp>
#include <vector>
#include <string>
#include "common.hpp"
#include "parser_errors.hpp"

namespace dbgen4
{
  appl::appl() = default;

  appl::~appl() { l->flush(); };

  int appl::exec(int argc, char** argv, char** env)
  {
    parser p;
    l->info("=========== Application initialized ===========");
    auto sts = p_.load_parameters(argc, argv, env);
    raw_command_line(argc, argv);
    // p.load_grammar();
    for (const auto& file : p_.files())
    {
      auto r = p.parse_yaml_file(file, p_.db_type());
      sts    = ME::enum_integer(r.e());
      // const auto* fmt = get_parser_err_str(r.second);
      l->info("File '{}' parser status: {}", file, magic_enum::enum_name(r.e()));
    }

    l->info("Application exit code '{}' '{}'",
            sts,
            magic_enum::enum_name(static_cast<parser_err_enum>(sts)));
    return sts;
  }

  void appl::raw_command_line(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::vector<std::string> vec(argv, argv + argc);
    auto                           cmd_line = join(vec, " ");
    l->debug("Raw command line: {}", cmd_line);
  }


}; // namespace dbgen4