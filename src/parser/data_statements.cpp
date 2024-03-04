#include "data_statements.hpp"

namespace dbgen4
{

  str_t data_statements::summary() const { return summary_; }

  void  data_statements::set_summary(const str_t& summary) { summary_ = summary; }
  str_t data_statements::description() const { return description_; }

  data_statement_map_t data_statements::map() const { return map_; }

  void data_statements::set_map(const data_statement_map_t& map) { map_ = map; }

  void data_statements::set_description(const str_t& description) { description_ = description; }

  bool data_statements::add_statement(const data_statement& s)
  {
    auto r = map_.emplace(s.id(), s);
    if (! r.second) l->error("Statement id: {} is duplicated.", s.id());
    return r.second;
  }
}; // namespace dbgen4
