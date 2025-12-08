#include "appl.hpp"
#include "build_type.hpp"
#include "context.hpp"
// #include "data_statements.hpp"
#include "parser.hpp"
#include <stdexcept>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)

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

  appl::~appl() { log_()->flush(); };
  /**
   * @brief method process one yaml file from parsing to code generation
   *
   * @param db access to the database
   * @param filename name of the yaml file to be processed
   * @return true all went ok
   * @return false there were errors / check the logs
   */
  e_data_statements appl::process_one_file(rtl::db_db2& db, generator& gen)
  {
    auto filename = gen.yaml_fn();
    auto r        = parser_.parse_yaml_file(filename, gen.db_type());
    if (! r)
    {
      auto sts = ME::enum_integer(r.error());
      log_()->info("File '{}' parser status: {} db status {}", filename, magic_enum::enum_name(r.error()), sts);
      return std::unexpected(r.error());
    }
    r = parser_.load_file_meta_data(r.value(), db); /// statements enriched with metadata
    if (! r)
    {
      log_()->info("File '{}' metadata load failed. status: {}", filename, ME::enum_name(r.error()));
      return std::unexpected(r.error());
    }
    auto res = gen.generate(r.value());
    if (! res)
    {
      log_()->info("File '{}' source code generation failed. status: {}", filename, ME::enum_name(res.error()));
      return std::unexpected(res.error());
    }
    //    log_()->info("Generated hpp file:\n{}", res.value());
    log_()->info("Data model generation from file '{}' successful", filename);
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
  exit_status_enum appl::exec(int argc, char** argv, char** env)
  {
    log_()->info("build type: {}", build_type_name());

    parser p;
    log_()->info("=========== Application initialized ===========");
    auto sts = p_.load_parameters(argc, argv, env);
    log_()->info("Command line parsing. status: '{}'", ME::enum_name(sts));
    if (sts != exit_status_enum::ok) return sts; // exit on help or error in parsing
    display_raw_command_line_log(argc, argv);
    try
    {
      rtl::db_db2 db; // access to the RDBMS
      auto        r = db.connect(p_.host(), p_.port(), p_.db_name(), p_.user(), p_.pass());
      log_()->info("Database connection status: {}", ME::enum_name<db_sts>(r));
      if (! rtl::is_success(r))
      {
        log_()->error("Unable to connect to database '{}'", p_.db_name());
        return exit_status_enum::connection_error;
      }
      context ctx(p_); /// package cmd line parameters
      // auto    res = ctx.prepare_templates(); /// prepare templates
      //      if (! res) return res.error();         /// errors in template generation
      generator gen(ctx); /// bare bone generator
      auto      res = gen.register_callbacks();
      if (! res) return res.error(); /// errors in template generation
      res = gen.prepare_templates();
      if (! res) return res.error(); /// errors in template generation
      /// walk over all parameter files
      for (const auto& filename : p_.files())
      {
        gen.set_yaml_fn_and_barename(filename);
        auto res = process_one_file(db, gen);
        if (! res)
        {
          log_()->error("Error during processing file '{}' error {}", filename, ME::enum_name(res.error()));
          sts = res.error();
          break;
        }
      }
      log_()->info("Application exit code '{}' '{}'", ME::enum_integer(sts), ME::enum_name(sts));
      db.rollback();
      db.disconnect();
      return sts;
    }
    catch (const CLI::CallForHelp& e)
    {
      log_()->debug("Help exit");
      return exit_status_enum::ok;
    }
    catch (const std::runtime_error& e)
    {
      log_()->critical("Runtime error: '{}'", e.what());
      return exit_status_enum::unhandled_exception;
    }
    catch (...)
    {
      const auto* const msg = "Unexpected error during application execution";
      log_()->error(msg);
      return exit_status_enum::unhandled_exception;
    };
  };

  void appl::display_raw_command_line_log(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const vec_str_t vec(argv, argv + argc);
    auto            cmd_line = join(vec, " ");
    log_()->trace("command line: {}", cmd_line);
  }


}; // namespace dbgen4