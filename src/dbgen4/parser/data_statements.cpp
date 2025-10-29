#include "common.hpp"
#include "data_statements.hpp"
#include "data_statement.hpp"

namespace dbgen4
{

  str_t data_statements::summary() const { return summary_; }

  void  data_statements::set_summary(const str_t& summary) { summary_ = summary; }
  str_t data_statements::description() const { return description_; }

  cmd_line_params data_statements::params() const { return params_; }

  data_statement_map_t data_statements::map() const { return map_; }

  void data_statements::set_map(const data_statement_map_t& map) { map_ = map; }

  void data_statements::set_description(const str_t& description) { description_ = description; }

  void data_statements::set_params(const cmd_line_params& params) { params_ = params; }

  bool data_statements::add_statement(const data_statement& s)
  {
    auto r = map_.emplace(s.id(), s);
    if (! r.second) log->error("Statement id: {} is duplicated.", s.id());
    return r.second;
  }
}; // namespace dbgen4
