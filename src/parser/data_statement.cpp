#include "data_statement.hpp"
#include <magic_enum.hpp>
namespace dbgen4
{

  str_t         data_statement::id() const { return id_; }
  map_db_type_t data_statement::sql() const { return sql_; }
  /**
   * Fetch sql statement specific for the provided database type.
   * If there is no sql for specific database type, it returns sql for generic sql.
   *
   * @param v database type
   * @return str_t sql associated with the provided database type or generic sql
   */
  str_t data_statement::sql(db_type_enum v) const
  {
    str_t res{}; /// result sql statement
    auto  it = sql_.find(v);
    if (it != sql_.end())
    { // we have sql for this specific db type
      return it->second;
    }
    else
    { // no sql for this specific db type, try to return generic sql
      l->info("No sql statement for db type '{}', trying to return generic sql.", ME::enum_name(v));
      auto it = sql_.find(db_type_enum::sql);
      if (it != sql_.end())
      {
        return it->second; // return generic sql
      }
      else
      {
        auto msg = fmt::format(
          "No specific and/or generic sql statement found. statement id: '{}' database: {}",
          id_,
          ME::enum_name(v));
        l->error(msg);
        return {};
      }
    }
  }

  void data_statement::set_id(const str_t& id) { id_ = id; }
  /// set sql for specific database type
  /// @param v database type
  /// @param sql sql statement
  void data_statement::set_sql(db_type_enum v, const str_t& sql) { sql_[v] = sql; }
  void data_statement::set_sql(const map_db_type_t& sql) { sql_ = sql; }
}; // namespace dbgen4
