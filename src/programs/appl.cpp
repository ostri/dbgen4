#include "appl.hpp"
#include "../parser/parser.hpp"
#include <magic_enum.hpp>
#include <vector>
#include <string>
#include "common.hpp"
#include "parser_errors.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"

namespace dbgen4
{
  using rtl::db_sts;

  appl::appl() = default;

  appl::~appl() { log()->flush(); };

  int appl::exec(int argc, char** argv, char** env)
  {
    parser p;
    log()->info("=========== Application initialized ===========");
    auto sts = p_.load_parameters(argc, argv, env);
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    if (sts != 55) return 0; // exit on help or error in parsing
    raw_command_line(argc, argv);
    try
    {
      rtl::db_db2 db;
      // auto r = db.connect(p_.db_host(), p_.db_name(), p_.db_user(), p_.db_password());
      //      auto r = db.connect("localhost", "50000", p_.db_name(), "ostri", "!123alfa");
      auto r = db.connect(p_.db_name());
      log()->info("Database connection status: {}", ME::enum_name<db_sts>(r));
      if (! rtl::is_success(r))
      {
        log()->error("Unable to connect to database '{}'", p_.db_name());
        return ME::enum_integer(parser_err_enum::connection_error);
      }
      for (const auto& file : p_.files())
      {
        auto r = p.parse_yaml_file(file, p_.db_type());
        sts    = ME::enum_integer(r.e());
        // const auto* fmt = get_parser_err_str(r.second);
        log()->info("File '{}' parser status: {}", file, magic_enum::enum_name(r.e()));
      }

      log()->info("Application exit code '{}' '{}'",
                  sts,
                  magic_enum::enum_name(static_cast<parser_err_enum>(sts)));
      db.disconnect();
      return sts;
    }
    catch (const CLI::CallForHelp& e)
    {
      log()->debug("Help exit");
      return 0;
    }
    catch (...)
    {
      const auto* const msg = "Unexpected error during application execution";
      log()->error(msg);
      return ME::enum_integer(parser_err_enum::unhandled_exception);
    };
  };

  void appl::raw_command_line(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::vector<std::string> vec(argv, argv + argc);
    auto                           cmd_line = join(vec, " ");
    log()->trace("command line: {}", cmd_line);
  }


}; // namespace dbgen4