// db2_types.hpp
#pragma once
/**
 * @file
 * @brief translation between DB2 CLI (ODBC) type codes and the neutral rtl vocabulary
 *
 * This is the only place in the db2 backend that is allowed to know both
 * worlds. Everything above it speaks rtl::sql_type; sqlcli1.h stops here.
 */

#include "sql_types.hpp"
#include <sqlcli1.h>
#include <cstddef>

namespace rtl::db2
{
  using rtl::schema::sql_type; // NOLINT(misc-unused-using-decls) - used throughout this file's translation table

  // ------------------------------------------------------------------------
  // The neutral date/time structures are declared to be layout compatible
  // with the ODBC ones so that they can be bound by a plain cast. If a future
  // header or compiler ever breaks that promise, the build fails right here
  // instead of quietly handing the driver a misaligned buffer.
  // ------------------------------------------------------------------------
  // clang-format off
  static_assert(sizeof(rtl::date) == sizeof(DATE_STRUCT));
  static_assert(offsetof(rtl::date, year)  == offsetof(DATE_STRUCT, year));
  static_assert(offsetof(rtl::date, month) == offsetof(DATE_STRUCT, month));
  static_assert(offsetof(rtl::date, day)   == offsetof(DATE_STRUCT, day));

  static_assert(sizeof(rtl::time) == sizeof(TIME_STRUCT));
  static_assert(offsetof(rtl::time, hour)   == offsetof(TIME_STRUCT, hour));
  static_assert(offsetof(rtl::time, minute) == offsetof(TIME_STRUCT, minute));
  static_assert(offsetof(rtl::time, second) == offsetof(TIME_STRUCT, second));

  static_assert(sizeof(rtl::timestamp) == sizeof(TIMESTAMP_STRUCT));
  static_assert(offsetof(rtl::timestamp, year)     == offsetof(TIMESTAMP_STRUCT, year));
  static_assert(offsetof(rtl::timestamp, month)    == offsetof(TIMESTAMP_STRUCT, month));
  static_assert(offsetof(rtl::timestamp, day)      == offsetof(TIMESTAMP_STRUCT, day));
  static_assert(offsetof(rtl::timestamp, hour)     == offsetof(TIMESTAMP_STRUCT, hour));
  static_assert(offsetof(rtl::timestamp, minute)   == offsetof(TIMESTAMP_STRUCT, minute));
  static_assert(offsetof(rtl::timestamp, second)   == offsetof(TIMESTAMP_STRUCT, second));
  static_assert(offsetof(rtl::timestamp, fraction) == offsetof(TIMESTAMP_STRUCT, fraction));

  static_assert(sizeof(rtl::interval) == sizeof(SQL_INTERVAL_STRUCT));
  static_assert(offsetof(rtl::interval, kind)  == offsetof(SQL_INTERVAL_STRUCT, interval_type));
  static_assert(offsetof(rtl::interval, sign)  == offsetof(SQL_INTERVAL_STRUCT, interval_sign));
  static_assert(offsetof(rtl::interval, value) == offsetof(SQL_INTERVAL_STRUCT, intval));
  // clang-format on

  /// our null indicator must agree with the driver's
  static_assert(rtl::null_data == SQL_NULL_DATA);

  // Generated buffers keep their length/null indicators in int32_t arrays and
  // hand them straight to SQLBindCol/SQLBindParameter, which write through
  // SQLLEN*. SQLLEN is SQLINTEGER or sqlint64 depending on how sqlcli.h was
  // configured - if it ever becomes the wide one, the driver would write 8
  // bytes into 4 byte slots and silently walk off the end of the array.
  static_assert(sizeof(SQLLEN) == sizeof(int32_t),
                "SQLLEN is wider than the int32_t indicator arrays in generated buffers - "
                "widen rtl::buffer_dscr_init::indicator_ptr and the generated len_ arrays before continuing.");

