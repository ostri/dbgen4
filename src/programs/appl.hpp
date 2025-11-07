#pragma once

#include "cmd_line_params.hpp"
#include "data_statements.hpp"
#include "db2_rtl.hpp"
#include "generator.hpp"
#include "parser.hpp"
#include "parser_errors.hpp"
#include <expected>
namespace dbgen4
{
  using e_data_statements = std::expected<data_statements, exit_status_enum>;
  class appl
  {
  public:
    appl();
    ~appl();
    appl(const appl&)                      = delete;
    appl(appl&&)                           = delete;
    appl&           operator=(const appl&) = delete;
    appl&           operator=(appl&&)      = delete;
    int             exec(int argc, char** argv, char** env); /// execute application
    spdlog::logger* log();
  private:
    /// method logs raw command line
    void              raw_command_line(int argc, char** argv);
    e_data_statements process_one_file(rtl::db_db2& db, const str_t& filename);
    /// member(s)
    cmd_line_params p_;      /// comand line parameter structure
    parser          parser_; /// parser object
    generator       gen_;    /// code generator object
  };
}; // namespace dbgen4
