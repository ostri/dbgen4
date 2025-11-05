#include "appl.hpp"
#include "build_type.hpp"
#include "parser.hpp"
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
#include <vector>
#include <string>
// #include "build_type.hpp"
#include "common.hpp"
#include "pars_result.hpp"
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
    log()->info("build type: {}", build_type_name());

    parser p;
    log()->info("=========== Application initialized ===========");
    auto sts = p_.load_parameters(argc, argv, env);
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    if (sts != 55) return 0; // exit on help or error in parsing
    // FIXME(ostri) magic number

    raw_command_line(argc, argv);
    try
    {
      rtl::db_db2 db;
      // auto r = db.connect(p_.db_host(), p_.db_name(), p_.db_user(), p_.db_password());
      auto r = db.connect(p_.host(), p_.port(), p_.db_name(), p_.user(), p_.pass());
      log()->info("Database connection status: {}", ME::enum_name<db_sts>(r));
      if (! rtl::is_success(r))
      {
        log()->error("Unable to connect to database '{}'", p_.db_name());
        return ME::enum_integer(parser_err_enum::connection_error);
      }
      /// walk over all parameter files
      for (const auto& filename : p_.files())
      {
        auto r = p.parse_yaml_file(filename, p_.db_type());
        sts    = ME::enum_integer(r.e());
        log()->info("File '{}' parser status: {}", filename, magic_enum::enum_name(r.e()));
        if (r.e() == parser_err_enum::ok)
        {
          r = p.load_file_meta_data(r.s(), db);
          // log()->debug(r.s().dump());
          //  1 = 1;
        }
      }

      log()->info("Application exit code '{}' '{}'",
                  sts,
                  magic_enum::enum_name(static_cast<parser_err_enum>(sts)));
      db.rollback();
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

  spdlog::logger* appl::log() { return log::get(); };

  void appl::raw_command_line(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::vector<std::string> vec(argv, argv + argc);
    auto                           cmd_line = join(vec, " ");
    log()->trace("command line: {}", cmd_line);
  }


}; // namespace dbgen4