  /**
   * @brief map a raw ODBC/DB2 sql type code to the neutral vocabulary
   *
   * Unrecognised codes become sql_type::unknown - the caller decides whether
   * that is fatal.
   *
   * @param code value reported by SQLDescribeCol / SQLDescribeParam
   * @return sql_type neutral type
   */
  [[nodiscard]] constexpr sql_type from_odbc(SQLSMALLINT code) noexcept
  {
    switch (code)
    {
    // --- atomic ---
    case SQL_INTEGER: return sql_type::integer;
    case SQL_SMALLINT: return sql_type::smallint;
    case SQL_BIGINT: return sql_type::bigint;
    case SQL_TINYINT: return sql_type::tiny_int;
    case SQL_FLOAT: return sql_type::float_;
    case SQL_REAL: return sql_type::real;
    case SQL_DOUBLE: return sql_type::double_;
    case SQL_BIT: return sql_type::bit;
    // --- 8 bit strings ---
    case SQL_CHAR: return sql_type::char_;
    case SQL_NUMERIC: return sql_type::numeric;
    case SQL_DECIMAL: return sql_type::decimal;
    case SQL_VARCHAR: return sql_type::var_char;
    case SQL_DECFLOAT: return sql_type::decfloat;
    case SQL_LONGVARCHAR: return sql_type::long_var_char;
    case SQL_CLOB: return sql_type::clob;
    case SQL_XML: return sql_type::xml;
    // --- 16 bit strings ---
    case SQL_WCHAR: return sql_type::wchar;
    case SQL_WVARCHAR: return sql_type::wvar_char;
    case SQL_WLONGVARCHAR: return sql_type::wlong_var_char;
    case SQL_DBCLOB: return sql_type::dbclob;
    // --- binary strings ---
    case SQL_GRAPHIC: return sql_type::graphic;
    case SQL_VARGRAPHIC: return sql_type::var_graphic;
    case SQL_BINARY: return sql_type::binary;
    case SQL_VARBINARY: return sql_type::var_binary;
    case SQL_LONGVARBINARY: return sql_type::long_var_binary;
    case SQL_BLOB: return sql_type::blob;
    // --- date / time ---
    // SQL_DATE and SQL_DATETIME share the value 9; SQL_TIME is 10, SQL_TIMESTAMP 11
    case SQL_DATE: return sql_type::date;
    case SQL_TIME: return sql_type::time;
    case SQL_TIMESTAMP: return sql_type::timestamp;
    case SQL_TYPE_DATE: return sql_type::type_date;
    case SQL_TYPE_TIME: return sql_type::type_time;
    case SQL_TYPE_TIMESTAMP: return sql_type::type_timestamp;
    // --- intervals ---
    case SQL_INTERVAL_YEAR: return sql_type::interval_year;
    case SQL_INTERVAL_MONTH: return sql_type::interval_month;
    case SQL_INTERVAL_YEAR_TO_MONTH: return sql_type::interval_year_to_month;
    case SQL_INTERVAL_DAY: return sql_type::interval_day;
    case SQL_INTERVAL_HOUR: return sql_type::interval_hour;
    case SQL_INTERVAL_MINUTE: return sql_type::interval_minute;
    case SQL_INTERVAL_SECOND: return sql_type::interval_second;
    case SQL_INTERVAL_DAY_TO_HOUR: return sql_type::interval_day_to_hour;
    case SQL_INTERVAL_DAY_TO_MINUTE: return sql_type::interval_day_to_minute;
    case SQL_INTERVAL_DAY_TO_SECOND: return sql_type::interval_day_to_second;
    case SQL_INTERVAL_HOUR_TO_MINUTE: return sql_type::interval_hour_to_minute;
    case SQL_INTERVAL_HOUR_TO_SECOND: return sql_type::interval_hour_to_second;
    case SQL_INTERVAL_MINUTE_TO_SECOND: return sql_type::interval_minute_to_second;
    // --- misc ---
    case SQL_GUID: return sql_type::guid;
    default: return sql_type::unknown;
    }
  }

