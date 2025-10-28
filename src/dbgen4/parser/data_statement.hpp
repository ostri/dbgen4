#pragma once

#include "common.hpp"
#include "sql_col_def.hpp"
#include <magic_enum.hpp>
#include <map>

namespace dbgen4
{
  /**
   * @brief array of sql's where each db type flavour has its own possible empty version
   *
   * sql is for generic sql statement. Others are for RDBMs specifics
   */
  using map_db_type_t = std::map<db_type_enum, str_t>;
  /**
   * @brief data about specific sql statement
   *
   */
  class data_statement : private log
  {
  public:
    data_statement()                                 = default;
    virtual ~data_statement()                        = default;
    data_statement(const data_statement& o)          = default;
    data_statement(data_statement&&) noexcept        = default;
    data_statement& operator=(const data_statement&) = default;
    data_statement& operator=(data_statement&&)      = delete;
    /*! getters */
    [[nodiscard]] str_t id() const; ///< fetch unique id of the statement
    /**
     * @brief Fetch sql specific for the provided database type.
     *
     * If there is no sql for specific database type, it returns sql for generic sql.
     *
     * @param v database type for which sql variant we are looking for
     * @return str_t sql variant of the sql statement specific for the provided db type or generic
     * sql if there is no specific one
     */
    [[nodiscard]] str_t sql() const; ///< fetch all sql statements for generic and
                                     ///< optionaly also for specific rdbms
    [[nodiscard]] sql_col_def_vec_t par_defs() const;
    [[nodiscard]] sql_col_def_vec_t res_defs() const;

    /*! setters */
    void set_id(const str_t& id);   ///< set unique id of the statement
    void set_sql(const str_t& sql); ///< set sql for specific database type
    void set_par_defs(const sql_col_def_vec_t& defs);
    void set_res_defs(const sql_col_def_vec_t& defs);
  protected:
  private:
    str_t             id_;       ///< unique id of the data statement
    str_t             sql_;      ///< sql statement (generic or specific for RDBMS)
    sql_col_def_vec_t par_defs_; ///< input parameter definitions for the statement
    sql_col_def_vec_t res_defs_; ///< output result definitions for the statement
  };

} // namespace dbgen4
