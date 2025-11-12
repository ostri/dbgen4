#pragma once
#include <cstdint>
#include <string_view>
#include <sqlcli1.h>
#include <map>
#include <variant>
#include <vector>

namespace rtl
{

  // === Enum-i (kot v tvojem header-ju) ===
  enum class sql_type : std::int16_t
  {
    unknown                   = SQL_UNKNOWN_TYPE,
    char_                     = SQL_CHAR,
    numeric                   = SQL_NUMERIC,
    decimal                   = SQL_DECIMAL,
    integer                   = SQL_INTEGER,
    smallInt                  = SQL_SMALLINT,
    float_                    = SQL_FLOAT,
    real                      = SQL_REAL,
    double_                   = SQL_DOUBLE,
    date                      = SQL_DATETIME,
    time                      = SQL_TIME,
    timestamp                 = SQL_TIMESTAMP,
    var_char                  = SQL_VARCHAR,
    decfloat                  = SQL_DECFLOAT,
    long_var_char             = SQL_LONGVARCHAR,
    binary                    = SQL_BINARY,
    var_binary                = SQL_VARBINARY,
    long_var_binary           = SQL_LONGVARBINARY,
    bigint                    = SQL_BIGINT,
    tiny_int                  = SQL_TINYINT,
    bit                       = SQL_BIT,
    graphic                   = SQL_GRAPHIC,
    var_graphic               = SQL_VARGRAPHIC,
    wchar                     = SQL_WCHAR,
    wvar_char                 = SQL_WVARCHAR,
    wlong_var_char            = SQL_WLONGVARCHAR,
    guid                      = SQL_GUID,
    type_date                 = SQL_TYPE_DATE,
    type_time                 = SQL_TYPE_TIME,
    type_timestamp            = SQL_TYPE_TIMESTAMP,
    interval_year             = SQL_INTERVAL_YEAR,
    interval_month            = SQL_INTERVAL_MONTH,
    interval_year_to_month    = SQL_INTERVAL_YEAR_TO_MONTH,
    interval_day              = SQL_INTERVAL_DAY,
    interval_hour             = SQL_INTERVAL_HOUR,
    interval_minute           = SQL_INTERVAL_MINUTE,
    interval_second           = SQL_INTERVAL_SECOND,
    interval_day_to_hour      = SQL_INTERVAL_DAY_TO_HOUR,
    interval_day_to_minute    = SQL_INTERVAL_DAY_TO_MINUTE,
    interval_day_to_second    = SQL_INTERVAL_DAY_TO_SECOND,
    interval_hour_to_minute   = SQL_INTERVAL_HOUR_TO_MINUTE,
    interval_hour_to_second   = SQL_INTERVAL_HOUR_TO_SECOND,
    interval_minute_to_second = SQL_INTERVAL_MINUTE_TO_SECOND,
    blob                      = SQL_BLOB,
    clob                      = SQL_CLOB,
    dbclob                    = SQL_DBCLOB,
    xml                       = SQL_XML
  };

