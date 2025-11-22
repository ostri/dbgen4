// buffer_dscr.h
#pragma once
#include "cli_constants.hpp"
#include "common.hpp"
#include <span>
#include <sqlcli1.h>
// #include <sql.h>
// #include <sqlext.h>
// #include <span>
// #include <array>

struct buffer_dscr_const
{
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  std::string_view name;           // name of the result/param column
  rtl::sql_cat     category;       // column category atomic, structure, ...
  std::string_view base_type;      // base C++ type
  SQLSMALLINT      value_type;     // SQL C type (code)
  SQLSMALLINT      param_type;     // SQL DB type (code)
  SQLULEN          column_size;    // column width in bytes
  SQLSMALLINT      decimal_digits; // decimal digits; for decimal only
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  constexpr buffer_dscr_const(std::string_view cn,  /// column name
                              rtl::sql_cat     cat, /// category
                              std::string_view bt,  /// base type
                              SQLSMALLINT      vt,  /// c type
                              SQLSMALLINT      pt,  /// sql type
                              SQLULEN          cs,  /// length
                              SQLSMALLINT      dd) noexcept
  : name(cn)
  , category(cat)
  , base_type(bt)
  , value_type(vt)
  , param_type(pt)
  , column_size(cs)
  , decimal_digits(dd)
  {
  }
} __attribute__((aligned(64))); // NOLINT
using span_buffer_dscr_const = std::span<const buffer_dscr_const>;
struct buffer_dscr_init
{
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  void*   value_ptr;
  SQLLEN* indicator_ptr;
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  constexpr buffer_dscr_init(void* vp, SQLLEN* ip) noexcept
  : value_ptr(vp)
  , indicator_ptr(ip)
  {
  }
} __attribute__((aligned(16))); // NOLINT
using span_buffer_dscr_init = std::span<const buffer_dscr_init>;
