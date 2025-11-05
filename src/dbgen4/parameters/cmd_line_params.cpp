//
// Created by ostri on 2024/02/04
//

#include "log.hpp"
#include "cmd_line_params.hpp"
#include "common.hpp"
#include <fmt/format.h>
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>

#include "CLI/App.hpp"
#include "CLI/Config.hpp"    // IWYU pragma: export
#include "CLI/Formatter.hpp" // IWYU pragma: export
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>
// #include <spdlog/common.h>

namespace dbgen4
{

  vec_str_t    cmd_line_params::files() const { return files_; }
  db_type_enum cmd_line_params::db_type() const { return db_type_; }
  str_t        cmd_line_params::db_name() const { return db_name_; }
  str_t        cmd_line_params::out_folder() const { return out_folder_; }
  bool         cmd_line_params::is_verbose() const { return verbose_; }
  str_t        cmd_line_params::user() const { return user_; }
  str_t        cmd_line_params::pass() const { return pass_; }
  str_t        cmd_line_params::host() const { return host_; }
  size_t       cmd_line_params::port() const { return port_; }
  /**
   * @brief Method returns string that describes the object attribute values
   *
   * @param offs offset from the left in log file
   * @return str_t
   */
  str_t cmd_line_params::dump(int /*offs*/) const
  {
    str_t s{};
    // str_t r(offs, ' ');

    const auto* db_type = ME::enum_name(db_type_).data();
    for (auto const& el : files_)
    {
      s += fmt::format(R"(    {}
)",
                       el);
    }
    // if (! s.empty()) s.pop_back(); // cut last space off
    auto msg = fmt::format(R"(
  host:       {}
  port:       {}
  db_name:    {}
  db_type:    {}
  user:       {}
  pass:       {}
  out folder: {}
  verbose:    {}
  files:      
{}
)",
                           host_,
                           port_,
                           db_name_,
                           db_type,
                           user_,
                           "****",
                           out_folder_,
                           verbose_,
                           s);
    // msg.pop_back();
    return msg;
  }

  int cmd_line_params::load_parameters(int argc, char** argv, char** /*env*/)
  {
    CLI::App app{"Generator of db layer for c++ programs."};

    str_t           s{};
    const vec_str_t arr(ME::enum_names<db_type_enum>().begin() + 1,
                        ME::enum_names<db_type_enum>().end());
    for (const auto& el : arr) s += str_t(el) + str_t(",");
    s.resize(s.length() - 1);
    str_t enum_str = std::string(ME::enum_name<db_type_enum>(db_type_enum::sql));
    // clang-format off
    /// database type
    auto help_str = fmt::format("database type : [{}]", s);
    app.add_option("-t,--db-type", enum_str, help_str      )
      ->default_val(db_type_enum::sql)
      ->check([s, this](const std::string& v)
      {
        auto res = ME::enum_cast<db_type_enum>(v);
        db_type_ = res ? res.value() : db_type_enum::sql;
        if (res && (res.value() != db_type_enum::sql))
          db_type_ = res.value();
        else
        {
          auto msg = fmt::format("Database type '{}' is not valid. Valid values are '{}'.", v, s);
          throw CLI::ConversionError(msg);
        }
        //return true;
        return "";
      });
    /// host name
    app.add_option("--host", host_, "host where database resides")
      ->default_val("localhost");
    /// port
      app.add_option("--port", port_, "port of host where database resides")
      ->default_val(50000); // NOLINT
    /// database name
    app.add_option("-n,--db-name", db_name_, "database name")
      ->required();
    /// database user
    app.add_option("-u,--username", user_, "database user")
      ->required();
    /// database user password
    app.add_option("-p,--password", pass_, "database user password")
      ->required();
    /// output folder
    app.add_option("-o,--out-folder", out_folder_, "output folder for generated files.")
      ->default_val("./");
    /// verbose
    app.add_flag("-v,--verbose", verbose_, "verbose output")
      ->default_val(false);
    /// gsql files
    app.add_option("files", files_, "gsql files to be processed")
      ->check(CLI::ExistingFile) // the file provided must exist
      ->required();              // one or more filenames must be provided
    // clang-format on
    try
    {
      set_log_level(false);
      if (argc == 1) throw CLI::CallForHelp();
      app.parse(argc, argv);
      set_log_level(verbose_);

      log()->info(R"(Command line parameter values :
{})",
                  dump(2));
    }
    catch (const CLI::CallForHelp& e)
    {
      log()->debug("Help command.");
      return app.exit(e);
    }
    catch (const CLI::ParseError& e)
    {
      auto msg =
        fmt::format("name: '{}' code: {} msg: '{}'", e.get_name(), e.get_exit_code(), e.what());
      log()->warn(msg);
      log()->warn("Parameters with error(s) \n{}", dump(2));
      return app.exit(e);
    }
    catch (...)
    {
      auto msg = fmt::format("Unhadled exception. file: {} line {}", __FILE_NAME__, __LINE__);
      log()->critical(msg);
      throw;
    }
    return 55; // NOLINT
    // FIXME(ostri) popravi to magično številko
  }

  void cmd_line_params::set_log_level(bool verbose) const
  {
    if (is_debug_build()) { log()->set_level(verbose ? log::trace : log::info); }
    else { log()->set_level(verbose ? log::info : log::warn); };
  }
}; // namespace dbgen4