  enum class c_sql_type : std::int16_t // NOLINT
  {
    c_char                      = SQL_C_CHAR,
    c_wchar                     = SQL_C_WCHAR,
    c_slong                     = SQL_C_SLONG,
    c_ulong                     = SQL_C_ULONG,
    c_short                     = SQL_C_SHORT,
    c_sshort                    = SQL_C_SSHORT,
    c_ushort                    = SQL_C_USHORT,
    c_long                      = SQL_C_LONG,
    c_sbigint                   = SQL_C_SBIGINT,
    c_ubigint                   = SQL_C_UBIGINT,
    c_float                     = SQL_C_FLOAT,
    c_double                    = SQL_C_DOUBLE,
    c_bit                       = SQL_C_BIT,
    c_stinyint                  = SQL_C_STINYINT,
    c_utinyint                  = SQL_C_UTINYINT,
    c_binary                    = SQL_C_BINARY,
    c_type_date                 = SQL_C_TYPE_DATE,
    c_type_time                 = SQL_C_TYPE_TIME,
    c_type_timestamp            = SQL_C_TYPE_TIMESTAMP,
    c_interval_year             = SQL_C_INTERVAL_YEAR,
    c_interval_month            = SQL_C_INTERVAL_MONTH,
    c_interval_year_to_month    = SQL_C_INTERVAL_YEAR_TO_MONTH,
    c_interval_day              = SQL_C_INTERVAL_DAY,
    c_interval_hour             = SQL_C_INTERVAL_HOUR,
    c_interval_minute           = SQL_C_INTERVAL_MINUTE,
    c_interval_second           = SQL_C_INTERVAL_SECOND,
    c_interval_day_to_hour      = SQL_C_INTERVAL_DAY_TO_HOUR,
    c_interval_day_to_minute    = SQL_C_INTERVAL_DAY_TO_MINUTE,
    c_interval_day_to_second    = SQL_C_INTERVAL_DAY_TO_SECOND,
    c_interval_hour_to_minute   = SQL_C_INTERVAL_HOUR_TO_MINUTE,
    c_interval_hour_to_second   = SQL_C_INTERVAL_HOUR_TO_SECOND,
    c_interval_minute_to_second = SQL_C_INTERVAL_MINUTE_TO_SECOND,
    c_guid                      = SQL_C_GUID,
    c_default                   = SQL_C_DEFAULT,
    c_unknown                   = SQL_UNKNOWN_TYPE
  };

  enum class sql_cat : std::uint8_t
  {
    atomic,
    c_string,
    w_string,
    b_string,
    structure
  };

  // === ODBC strukture ===
  // struct SQL_DATE_STRUCT
  // {
  //   SQLSMALLINT  year;
  //   SQLUSMALLINT month, day;
  // } __attribute__((aligned(8))); // NOLINT
  // struct SQL_TIME_STRUCT
  // {
  //   SQLUSMALLINT hour, minute, second;
  // } __attribute__((aligned(8))); // NOLINT
  // struct SQL_TIMESTAMP_STRUCT
  // {
  //   SQLSMALLINT year, month, day, hour, minute, second;
  //   SQLUINTEGER fraction;
  // } __attribute__((aligned(16))); // NOLINT
  struct SQL_GUID_
  {
    SQLUINTEGER Data1;
    SQLSMALLINT Data2, Data3;
    SQLUINTEGER Data4[2];         // NOLINT
  } __attribute__((aligned(16))); // NOLINT

  // === C++ atomarni tipi (za variant) ===
  using cpp_atomic_t =
    std::variant<std::int8_t, std::int16_t, std::int32_t, std::int64_t, float, double, bool>;

  // === String view-i ===
  using string_view_char  = std::basic_string_view<char>;
  using string_view_wchar = std::basic_string_view<wchar_t>;
  using string_view_uint8 = std::basic_string_view<uint8_t>;

  using cstr_t  = string_view_char;
  using wcstr_t = string_view_wchar;
  using bcstr_t = string_view_uint8;

  using str_t  = std::string;
  using wstr_t = std::wstring;
  using bstr_t = std::vector<uint8_t>;

