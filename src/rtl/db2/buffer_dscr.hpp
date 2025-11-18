// buffer_dscr.h
#pragma once
#include <span>
#include <sqlcli1.h>
// #include <sql.h>
// #include <sqlext.h>
// #include <span>
// #include <array>

struct buffer_dscr_const
{
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  SQLSMALLINT value_type;
  SQLSMALLINT param_type;
  SQLULEN     column_size;
  SQLSMALLINT decimal_digits;
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  constexpr buffer_dscr_const(SQLSMALLINT vt, SQLSMALLINT pt, SQLULEN cs, SQLSMALLINT dd) noexcept
  : value_type(vt)
  , param_type(pt)
  , column_size(cs)
  , decimal_digits(dd)
  {
  }
} __attribute__((aligned(16))); // NOLINT
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
