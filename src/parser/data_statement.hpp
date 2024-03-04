#ifndef DATA_STATEMENT_HPP
#define DATA_STATEMENT_HPP

#include "common.hpp"
#include <magic_enum.hpp>

namespace dbgen4
{
  /**
   * @brief array of sql's where each db type flavour has its own possible empty version
   *
   * first value is for generic sql statement.
   * be carefull the number enum values can grow. Always add new db types at the end.
   */
  using arr_db_type_t = std::array<str_t, ME::enum_count<db_type_enum>()>;
  /**
   * @brief data about specific sql statement
   *
   */
  class data_statement : private log
  {
  public:
    /*! getters */
    [[nodiscard]] str_t id() const; ///< fetch unique id of the statement
    /**
     * @brief Fetch sql specific for the provided database type.
     *
     * If there is no sql for specific database type, it returns sql for generic sql.
     *
     * @param v database type for which sql variant we are looking for
     * @return str_t sql variant of the sql statement specific for the provided db type
     */
    [[nodiscard]] str_t sql(db_type_enum v) const;
    /*! setters */
    void set_id(const str_t& id); ///< set unique id of the statement
    void set_sql(db_type_enum v, const str_t& sql);
  protected:
    [[nodiscard]] arr_db_type_t sql() const; ///< fetch array of sql associated with this statement
    void set_sql(const arr_db_type_t& sql);  ///< set sqls for all database types
  private:
    str_t         id_{};     ///< unique id of the data statement
    arr_db_type_t sql_ = {}; ///< sql statements see db_type_enum
  };

} // namespace dbgen4

#endif // DATA_STATEMENT_HPP
