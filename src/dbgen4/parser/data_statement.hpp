#pragma once

#include "common.hpp"
#include "db2_rtl.hpp"
// #include <map>

namespace dbgen4
{
  // /**
  //  * @brief array of sql's where each db type flavour has its own possible empty version
  //  *
  //  * sql is for generic sql statement. Others are for RDBMs specifics
  //  */
  // using map_db_type_t = std::map<db_type_enum, str_t>;
  using meta_vec = std::vector<rtl::meta_dscr>;
  /**
   * @brief data about specific sql statement
   *
   */
  class data_statement
  {
  public:
    data_statement()                        = default;
    virtual ~data_statement()               = default;
    data_statement(const data_statement& o) = default;
    explicit data_statement(const rtl::qry_metadata& o);
    data_statement(data_statement&&) noexcept        = default;
    data_statement& operator=(const data_statement&) = default;
    data_statement& operator=(const rtl::qry_metadata& o);
    /**
     * @brief assign from rtl::qry_metadata
     *
     * @param o
     */
    void            assign(const rtl::qry_metadata& o);
    data_statement& operator=(data_statement&&) = delete;
    /*! getters */
    [[nodiscard]] str_t id() const; ///< fetch unique id of the statement
    /**
     * @brief fetch sql statement
     * @return str_t sql statement
     */
    [[nodiscard]] str_t    sql() const;
    [[nodiscard]] str_t    dscr() const;
    std::string            dump_meta_vector(size_t          offs,
                                            const char*     fmt,
                                            const char*     header,
                                            const meta_vec& v) const;
    [[nodiscard]] meta_vec columns() const;
    [[nodiscard]] meta_vec params() const;
    /**
     * @brief dump data statement info
     *
     * @param offs
     * @return std::string serialized data statement info
     */
    [[nodiscard]] std::string dump(size_t offs) const;
    /*! setters */
    void set_id(const str_t& id);     ///< set unique id of the statement
    void set_sql(const str_t& sql);   ///< set sql for specific database type
    void set_dscr(const str_t& dscr); ///< set description of the statement
  protected:
  private:
    [[nodiscard]] spdlog::logger* log() const;

    str_t    id_;      ///< unique id of the data statement
    str_t    sql_;     ///< sql statement (generic or specific for RDBMS)
    str_t    dscr_;    ///< statement description
    meta_vec columns_; ///< Result-set column metadata
    meta_vec params_;  ///< Input parameter metadata
  };

} // namespace dbgen4