  /**
   * @brief map a neutral type back to the ODBC sql type code
   *
   * Used when binding a parameter - SQLBindParameter wants the sql side type.
   *
   * @param type neutral type
   * @return SQLSMALLINT ODBC sql type code
   */
  [[nodiscard]] constexpr SQLSMALLINT to_odbc(sql_type type) noexcept
  {
    switch (type)
    {
    // --- atomic ---
    case sql_type::integer: return SQL_INTEGER;
    case sql_type::smallint: return SQL_SMALLINT;
    case sql_type::bigint: return SQL_BIGINT;
    case sql_type::tiny_int: return SQL_TINYINT;
    case sql_type::float_: return SQL_FLOAT;
    case sql_type::real: return SQL_REAL;
    case sql_type::double_: return SQL_DOUBLE;
    case sql_type::bit: return SQL_BIT;
    // --- 8 bit strings ---
    case sql_type::char_: return SQL_CHAR;
    case sql_type::numeric: return SQL_NUMERIC;
    case sql_type::decimal: return SQL_DECIMAL;
    case sql_type::var_char: return SQL_VARCHAR;
    case sql_type::decfloat: return SQL_DECFLOAT;
    case sql_type::long_var_char: return SQL_LONGVARCHAR;
    case sql_type::clob: return SQL_CLOB;
    case sql_type::xml: return SQL_XML;
    // --- 16 bit strings ---
    case sql_type::wchar: return SQL_WCHAR;
    case sql_type::wvar_char: return SQL_WVARCHAR;
    case sql_type::wlong_var_char: return SQL_WLONGVARCHAR;
    case sql_type::dbclob: return SQL_DBCLOB;
    // --- binary strings ---
    case sql_type::graphic: return SQL_GRAPHIC;
    case sql_type::var_graphic: return SQL_VARGRAPHIC;
    case sql_type::binary: return SQL_BINARY;
    case sql_type::var_binary: return SQL_VARBINARY;
    case sql_type::long_var_binary: return SQL_LONGVARBINARY;
    case sql_type::blob: return SQL_BLOB;
    // --- date / time ---
    case sql_type::date: return SQL_DATE;
    case sql_type::time: return SQL_TIME;
    case sql_type::timestamp: return SQL_TIMESTAMP;
    case sql_type::type_date: return SQL_TYPE_DATE;
    case sql_type::type_time: return SQL_TYPE_TIME;
    case sql_type::type_timestamp: return SQL_TYPE_TIMESTAMP;
    // --- intervals ---
    case sql_type::interval_year: return SQL_INTERVAL_YEAR;
    case sql_type::interval_month: return SQL_INTERVAL_MONTH;
    case sql_type::interval_year_to_month: return SQL_INTERVAL_YEAR_TO_MONTH;
    case sql_type::interval_day: return SQL_INTERVAL_DAY;
    case sql_type::interval_hour: return SQL_INTERVAL_HOUR;
    case sql_type::interval_minute: return SQL_INTERVAL_MINUTE;
    case sql_type::interval_second: return SQL_INTERVAL_SECOND;
    case sql_type::interval_day_to_hour: return SQL_INTERVAL_DAY_TO_HOUR;
    case sql_type::interval_day_to_minute: return SQL_INTERVAL_DAY_TO_MINUTE;
    case sql_type::interval_day_to_second: return SQL_INTERVAL_DAY_TO_SECOND;
    case sql_type::interval_hour_to_minute: return SQL_INTERVAL_HOUR_TO_MINUTE;
    case sql_type::interval_hour_to_second: return SQL_INTERVAL_HOUR_TO_SECOND;
    case sql_type::interval_minute_to_second: return SQL_INTERVAL_MINUTE_TO_SECOND;
    // --- misc ---
    case sql_type::guid: return SQL_GUID;
    /// count_ is the table width, never a column's type; it lands here with
    /// unknown because there is nothing else it could honestly map to.
    ///
    /// These repeat what default: does, and bugprone-branch-clone says so. They
    /// stay because -Wswitch-enum is -Werror in this build and wants every
    /// enumerator named outright - dropping them to satisfy the linter would
    /// break the compile.
    // NOLINTNEXTLINE(bugprone-branch-clone)
    case sql_type::unknown:
    case sql_type::count_: return SQL_UNKNOWN_TYPE;
    default: return SQL_UNKNOWN_TYPE;
    }
  }

