#pragma once
#include <cstdint>
#include <string_view>
#include <sqlcli1.h>

namespace rtl
{

  /**
   * @brief Type-safe enumeration (enum class) for all standard ODBC/CLI SQL Data Types.
   * * Each enumerator is assigned its value via the standard SQL_* mnemonic defined
   * in the ODBC/CLI headers, ensuring direct correlation.
   */
  enum class sql_type : std::int16_t
  {
    // === Core and Numeric Types ===
    unknown   = SQL_UNKNOWN_TYPE, // Default or error state
    char_     = SQL_CHAR,         //
    numeric   = SQL_NUMERIC,      //
    decimal   = SQL_DECIMAL,      //
    integer   = SQL_INTEGER,      //
    smallInt  = SQL_SMALLINT,     //
    float_    = SQL_FLOAT,        //
    real      = SQL_REAL,         //
    double_   = SQL_DOUBLE,       //
    date      = SQL_DATETIME,     //
    time      = SQL_TIME,         //
    timestamp = SQL_TIMESTAMP,    //
    var_char  = SQL_VARCHAR,      //
    decfloat  = SQL_DECFLOAT,     //

    // === Negative Codes (Extended Types) ===
    long_var_char   = SQL_LONGVARCHAR,
    binary          = SQL_BINARY,
    var_binary      = SQL_VARBINARY,
    long_var_binary = SQL_LONGVARBINARY,
    bigint          = SQL_BIGINT,
    tiny_int        = SQL_TINYINT,
    bit             = SQL_BIT,
    graphic         = SQL_GRAPHIC,
    var_graphic     = SQL_VARGRAPHIC,

    // Unicode Types (W = Wide/National)
    wchar          = SQL_WCHAR,
    wvar_char      = SQL_WVARCHAR,
    wlong_var_char = SQL_WLONGVARCHAR,
    guid           = SQL_GUID,

    // === ODBC 3.x Date/Time Codes (More precise types) ===
    type_date      = SQL_TYPE_DATE,
    type_time      = SQL_TYPE_TIME,
    type_timestamp = SQL_TYPE_TIMESTAMP,

    // === Interval Types (All possible codes 100-112) ===
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

    // === LOB (Large Object) Types ===

    blob   = SQL_BLOB,   //
    clob   = SQL_CLOB,   //
    dbclob = SQL_DBCLOB, //
    xml    = SQL_XML     //
  };

  // 1. Function to map the numeric code to a string mnemonic

  /**
   * @brief Maps an ODBC/CLI numeric constant (SQL_ type code) to its string mnemonic.
   * * This function handles the reverse mapping from the integer value back to the constant name.
   * * It uses std::string_view for zero-copy, compile-time string handling.
   * @param sql_type The integer value (e.g., 4 for SQL_INTEGER).
   * @return std::string_view The string representation (e.g., "SQL_INTEGER").
   */
  constexpr std::string_view get_sql_type_mnemonic(std::int16_t a_sql_type) noexcept
  {
    // Convert the integer input to the enum for use in the switch statement
    // This provides symbol safety and clarity.
    switch (static_cast<sql_type>(a_sql_type))
    {
    // Core and Numeric Types
    case sql_type::char_: return "SQL_CHAR";
    case sql_type::numeric: return "SQL_NUMERIC";
    case sql_type::decimal: return "SQL_DECIMAL";
    case sql_type::integer: return "SQL_INTEGER";
    case sql_type::smallInt: return "SQL_SMALLINT";
    case sql_type::float_: return "SQL_FLOAT";
    case sql_type::real: return "SQL_REAL";
    case sql_type::double_: return "SQL_DOUBLE";
    case sql_type::date: return "SQL_DATE";
    case sql_type::time: return "SQL_TIME";
    case sql_type::timestamp: return "SQL_TIMESTAMP";
    case sql_type::var_char: return "SQL_VARCHAR";
    case sql_type::decfloat: return "SQL_DECFLOAT";

    // Negative Codes (Extended Types)
    case sql_type::long_var_char: return "SQL_LONGVARCHAR";
    case sql_type::binary: return "SQL_BINARY";
    case sql_type::var_binary: return "SQL_VARBINARY";
    case sql_type::long_var_binary: return "SQL_LONGVARBINARY";
    case sql_type::bigint: return "SQL_BIGINT";
    case sql_type::tiny_int: return "SQL_TINYINT";
    case sql_type::bit: return "SQL_BIT";
    case sql_type::graphic: return "SQL_GRAPHIC";
    case sql_type::var_graphic: return "SQL_VARGRAPHIC";
    case sql_type::wchar: return "SQL_WCHAR";
    case sql_type::wvar_char: return "SQL_WVARCHAR";
    case sql_type::wlong_var_char: return "SQL_WLONGVARCHAR";
    case sql_type::guid: return "SQL_GUID";

    // ODBC 3.x Date/Time Codes
    case sql_type::type_date: return "SQL_TYPE_DATE";
    case sql_type::type_time: return "SQL_TYPE_TIME";
    case sql_type::type_timestamp: return "SQL_TYPE_TIMESTAMP";

    // Complete Interval Types
    case sql_type::interval_year: return "SQL_INTERVAL_YEAR";
    case sql_type::interval_month: return "SQL_INTERVAL_MONTH";
    case sql_type::interval_year_to_month: return "SQL_INTERVAL_YEAR_TO_MONTH";
    case sql_type::interval_day: return "SQL_INTERVAL_DAY";
    case sql_type::interval_hour: return "SQL_INTERVAL_HOUR";
    case sql_type::interval_minute: return "SQL_INTERVAL_MINUTE";
    case sql_type::interval_second: return "SQL_INTERVAL_SECOND";
    case sql_type::interval_day_to_hour: return "SQL_INTERVAL_DAY_TO_HOUR";
    case sql_type::interval_day_to_minute: return "SQL_INTERVAL_DAY_TO_MINUTE";
    case sql_type::interval_day_to_second: return "SQL_INTERVAL_DAY_TO_SECOND";
    case sql_type::interval_hour_to_minute: return "SQL_INTERVAL_HOUR_TO_MINUTE";
    case sql_type::interval_hour_to_second: return "SQL_INTERVAL_HOUR_TO_SECOND";
    case sql_type::interval_minute_to_second: return "SQL_INTERVAL_MINUTE_TO_SECOND";

    // LOB (Large Object) Types
    case sql_type::blob: return "SQL_BLOB";
    case sql_type::clob: return "SQL_CLOB";
    case sql_type::dbclob: return "SQL_DBCLOB";
    case sql_type::xml: return "SQL_XML";

    // Explicitly handle Unknown and all other unlisted or vendor-specific codes
    case sql_type::unknown:
    default:
      __builtin_unreachable();
      // return "SQL_UNKNOWN_TYPE";
    }
  }

  /**
   * @brief Maps the SqlDataType enum value to its ODBC/CLI string mnemonic.
   * * This is the preferred method for logging or displaying metadata information.
   * * It internally calls the code-based mapping function, ensuring consistency with the enum
   * definition.
   * @param type The SqlDataType enum value.
   * @return std::string_view The string representation (e.g., "SQL_INTEGER").
   */
  constexpr std::string_view get_sql_type_mnemonic(sql_type type) noexcept
  {
    // Cast the enum value back to its underlying integer type and reuse the code-based function.
    return get_sql_type_mnemonic(static_cast<std::int16_t>(type));
  }
} // namespace rtl