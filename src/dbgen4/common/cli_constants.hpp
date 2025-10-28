#pragma once
#include <cstdint>
#include <string_view>
#include <sqlcli1.h>

namespace dbgen4
{

  /**
   * @brief Type-safe enumeration (enum class) for all standard ODBC/CLI SQL Data Types.
   * * Each enumerator is assigned its value via the standard SQL_* mnemonic defined
   * in the ODBC/CLI headers, ensuring direct correlation.
   */
  enum class sql_data_type : std::int16_t
  {
    // === Core and Numeric Types ===
    Unknown   = 0, // Default or error state (Explicitly 0 as it's not a standard SQL_ type code)
    Char      = SQL_CHAR,
    Numeric   = SQL_NUMERIC,
    Decimal   = SQL_DECIMAL,
    Integer   = SQL_INTEGER,
    SmallInt  = SQL_SMALLINT,
    Float     = SQL_FLOAT,
    Real      = SQL_REAL,
    Double    = SQL_DOUBLE,
    Date      = SQL_DATE,
    Time      = SQL_TIME,
    Timestamp = SQL_TIMESTAMP,
    Varchar   = SQL_VARCHAR,

    // === Negative Codes (Extended Types) ===
    LongVarchar   = SQL_LONGVARCHAR,
    Binary        = SQL_BINARY,
    VarBinary     = SQL_VARBINARY,
    LongVarBinary = SQL_LONGVARBINARY,
    BigInt        = SQL_BIGINT,
    TinyInt       = SQL_TINYINT,
    Bit           = SQL_BIT,

    // Unicode Types (W = Wide/National)
    WChar        = SQL_WCHAR,
    WVarchar     = SQL_WVARCHAR,
    WLongVarchar = SQL_WLONGVARCHAR,
    Guid         = SQL_GUID,

    // === ODBC 3.x Date/Time Codes (More precise types) ===
    TypeDate      = SQL_TYPE_DATE,
    TypeTime      = SQL_TYPE_TIME,
    TypeTimestamp = SQL_TYPE_TIMESTAMP,

    // === Interval Types (All possible codes 100-112) ===
    IntervalYear           = SQL_INTERVAL_YEAR,
    IntervalMonth          = SQL_INTERVAL_MONTH,
    IntervalYearToMonth    = SQL_INTERVAL_YEAR_TO_MONTH,
    IntervalDay            = SQL_INTERVAL_DAY,
    IntervalHour           = SQL_INTERVAL_HOUR,
    IntervalMinute         = SQL_INTERVAL_MINUTE,
    IntervalSecond         = SQL_INTERVAL_SECOND,
    IntervalDayToHour      = SQL_INTERVAL_DAY_TO_HOUR,
    IntervalDayToMinute    = SQL_INTERVAL_DAY_TO_MINUTE,
    IntervalDayToSecond    = SQL_INTERVAL_DAY_TO_SECOND,
    IntervalHourToMinute   = SQL_INTERVAL_HOUR_TO_MINUTE,
    IntervalHourToSecond   = SQL_INTERVAL_HOUR_TO_SECOND,
    IntervalMinuteToSecond = SQL_INTERVAL_MINUTE_TO_SECOND,

    // === LOB (Large Object) Types ===

    Blob   = SQL_BLOB,
    Clob   = SQL_CLOB,
    DBCLOB = SQL_DBCLOB,
  };

  // 1. Function to map the numeric code to a string mnemonic