  /**
   * @brief the ODBC C type code a neutral type is bound as
   *
   * This is the SQL_C_* value handed to SQLBindCol / SQLBindParameter. It
   * follows directly from the storage category, which is why the generated
   * buffer description does not need to carry it.
   *
   * @param type neutral type
   * @return SQLSMALLINT ODBC C type code
   */
  [[nodiscard]] constexpr SQLSMALLINT to_odbc_c(sql_type type) noexcept
  {
    switch (type)
    {
    // --- atomic ---
    case sql_type::integer: return SQL_C_SLONG;
    case sql_type::smallint: return SQL_C_SHORT;
    case sql_type::bigint: return SQL_C_SBIGINT;
    case sql_type::tiny_int: return SQL_C_STINYINT;
    case sql_type::float_: return SQL_C_DOUBLE;
    case sql_type::real: return SQL_C_FLOAT;
    case sql_type::double_: return SQL_C_DOUBLE;
    case sql_type::bit: return SQL_C_BIT;
    // --- 8 bit strings: numeric and decimal travel as text on purpose ---
    case sql_type::char_:
    case sql_type::numeric:
    case sql_type::decimal:
    case sql_type::var_char:
    case sql_type::decfloat:
    case sql_type::long_var_char:
    case sql_type::clob:
    case sql_type::xml: return SQL_C_CHAR;
    // --- 16 bit strings ---
    case sql_type::wchar:
    case sql_type::wvar_char:
    case sql_type::wlong_var_char:
    case sql_type::dbclob: return SQL_C_WCHAR;
    // --- binary strings ---
    case sql_type::graphic:
    case sql_type::var_graphic:
    case sql_type::binary:
    case sql_type::var_binary:
    case sql_type::long_var_binary:
    case sql_type::blob: return SQL_C_BINARY;
    // --- date / time ---
    case sql_type::date:
    case sql_type::type_date: return SQL_C_TYPE_DATE;
    case sql_type::time:
    case sql_type::type_time: return SQL_C_TYPE_TIME;
    case sql_type::timestamp:
    case sql_type::type_timestamp: return SQL_C_TYPE_TIMESTAMP;
    // --- intervals ---
    case sql_type::interval_year: return SQL_C_INTERVAL_YEAR;
    case sql_type::interval_month: return SQL_C_INTERVAL_MONTH;
    case sql_type::interval_year_to_month: return SQL_C_INTERVAL_YEAR_TO_MONTH;
    case sql_type::interval_day: return SQL_C_INTERVAL_DAY;
    case sql_type::interval_hour: return SQL_C_INTERVAL_HOUR;
    case sql_type::interval_minute: return SQL_C_INTERVAL_MINUTE;
    case sql_type::interval_second: return SQL_C_INTERVAL_SECOND;
    case sql_type::interval_day_to_hour: return SQL_C_INTERVAL_DAY_TO_HOUR;
    case sql_type::interval_day_to_minute: return SQL_C_INTERVAL_DAY_TO_MINUTE;
    case sql_type::interval_day_to_second: return SQL_C_INTERVAL_DAY_TO_SECOND;
    case sql_type::interval_hour_to_minute: return SQL_C_INTERVAL_HOUR_TO_MINUTE;
    case sql_type::interval_hour_to_second: return SQL_C_INTERVAL_HOUR_TO_SECOND;
    case sql_type::interval_minute_to_second: return SQL_C_INTERVAL_MINUTE_TO_SECOND;
    // --- misc ---
    case sql_type::guid: return SQL_C_GUID;
    /// see to_odbc() - count_ is a sentinel, not a type, and these branches
    /// duplicate default: on purpose so that -Wswitch-enum stays satisfied
    // NOLINTNEXTLINE(bugprone-branch-clone)
    case sql_type::unknown:
    case sql_type::count_: return SQL_C_DEFAULT;
    default: return SQL_C_DEFAULT;
    }
  }

} // namespace rtl::db2
