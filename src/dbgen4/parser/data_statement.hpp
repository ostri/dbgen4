#pragma once

#include "common.hpp"
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
    [[nodiscard]] str_t sql(db_type_enum v) const; ///< fetch sql statement for specific RDBMS
    [[nodiscard]] map_db_type_t sql() const;       ///< fetch all sql statements for generic and
                                                   ///< optionaly also for specific rdbms
    /*! setters */
    void set_id(const str_t& id);                   ///< set unique id of the statement
    void set_sql(db_type_enum v, const str_t& sql); ///< set sql for specific database type
  protected:
    void set_sql(const map_db_type_t& sql); ///< set sqls for all database types
                                            ///< (whole structure at once)
  private:
    // NOLINTNEXTLINE (readability-redundant-member-init)
    str_t id_{}; ///< unique id of the data statement
    // NOLINTNEXTLINE (readability-redundant-member-init)
    map_db_type_t sql_ = {}; ///< sql statements at least sql and optional others
  };

} // namespace dbgen4