  /**
   * @brief Maps an ODBC/CLI numeric constant (SQL_ type code) to its string mnemonic.
   * * This function handles the reverse mapping from the integer value back to the constant name.
   * * It uses std::string_view for zero-copy, compile-time string handling.
   * @param sql_type The integer value (e.g., 4 for SQL_INTEGER).
   * @return std::string_view The string representation (e.g., "SQL_INTEGER").
   */
  constexpr std::string_view get_sql_type_mnemonic(std::int16_t sql_type) noexcept
  {
    // Convert the integer input to the enum for use in the switch statement
    // This provides symbol safety and clarity.
    switch (static_cast<sql_data_type>(sql_type))
    {
    // Core and Numeric Types
    case sql_data_type::Char: return "SQL_CHAR";
    case sql_data_type::Numeric: return "SQL_NUMERIC";
    case sql_data_type::Decimal: return "SQL_DECIMAL";
    case sql_data_type::Integer: return "SQL_INTEGER";
    case sql_data_type::SmallInt: return "SQL_SMALLINT";
    case sql_data_type::Float: return "SQL_FLOAT";
    case sql_data_type::Real: return "SQL_REAL";
    case sql_data_type::Double: return "SQL_DOUBLE";
    case sql_data_type::Date: return "SQL_DATE";
    case sql_data_type::Time: return "SQL_TIME";
    case sql_data_type::Timestamp: return "SQL_TIMESTAMP";
    case sql_data_type::Varchar: return "SQL_VARCHAR";

    // Negative Codes (Extended Types)
    case sql_data_type::LongVarchar: return "SQL_LONGVARCHAR";
    case sql_data_type::Binary: return "SQL_BINARY";
    case sql_data_type::VarBinary: return "SQL_VARBINARY";
    case sql_data_type::LongVarBinary: return "SQL_LONGVARBINARY";
    case sql_data_type::BigInt: return "SQL_BIGINT";
    case sql_data_type::TinyInt: return "SQL_TINYINT";
    case sql_data_type::Bit: return "SQL_BIT";
    case sql_data_type::WChar: return "SQL_WCHAR";
    case sql_data_type::WVarchar: return "SQL_WVARCHAR";
    case sql_data_type::WLongVarchar: return "SQL_WLONGVARCHAR";
    case sql_data_type::Guid: return "SQL_GUID";

    // ODBC 3.x Date/Time Codes
    case sql_data_type::TypeDate: return "SQL_TYPE_DATE";
    case sql_data_type::TypeTime: return "SQL_TYPE_TIME";
    case sql_data_type::TypeTimestamp: return "SQL_TYPE_TIMESTAMP";

    // Complete Interval Types
    case sql_data_type::IntervalYear: return "SQL_INTERVAL_YEAR";
    case sql_data_type::IntervalMonth: return "SQL_INTERVAL_MONTH";
    case sql_data_type::IntervalYearToMonth: return "SQL_INTERVAL_YEAR_TO_MONTH";
    case sql_data_type::IntervalDay: return "SQL_INTERVAL_DAY";
    case sql_data_type::IntervalHour: return "SQL_INTERVAL_HOUR";
    case sql_data_type::IntervalMinute: return "SQL_INTERVAL_MINUTE";
    case sql_data_type::IntervalSecond: return "SQL_INTERVAL_SECOND";
    case sql_data_type::IntervalDayToHour: return "SQL_INTERVAL_DAY_TO_HOUR";
    case sql_data_type::IntervalDayToMinute: return "SQL_INTERVAL_DAY_TO_MINUTE";
    case sql_data_type::IntervalDayToSecond: return "SQL_INTERVAL_DAY_TO_SECOND";
    case sql_data_type::IntervalHourToMinute: return "SQL_INTERVAL_HOUR_TO_MINUTE";
    case sql_data_type::IntervalHourToSecond: return "SQL_INTERVAL_HOUR_TO_SECOND";
    case sql_data_type::IntervalMinuteToSecond: return "SQL_INTERVAL_MINUTE_TO_SECOND";

    // LOB (Large Object) Types
    case sql_data_type::Blob: return "SQL_BLOB";
    case sql_data_type::Clob: return "SQL_CLOB";
    case sql_data_type::DBCLOB: return "SQL_DBCLOB";

    // Explicitly handle Unknown and all other unlisted or vendor-specific codes
    case sql_data_type::Unknown:
    default: return "SQL_UNKNOWN_TYPE";
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
  constexpr std::string_view get_sql_type_mnemonic(sql_data_type type) noexcept
  {
    // Cast the enum value back to its underlying integer type and reuse the code-based function.
    return get_sql_type_mnemonic(static_cast<std::int16_t>(type));
  }
} // namespace dbgen4