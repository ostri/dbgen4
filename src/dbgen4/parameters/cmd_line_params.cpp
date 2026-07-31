//
// Created by ostri on 2024/02/04
//

// #include "log.hpp"
#include "cmd_line_params.hpp"
#include "common.hpp"
#include "parser_errors.hpp"
#include "rtl.hpp" // rtl::default_port()
#include <fmt/format.h>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)


#include "CLI/App.hpp"
#include "CLI/Config.hpp"    // IWYU pragma: export
#include "CLI/Formatter.hpp" // IWYU pragma: export
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

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
  size_t       cmd_line_params::max_field_len() const { return max_field_len_; }
  /**
   * @brief Method returns string that describes the object attribute values
   *
   * @param offs offset from the left in log file
   * @return str_t
   */
  str_t cmd_line_params::dump(size_t offs) const
  {
    str_t s{};
    str_t left_padding(offs, ' ');

    const auto* db_type = ME::enum_name(db_type_).data();
    /// serialize files
    for (auto const& el : files_)
    {
      s += fmt::format(R"(    {}
)",
                       el);
    }
    auto msg = fmt::format(R"(
{}host:       {}
{}port:       {}
{}db_name:    {}
{}db_type:    {}
{}user:       {}
{}pass:       {}
{}out folder: {}
{}verbose:    {}
{}files:
{}
)",
                           left_padding,
                           host_,
                           left_padding,
                           port_,
                           left_padding,
                           db_name_,
                           left_padding,
                           db_type,
                           left_padding,
                           user_,
                           left_padding,
                           "****",
                           left_padding,
                           out_folder_,
                           left_padding,
                           verbose_,
                           left_padding,
                           s);
    return msg;
  }

  exit_status_enum cmd_line_params::load_parameters(int argc, char** argv, char** /*env*/)
  {
    CLI::App app{"Generator of db layer for c++ programs."};

    str_t s{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const vec_str_t arr(ME::enum_names<db_type_enum>().begin() + 1, ME::enum_names<db_type_enum>().end());
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
        if (res && (res.value() != db_type_enum::sql)) // NOLINT(readability-inconsistent-ifelse-braces)
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
      ->default_val(rtl::default_port()); // depends on the backend linked in
    /// database name
    app.add_option("-n,--db-name", db_name_, "database name")
      ->required();
    /// database user
    app.add_option("-u,--username", user_, "database user")
      ->required();
    /// database user password
    app.add_option("-p,--password", pass_, "database user password")
      ->required();
    /// fallback width for columns the database reports no length for
    app.add_option("-l,--max-field-len", max_field_len_,
                   "width assumed for columns with no declared length (text, json, bytea, ...). "
                   "Override per column with 'field-len' in the yaml file.")
      ->default_val(default_max_field_len)
      ->check(CLI::PositiveNumber);
    /// output folder
    app.add_option("-o,--out-folder", out_folder_, "output folder for generated files.")
      ->default_val("./");
    /// verbose
    app.add_flag("-v,--verbose", verbose_, "verbose output")
      ->default_val(false);
    /// YAML files
    app.add_option("files", files_, "YAML files to be processed")
      ->check(CLI::ExistingFile) // the file provided must exist
      ->required();              // one or more filenames must be provided
    // clang-format on
    try
    {
      // set_log_level(false);
      const std::vector<const char*> arg = {};
      if (argc == 1)
      {
        // char* fake_argv[] = {argv[0], (char*)"--help"}; // NOLINT
        // argc              = 2;
        // argv              = fake_argv; // NOLINT
        std::array<char*, 3> arg = {argv[0], const_cast<char*>("--help"), nullptr}; // NOLINT
        app.parse(arg.size() - 1, arg.data());
      }
      else app.parse(argc, argv); // NOLINT(readability-inconsistent-ifelse-braces)
      set_log_level(verbose_);

      log_()->info(R"(Command line parameter values :
{})",
                   dump(2));
      return exit_status_enum::ok;
    }
    catch (const CLI::CallForAllHelp& e)
    {
      app.exit(e);
      log_()->debug("All Help command.{}", e.what());
      return exit_status_enum::cmd_all_help;
    }
    catch (const CLI::CallForHelp& e)
    {
      app.exit(e);
      log_()->debug("Help command.{}", e.what());
      return exit_status_enum::cmd_help;
    }
    catch (const CLI::CallForVersion& e)
    {
      app.exit(e);
      log_()->debug("Version command.{}", e.what());
      return exit_status_enum::cmd_version;
    }
    catch (const CLI::ParseError& e)
    {
      app.exit(e);
      auto msg = fmt::format("name: '{}' code: {} msg: '{}'", e.get_name(), e.get_exit_code(), e.what());
      log_()->warn(msg);
      log_()->warn("Parameters with error(s) \n{}", dump(2));
      return exit_status_enum::connection_error;
    }
    catch (...)
    {
      auto msg = fmt::format("Unhandled exception. file: {} line {}", __FILE_NAME__, __LINE__);
      log_()->critical(msg);
      throw;
    }
  }

  void cmd_line_params::set_log_level(bool verbose) const
  {
    if (is_debug_build()) { log_()->set_level(verbose ? rtl::logger::level::trace : rtl::logger::level::info); }
    else {
      log_()->set_level(verbose ? rtl::logger::level::info : rtl::logger::level::warn);
    };
  }
}; // namespace dbgen4
