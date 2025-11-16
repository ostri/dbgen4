#pragma once

#include <span>
#include <string_view>
#include <array>
#include <typeinfo>
#include <optional>
#include <map>
#include <cstdint>

// === 1. Enum definicije ===
enum class sql_type : int
{
  integer,
  smallInt,
  bigint,
  tiny_int,
  float_,
  real,
  double_,
  bit,
  char_,
  numeric,
  decimal,
  var_char,
  decfloat,
  long_var_char,
  clob,
  xml,
  wchar,
  wvar_char,
  wlong_var_char,
  graphic,
  var_graphic,
  dbclob,
  binary,
  var_binary,
  long_var_binary,
  blob,
  date,
  time,
  timestamp,
  type_date,
  type_time,
  type_timestamp,
  interval_year,
  interval_month,
  interval_year_to_month,
  interval_day,
  interval_hour,
  interval_minute,
  interval_second,
  interval_day_to_hour,
  interval_day_to_minute,
  interval_day_to_second,
  interval_hour_to_minute,
  interval_hour_to_second,
  interval_minute_to_second,
  guid,
  unknown
};

enum class c_sql_type : int
{
  c_slong,
  c_short,
  c_sbigint,
  c_stinyint,
  c_double,
  c_float,
  c_bit,
  c_char,
  c_wchar,
  c_binary,
  c_type_date,
  c_type_time,
  c_type_timestamp,
  c_interval_year,
  c_interval_month,
  c_interval_year_to_month,
  c_interval_day,
  c_interval_hour,
  c_interval_minute,
  c_interval_second,
  c_interval_day_to_hour,
  c_interval_day_to_minute,
  c_interval_day_to_second,
  c_interval_hour_to_minute,
  c_interval_hour_to_second,
  c_interval_minute_to_second,
  c_guid,
  c_unknown
};

enum class sql_cat
{
  atomic,
  c_string,
  w_string,
  b_string,
  structure
};

using cstr_t = const char*;

// === 2. Struktura ===
struct sql_mapping
{
  sql_type         sql;
  c_sql_type       c_type;
  std::string_view c_name;
  sql_cat          category;
  std::string_view cpp_type;
  cstr_t           param_type;
  cstr_t           return_type;
  std::string_view sql_odbc_name;
};

