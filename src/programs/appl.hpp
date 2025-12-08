#pragma once

#include "cmd_line_params.hpp"
// #include "context.hpp"
#include "data_statements.hpp"
#include "db2_rtl.hpp"
#include "generator.hpp"
#include "parser.hpp"
#include "parser_errors.hpp"
#include <expected>
namespace dbgen4
{
  using e_data_statements = ::std::expected<data_statements, exit_status_enum>;
  class appl
  {
  public:
    appl();
    ~appl();
    appl(const appl&)                       = delete;
    appl(appl&&)                            = delete;
    appl&            operator=(const appl&) = delete;
    appl&            operator=(appl&&)      = delete;
    exit_status_enum exec(int argc, char** argv, char** env); /// execute application
  private:
    static class log::log* log_() { return log::get(); };
    /// method logs raw command line
    void              display_raw_command_line_log(int argc, char** argv);
    e_data_statements process_one_file(rtl::db_db2& db, generator& gen);
    /// member(s)
    cmd_line_params p_;      /// comand line parameter structure
    parser          parser_; /// parser object
    // generator       gen_;    /// code generator object
  };
}; // namespace dbgen4