  // === Glavna struktura z vsemi podatki ===
  struct sql_mapping
  {
    sql_type   sql;                /// primary key (odbc type)
    c_sql_type c_type;             /// C type mapping (enum)
    cstr_t     c_mnemonic;         /// C type mapping (string)
    sql_cat    category;           /// category of the db/c++ type
    cstr_t     cpp_type_name;      /// storage mapping
    cstr_t     par_type_name;      /// parameter type mapping
    cstr_t     ret_type_name;      /// result type mapping
  } __attribute__((aligned(128))); // NOLINT
  // clang-format off
// === Runtime-konstantna mapa (inicializirana enkrat) ===
[[nodiscard]] inline const std::map<sql_type, sql_mapping>& get_sql_to_cpp_map() noexcept
{
    static const auto map = []() -> std::map<sql_type, sql_mapping> {
        std::map<sql_type, sql_mapping> m;

        auto add = [&](sql_type s, c_sql_type c, std::string_view mn,
                       sql_cat cat, std::string_view cpp_name, cstr_t par_name, cstr_t ret_name) {
            m[s] = sql_mapping{s, c, mn, cat, cpp_name, par_name, ret_name}; //NOLINT
        };

        // === 1. Atomarni tipi ===
        add(sql_type::integer,    c_sql_type::c_slong,     "SQL_C_SLONG",     sql_cat::atomic, "int32_t", "int32_t", "int32_t");
        add(sql_type::smallInt,   c_sql_type::c_short,     "SQL_C_SHORT",     sql_cat::atomic, "int16_t", "int16_t", "int16_t");
        add(sql_type::bigint,     c_sql_type::c_sbigint,   "SQL_C_SBIGINT",   sql_cat::atomic, "int64_t", "int64_t", "int64_t");
        add(sql_type::tiny_int,   c_sql_type::c_stinyint,  "SQL_C_STINYINT",  sql_cat::atomic, "int8_t",  "int8_t",  "int8_t");
        add(sql_type::float_,     c_sql_type::c_double,    "SQL_C_DOUBLE",    sql_cat::atomic, "double",  "double",  "double");
        add(sql_type::real,       c_sql_type::c_float,     "SQL_C_FLOAT",     sql_cat::atomic, "float",   "float",   "float");
        add(sql_type::double_,    c_sql_type::c_double,    "SQL_C_DOUBLE",    sql_cat::atomic, "double",  "double",  "double");
        add(sql_type::bit,        c_sql_type::c_bit,       "SQL_C_BIT",       sql_cat::atomic, "bool",    "bool",    "bool");

        // === 2. 8-bit stringi (SQL_C_CHAR) ===
        add(sql_type::char_,           c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::numeric,         c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::decimal,         c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::var_char,        c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::decfloat,        c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::long_var_char,   c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::clob,            c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");
        add(sql_type::xml,             c_sql_type::c_char, "SQL_C_CHAR", sql_cat::c_string, "char", "cstr_t", "cstr_t");

        // === 3. 16-bit stringi (SQL_C_WCHAR) ===
        add(sql_type::wchar,           c_sql_type::c_wchar, "SQL_C_WCHAR", sql_cat::w_string, "wchar_t", "wcstr_t", "wcstr_t");
        add(sql_type::wvar_char,       c_sql_type::c_wchar, "SQL_C_WCHAR", sql_cat::w_string, "wchar_t", "wcstr_t", "wcstr_t");
        add(sql_type::wlong_var_char,  c_sql_type::c_wchar, "SQL_C_WCHAR", sql_cat::w_string, "wchar_t", "wcstr_t", "wcstr_t");
        add(sql_type::graphic,         c_sql_type::c_wchar, "SQL_C_WCHAR", sql_cat::w_string, "wchar_t", "wcstr_t", "wcstr_t");
        add(sql_type::var_graphic,     c_sql_type::c_wchar, "SQL_C_WCHAR", sql_cat::w_string, "wchar_t", "wcstr_t", "wcstr_t");
        add(sql_type::dbclob,          c_sql_type::c_wchar, "SQL_C_WCHAR", sql_cat::w_string, "wchar_t", "wcstr_t", "wcstr_t");

        // === 4. Binarno (SQL_C_BINARY) ===
        add(sql_type::binary,          c_sql_type::c_binary, "SQL_C_BINARY", sql_cat::b_string, "uint8_t", "bcstr_t", "bcstr_t");
        add(sql_type::var_binary,      c_sql_type::c_binary, "SQL_C_BINARY", sql_cat::b_string, "uint8_t", "bcstr_t", "bcstr_t");
        add(sql_type::long_var_binary, c_sql_type::c_binary, "SQL_C_BINARY", sql_cat::b_string, "uint8_t", "bcstr_t", "bcstr_t");
        add(sql_type::blob,            c_sql_type::c_binary, "SQL_C_BINARY", sql_cat::b_string, "uint8_t", "bcstr_t", "bcstr_t");

        // === 5. Strukture ===
        // Date/Time
        add(sql_type::date,            c_sql_type::c_type_date,      "SQL_C_TYPE_DATE",      sql_cat::structure, "SQL_DATE_STRUCT",      "SQL_DATE_STRUCT",      "SQL_DATE_STRUCT");
        add(sql_type::time,            c_sql_type::c_type_time,      "SQL_C_TYPE_TIME",      sql_cat::structure, "SQL_TIME_STRUCT",      "SQL_TIME_STRUCT",      "SQL_TIME_STRUCT");
        add(sql_type::timestamp,       c_sql_type::c_type_timestamp, "SQL_C_TYPE_TIMESTAMP", sql_cat::structure, "SQL_TIMESTAMP_STRUCT", "SQL_TIMESTAMP_STRUCT", "SQL_TIMESTAMP_STRUCT");
        add(sql_type::type_date,       c_sql_type::c_type_date,      "SQL_C_TYPE_DATE",      sql_cat::structure, "SQL_DATE_STRUCT",      "SQL_DATE_STRUCT",      "SQL_DATE_STRUCT");
        add(sql_type::type_time,       c_sql_type::c_type_time,      "SQL_C_TYPE_TIME",      sql_cat::structure, "SQL_TIME_STRUCT",      "SQL_TIME_STRUCT",      "SQL_TIME_STRUCT");
        add(sql_type::type_timestamp,  c_sql_type::c_type_timestamp, "SQL_C_TYPE_TIMESTAMP", sql_cat::structure, "SQL_TIMESTAMP_STRUCT", "SQL_TIMESTAMP_STRUCT", "SQL_TIMESTAMP_STRUCT");

        // Interval
        add(sql_type::interval_year,             c_sql_type::c_interval_year,             "SQL_C_INTERVAL_YEAR",             sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_month,            c_sql_type::c_interval_month,            "SQL_C_INTERVAL_MONTH",            sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_year_to_month,    c_sql_type::c_interval_year_to_month,    "SQL_C_INTERVAL_YEAR_TO_MONTH",    sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_day,              c_sql_type::c_interval_day,              "SQL_C_INTERVAL_DAY",              sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_hour,             c_sql_type::c_interval_hour,             "SQL_C_INTERVAL_HOUR",             sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_minute,           c_sql_type::c_interval_minute,           "SQL_C_INTERVAL_MINUTE",           sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_second,           c_sql_type::c_interval_second,           "SQL_C_INTERVAL_SECOND",           sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_day_to_hour,      c_sql_type::c_interval_day_to_hour,      "SQL_C_INTERVAL_DAY_TO_HOUR",      sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_day_to_minute,    c_sql_type::c_interval_day_to_minute,    "SQL_C_INTERVAL_DAY_TO_MINUTE",    sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_day_to_second,    c_sql_type::c_interval_day_to_second,    "SQL_C_INTERVAL_DAY_TO_SECOND",    sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_hour_to_minute,   c_sql_type::c_interval_hour_to_minute,   "SQL_C_INTERVAL_HOUR_TO_MINUTE",   sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_hour_to_second,   c_sql_type::c_interval_hour_to_second,   "SQL_C_INTERVAL_HOUR_TO_SECOND",   sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");
        add(sql_type::interval_minute_to_second, c_sql_type::c_interval_minute_to_second, "SQL_C_INTERVAL_MINUTE_TO_SECOND", sql_cat::structure, "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT", "SQL_INTERVAL_STRUCT");

        // GUID
        add(sql_type::guid, c_sql_type::c_guid, "SQL_C_GUID", sql_cat::structure, "SQLGUID", "SQLGUID", "SQLGUID");

        // UNKNOWN (fallback)
        add(sql_type::unknown, c_sql_type::c_unknown, "SQL_C_UNKNOWN", sql_cat::structure, "void", "void", "void");

        return m;
    }();

    return map;
}
  // clang-format on
  // === Funkcija za dostop ===
  // NOLINTNEXTLINE(misc-no-recursion)
  [[nodiscard]] inline const sql_mapping* get_sql_mapping(sql_type type) noexcept
  {
    const auto& map = get_sql_to_cpp_map();
    auto        it  = map.find(type);
    return (it != map.end()) ? &it->second : get_sql_mapping(sql_type::unknown);
  }

} // namespace rtl