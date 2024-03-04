#include "data_statement.hpp"
#include <magic_enum.hpp>
namespace dbgen4
{

  str_t         data_statement::id() const { return id_; }
  arr_db_type_t data_statement::sql() const { return sql_; }
  /**
   *
   * @param v database type
   * @return str_t sql assiciated with the provided database type or generic sql
   */
  str_t data_statement::sql(db_type_enum v) const
  {
    str_t  res{};
    size_t pos = ME::enum_integer(v);
    res        = sql_.at(pos);
    if (res.empty()) res = sql_[ME::enum_integer(db_type_enum::sql)];
    return res;
  }
  void data_statement::set_id(const str_t& id) { id_ = id; }
  void data_statement::set_sql(db_type_enum v, const str_t& sql)
  {
    sql_.at(ME::enum_integer(v)) = sql;
  }
  void data_statement::set_sql(const arr_db_type_t& sql) { sql_ = sql; }
}; // namespace dbgen4
