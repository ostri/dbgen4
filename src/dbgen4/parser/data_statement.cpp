#include "data_statement.hpp"
#include <magic_enum.hpp>
#include "common.hpp"
#include <fmt/format.h>
namespace dbgen4
{
  str_t data_statement::id() const { return id_; }
  str_t data_statement::sql() const { return sql_; }

  // sql_col_def_vec_t data_statement::par_defs() const { return par_defs_; }
  // sql_col_def_vec_t data_statement::res_defs() const { return res_defs_; }


  void data_statement::set_id(const str_t& id) { id_ = id; }

  void data_statement::set_sql(const str_t& sql) { sql_ = sql; }

  // void data_statement::set_par_defs(const sql_col_def_vec_t& defs) { par_defs_ = defs; }
  // void data_statement::set_res_defs(const sql_col_def_vec_t& defs) { res_defs_ = defs; }
}; // namespace dbgen4