// === 3. Constexpr podatki ===
[[nodiscard]] constexpr std::span<const sql_mapping> get_sql_mappings_constexpr() noexcept
{
  using namespace std::literals;


  // clang-format off
static constexpr auto mappings = std::to_array<sql_mapping>({
  {sql_type::integer,                     c_sql_type::c_slong,             "SQL_C_SLONG"sv,             sql_cat::atomic,     "int32_t"sv,       "int32_t"sv,       "int32_t"sv,       "SQL_INTEGER"sv},
  {sql_type::smallInt,                    c_sql_type::c_short,             "SQL_C_SHORT"sv,             sql_cat::atomic,     "int16_t"sv,       "int16_t"sv,       "int16_t"sv,       "SQL_SMALLINT"sv},
  {sql_type::bigint,                      c_sql_type::c_sbigint,           "SQL_C_SBIGINT"sv,           sql_cat::atomic,     "int64_t"sv,       "int64_t"sv,       "int64_t"sv,       "SQL_BIGINT"sv},
  {sql_type::tiny_int,                    c_sql_type::c_stinyint,          "SQL_C_STINYINT"sv,          sql_cat::atomic,     "int8_t"sv,        "int8_t"sv,        "int8_t"sv,        "SQL_TINYINT"sv},
  {sql_type::float_,                      c_sql_type::c_double,            "SQL_C_DOUBLE"sv,            sql_cat::atomic,     "double"sv,        "double"sv,        "double"sv,        "SQL_FLOAT"sv},
  {sql_type::real,                        c_sql_type::c_float,             "SQL_C_FLOAT"sv,             sql_cat::atomic,     "float"sv,         "float"sv,         "float"sv,         "SQL_REAL"sv},
  {sql_type::double_,                     c_sql_type::c_double,            "SQL_C_DOUBLE"sv,            sql_cat::atomic,     "double"sv,        "double"sv,        "double"sv,        "SQL_DOUBLE"sv},
  {sql_type::bit,                         c_sql_type::c_bit,               "SQL_C_BIT"sv,               sql_cat::atomic,     "bool"sv,          "bool"sv,          "bool"sv,          "SQL_BIT"sv},

  {sql_type::char_,                       c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_CHAR"sv},
  {sql_type::numeric,                     c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_NUMERIC"sv},
  {sql_type::decimal,                     c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_DECIMAL"sv},
  {sql_type::var_char,                    c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_VARCHAR"sv},
  {sql_type::decfloat,                    c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_DECFLOAT"sv},
  {sql_type::long_var_char,               c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_LONGVARCHAR"sv},
  {sql_type::clob,                        c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_CLOB"sv},
  {sql_type::xml,                         c_sql_type::c_char,              "SQL_C_CHAR"sv,              sql_cat::c_string,   "char"sv,          "cstr_t"sv,        "cstr_t"sv,        "SQL_XML"sv},

  {sql_type::wchar,                       c_sql_type::c_wchar,             "SQL_C_WCHAR"sv,             sql_cat::w_string,   "wchar_t"sv,       "wcstr_t"sv,       "wcstr_t"sv,       "SQL_WCHAR"sv},
  {sql_type::wvar_char,                   c_sql_type::c_wchar,             "SQL_C_WCHAR"sv,             sql_cat::w_string,   "wchar_t"sv,       "wcstr_t"sv,       "wcstr_t"sv,       "SQL_WVARCHAR"sv},
  {sql_type::wlong_var_char,              c_sql_type::c_wchar,             "SQL_C_WCHAR"sv,             sql_cat::w_string,   "wchar_t"sv,       "wcstr_t"sv,       "wcstr_t"sv,       "SQL_WLONGVARCHAR"sv},
  {sql_type::graphic,                     c_sql_type::c_wchar,             "SQL_C_WCHAR"sv,             sql_cat::w_string,   "wchar_t"sv,       "wcstr_t"sv,       "wcstr_t"sv,       "SQL_GRAPHIC"sv},
  {sql_type::var_graphic,                 c_sql_type::c_wchar,             "SQL_C_WCHAR"sv,             sql_cat::w_string,   "wchar_t"sv,       "wcstr_t"sv,       "wcstr_t"sv,       "SQL_VARGRAPHIC"sv},
  {sql_type::dbclob,                      c_sql_type::c_wchar,             "SQL_C_WCHAR"sv,             sql_cat::w_string,   "wchar_t"sv,       "wcstr_t"sv,       "wcstr_t"sv,       "SQL_DBCLOB"sv},

  {sql_type::binary,                      c_sql_type::c_binary,            "SQL_C_BINARY"sv,            sql_cat::b_string,   "uint8_t"sv,       "bcstr_t"sv,       "bcstr_t"sv,       "SQL_BINARY"sv},
  {sql_type::var_binary,                  c_sql_type::c_binary,            "SQL_C_BINARY"sv,            sql_cat::b_string,   "uint8_t"sv,       "bcstr_t"sv,       "bcstr_t"sv,       "SQL_VARBINARY"sv},
  {sql_type::long_var_binary,             c_sql_type::c_binary,            "SQL_C_BINARY"sv,            sql_cat::b_string,   "uint8_t"sv,       "bcstr_t"sv,       "bcstr_t"sv,       "SQL_LONGVARBINARY"sv},
  {sql_type::blob,                        c_sql_type::c_binary,            "SQL_C_BINARY"sv,            sql_cat::b_string,   "uint8_t"sv,       "bcstr_t"sv,       "bcstr_t"sv,       "SQL_BLOB"sv},

  {sql_type::date,                        c_sql_type::c_type_date,         "SQL_C_TYPE_DATE"sv,         sql_cat::structure,  "SQL_DATE_STRUCT"sv,      "SQL_DATE_STRUCT"sv,      "SQL_DATE_STRUCT"sv,      "SQL_TYPE_DATE"sv},
  {sql_type::time,                        c_sql_type::c_type_time,         "SQL_C_TYPE_TIME"sv,         sql_cat::structure,  "SQL_TIME_STRUCT"sv,      "SQL_TIME_STRUCT"sv,      "SQL_TIME_STRUCT"sv,      "SQL_TYPE_TIME"sv},
  {sql_type::timestamp,                   c_sql_type::c_type_timestamp,    "SQL_C_TYPE_TIMESTAMP"sv,    sql_cat::structure,  "SQL_TIMESTAMP_STRUCT"sv, "SQL_TIMESTAMP_STRUCT"sv, "SQL_TIMESTAMP_STRUCT"sv, "SQL_TYPE_TIMESTAMP"sv},
  {sql_type::type_date,                   c_sql_type::c_type_date,         "SQL_C_TYPE_DATE"sv,         sql_cat::structure,  "SQL_DATE_STRUCT"sv,      "SQL_DATE_STRUCT"sv,      "SQL_DATE_STRUCT"sv,      "SQL_TYPE_DATE"sv},
  {sql_type::type_time,                   c_sql_type::c_type_time,         "SQL_C_TYPE_TIME"sv,         sql_cat::structure,  "SQL_TIME_STRUCT"sv,      "SQL_TIME_STRUCT"sv,      "SQL_TIME_STRUCT"sv,      "SQL_TYPE_TIME"sv},
  {sql_type::type_timestamp,              c_sql_type::c_type_timestamp,    "SQL_C_TYPE_TIMESTAMP"sv,    sql_cat::structure,  "SQL_TIMESTAMP_STRUCT"sv, "SQL_TIMESTAMP_STRUCT"sv, "SQL_TIMESTAMP_STRUCT"sv, "SQL_TYPE_TIMESTAMP"sv},

  {sql_type::interval_year,               c_sql_type::c_interval_year,     "SQL_C_INTERVAL_YEAR"sv,     sql_cat::structure,  "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_YEAR"sv},
  {sql_type::interval_month,              c_sql_type::c_interval_month,    "SQL_C_INTERVAL_MONTH"sv,    sql_cat::structure,  "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_MONTH"sv},
  {sql_type::interval_year_to_month,      c_sql_type::c_interval_year_to_month, "SQL_C_INTERVAL_YEAR_TO_MONTH"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_YEAR_TO_MONTH"sv},
  {sql_type::interval_day,                c_sql_type::c_interval_day,      "SQL_C_INTERVAL_DAY"sv,      sql_cat::structure,  "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_DAY"sv},
  {sql_type::interval_hour,               c_sql_type::c_interval_hour,     "SQL_C_INTERVAL_HOUR"sv,     sql_cat::structure,  "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_HOUR"sv},
  {sql_type::interval_minute,             c_sql_type::c_interval_minute,   "SQL_C_INTERVAL_MINUTE"sv,   sql_cat::structure,  "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_MINUTE"sv},
  {sql_type::interval_second,             c_sql_type::c_interval_second,   "SQL_C_INTERVAL_SECOND"sv,   sql_cat::structure,  "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_SECOND"sv},
  {sql_type::interval_day_to_hour,        c_sql_type::c_interval_day_to_hour, "SQL_C_INTERVAL_DAY_TO_HOUR"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_DAY_TO_HOUR"sv},
  {sql_type::interval_day_to_minute,      c_sql_type::c_interval_day_to_minute, "SQL_C_INTERVAL_DAY_TO_MINUTE"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_DAY_TO_MINUTE"sv},
  {sql_type::interval_day_to_second,      c_sql_type::c_interval_day_to_second, "SQL_C_INTERVAL_DAY_TO_SECOND"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_DAY_TO_SECOND"sv},
  {sql_type::interval_hour_to_minute,     c_sql_type::c_interval_hour_to_minute, "SQL_C_INTERVAL_HOUR_TO_MINUTE"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_HOUR_TO_MINUTE"sv},
  {sql_type::interval_hour_to_second,     c_sql_type::c_interval_hour_to_second, "SQL_C_INTERVAL_HOUR_TO_SECOND"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_HOUR_TO_SECOND"sv},
  {sql_type::interval_minute_to_second,   c_sql_type::c_interval_minute_to_second, "SQL_C_INTERVAL_MINUTE_TO_SECOND"sv, sql_cat::structure, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_STRUCT"sv, "SQL_INTERVAL_MINUTE_TO_SECOND"sv},

  {sql_type::guid,                        c_sql_type::c_guid,              "SQL_C_GUID"sv,              sql_cat::structure,  "SQLGUID"sv,       "SQLGUID"sv,       "SQLGUID"sv,       "SQL_GUID"sv},
  {sql_type::unknown,                     c_sql_type::c_unknown,           "SQL_C_UNKNOWN"sv,           sql_cat::structure,  "void"sv,          "void"sv,          "void"sv,          "SQL_UNKNOWN_TYPE"sv}
});
  // clang-format on

} // namespace rtl
// clang-format on
return std::span(mappings);
}