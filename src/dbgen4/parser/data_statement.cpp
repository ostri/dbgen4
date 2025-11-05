#include "data_statement.hpp"
// #include <magic_enum.hpp>
#include "common.hpp"
#include <fmt/format.h>
namespace dbgen4
{
  str_t data_statement::id() const { return id_; }
  str_t data_statement::sql() const { return sql_; }

  void data_statement::set_id(const str_t& id) { id_ = id; }

  void data_statement::set_sql(const str_t& sql) { sql_ = trim_whitespace_view(sql); }

  spdlog::logger* data_statement::log() const { return log::get(); }
}; // namespace dbgen4
