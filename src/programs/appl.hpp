#pragma once

#include "cmd_line_params.hpp"
#include "db2_rtl.hpp"
#include "generator.hpp"
#include "parser.hpp"
namespace dbgen4
{
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
    void        raw_command_line(int argc, char** argv);
    pars_result process_one_file(rtl::db_db2& db, const str_t& filename);
    /// member(s)
    cmd_line_params p_;      /// comand line parameter structure
    parser          parser_; /// parser object
    generator       gen_;    /// code generator object
  };
}; // namespace dbgen4
