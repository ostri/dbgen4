#pragma once
#include "cli_constants.hpp"
#include "common.hpp"
#include <cstdint>
#include <vector>

namespace dbgen4
{
  /**
   * @brief data about one colum/parameter definition in sql statement
   *
   */
  class sql_col_def
  {
  public:
    sql_col_def() = default;
    sql_col_def(uint16_t pos, const cstr_t& name, rtl::sql_data_type type, bool nullable)
    : position_(pos)
    , name_(name)
    , type_(type)
    , nullable_(nullable)
    {
    }
    ~sql_col_def() = default;

    sql_col_def(const sql_col_def& o)              = default;
    sql_col_def(sql_col_def&&) noexcept            = default;
    sql_col_def& operator=(const sql_col_def& o)   = default;
    sql_col_def& operator=(sql_col_def&&) noexcept = delete;
    /// getters
    [[nodiscard]] uint16_t           position() const;
    [[nodiscard]] str_t              name() const;
    [[nodiscard]] rtl::sql_data_type type() const;
    [[nodiscard]] bool               nullable() const;
    /// setters
    void setPosition(const uint16_t& position);
    void set_name(const str_t& name);
    void set_type(const rtl::sql_data_type& type);
    void is_nullable(bool nullable);
  protected:
    // NOLINTNEXTLINE(cert-err58-cpp)
    inline static const auto log = log::get();
  private:
    uint16_t position_{0};      ///< position of the column in the select list or parameter list
    str_t    name_;             ///< name of the column/parameter
    rtl::sql_data_type type_{}; ///< data type of the column/parameter
    bool               nullable_{true}; ///< is the column/parameter nullable or not
  };

  using sql_col_def_vec_t = std::vector<sql_col_def>;
}; // namespace dbgen4