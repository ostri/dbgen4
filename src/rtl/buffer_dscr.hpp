// buffer_dscr.hpp
#pragma once
/**
 * @file
 * @brief description of a generated result/parameter buffer
 *
 * Backend neutral on purpose: the generated header must compile against any
 * backend. What a column *is* (rtl::sql_type) is generated; how it gets bound
 * (ODBC SQL_C_* code, PostgreSQL OID, ...) is decided by the backend at bind
 * time - see db2/db2_types.hpp.
 */

#include "sql_types.hpp"
#include <cstdint>
#include <span>
#include <string_view>

namespace rtl
{
  /**
   * @brief compile time description of one column/parameter in a buffer
   */
  struct buffer_dscr_const
  {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    std::string_view name;           ///< column/parameter name
    sql_cat          category;       ///< storage category
    std::string_view base_type;      ///< base C++ type, for diagnostics
    sql_type         type;           ///< neutral sql type
    uint32_t         column_size;    ///< column width in characters/bytes
    int16_t          decimal_digits; ///< decimal digits, for numeric/decimal
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    constexpr buffer_dscr_const(std::string_view cn,  /// column name
                                sql_cat          cat, /// category
                                std::string_view bt,  /// base type
                                sql_type         t,   /// neutral sql type
                                uint32_t         cs,  /// length
                                int16_t          dd) noexcept
    : name(cn)
    , category(cat)
    , base_type(bt)
    , type(t)
    , column_size(cs)
    , decimal_digits(dd)
    {
    }
  };
  using span_buffer_dscr_const = std::span<const buffer_dscr_const>;

  /**
   * @brief run time description - where the data of one column actually lives
   */
  struct buffer_dscr_init
  {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    void*    value_ptr;     ///< first element of the value array
    int32_t* indicator_ptr; ///< first element of the length/null indicator array
    /// distance in bytes between two rows of value_ptr
    ///
    /// The ODBC backend never needs this - it hands the whole array to the
    /// driver, which strides it itself. libpq returns values one at a time,
    /// so its runtime has to walk the array by hand.
    size_t stride;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    constexpr buffer_dscr_init(void* vp, int32_t* ip, size_t st) noexcept
    : value_ptr(vp)
    , indicator_ptr(ip)
    , stride(st)
    {
    }
  };
  using span_buffer_dscr_init = std::span<const buffer_dscr_init>;

} // namespace rtl
