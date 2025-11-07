#include "appl.hpp"
#include "build_type.hpp"
#include "data_statements.hpp"
#include "parser.hpp"
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
// #include <vector>
// #include <string>
//  #include "build_type.hpp"
#include "common.hpp"
// #include "pars_result.hpp"
#include "parser_errors.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"

namespace dbgen4
{
  using rtl::db_sts;

  appl::appl() = default;

  appl::~appl() { log()->flush(); };
  /**
   * @brief method process one yaml file from parsing to code generation
   *
   * @param db access to the database
   * @param filename name of the yaml file to be processed
   * @return true all went ok
   * @return false there were errors / check the logs
   */
  e_data_statements appl::process_one_file(rtl::db_db2& db, const str_t& filename)
  {
    auto r   = parser_.parse_yaml_file(filename, p_.db_type());
    auto sts = ME::enum_integer(r.error());
    log()->info(
      "File '{}' parser status: {} db status {}", filename, magic_enum::enum_name(r.error()), sts);
    if (! r) return std::unexpected(r.error());

    r = parser_.load_file_meta_data(r.value(), db);
    if (! r) return std::unexpected(r.error());
    log()->debug(r.value().dump()); /// dump loaded sql statements(sql + meta data)
    auto res = gen_.internal_model_to_json(r.value(), p_, filename); /// generate json data model
    if (! res)
    {
      log()->error("Error during data model generation from file '{}' error {}",
                   filename,
                   ME::enum_name(res.error()));
      return std::unexpected(res.error());
    }
    log()->info("Data model generation from file '{}' successful", filename);
    return r.value();
  }
  /**
   * @brief main execution method of the application
   *
   * @param argc number of command line arguments
   * @param argv array of command line arguments
   * @param env array of environment variables
   * @return int exit status
   */
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
      rtl::db_db2 db; // access to the RDBMS
      auto        r = db.connect(p_.host(), p_.port(), p_.db_name(), p_.user(), p_.pass());
      log()->info("Database connection status: {}", ME::enum_name<db_sts>(r));
      if (! rtl::is_success(r))
      {
        log()->error("Unable to connect to database '{}'", p_.db_name());
        return ME::enum_integer(exit_status_enum::connection_error);
      }
      /// walk over all parameter files
      for (const auto& filename : p_.files())
      {
        auto res = process_one_file(db, filename);
        if (! res)
        {
          log()->error(
            "Error during processing file '{}' error {}", filename, ME::enum_name(res.error()));
          sts = ME::enum_integer(res.error());
          break;
        }
      }
      log()->info("Application exit code '{}' '{}'",
                  sts,
                  magic_enum::enum_name(static_cast<exit_status_enum>(sts)));
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
      return ME::enum_integer(exit_status_enum::unhandled_exception);
    };
  };

  spdlog::logger* appl::log() { return log::get(); };

  void appl::raw_command_line(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const vec_str_t vec(argv, argv + argc);
    auto            cmd_line = join(vec, " ");
    log()->trace("command line: {}", cmd_line);
  }


}; // namespace dbgen4