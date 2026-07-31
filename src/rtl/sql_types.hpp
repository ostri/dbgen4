// sql_types.hpp
#pragma once
/**
 * @file
 * @brief backend neutral sql type vocabulary
 *
 * This header is the facade between the code generator and the concrete
 * database backends. It must not include any backend header (sqlcli1.h,
 * sql.h, libpq-fe.h, ...) - everything here is expressed in plain C++.
 *
 * Each backend provides its own translation unit that maps the native type
 * codes to sql_type and back (see db2/db2_types.hpp, psql/psql_types.hpp).
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rtl
{
  using cstr_t  = std::basic_string_view<char>;
  using wcstr_t = std::basic_string_view<wchar_t>;
  using bcstr_t = std::basic_string_view<uint8_t>;

  using str_t  = std::string;
  using wstr_t = std::wstring;
  using bstr_t = std::vector<uint8_t>;

  /// value of a length/indicator slot that marks a sql NULL
  constexpr int32_t null_data = -1;

  /**
   * @brief lowercase a string
   *
   * Column names arrive from the database in whatever case it stores them -
   * DB2 folds to upper - and end up as C++ identifiers, so they are folded
   * down here. Lives in rtl rather than in the generator: a backend must not
   * have to depend on the generator to do its own job.
   */
  [[nodiscard]] inline std::string lowercase(std::string_view input)
  {
    std::string out;
    out.resize(input.size());
    std::ranges::transform(input,
                           out.begin(),
                           [](unsigned char c) //
                           { return static_cast<char>(std::tolower(c)); });
    return out;
  }
  /**
   * @brief sql types known to the generator
   *
   * The values are our own and dense on purpose - they are deliberately not
   * the ODBC SQL_* codes, so that this header carries no backend dependency.
   * Backends translate between their native codes and these values.
   */
  enum class sql_type : std::int16_t // NOLINT(performance-enum-size)
  {
    unknown = 0,
    // atomic
    integer,
    smallint,
    bigint,
    tiny_int,
    float_,
    real,
    double_,
    bit,
    // 8 bit strings
    char_,
    numeric,
    decimal,
    var_char,
    decfloat,
    long_var_char,
    clob,
    xml,
    // 16 bit strings
    wchar,
    wvar_char,
    wlong_var_char,
    dbclob,
    // binary strings
    graphic,
    var_graphic,
    binary,
    var_binary,
    long_var_binary,
    blob,
    // date / time structures
    date,
    time,
    timestamp,
    type_date,
    type_time,
    type_timestamp,
    // interval structures
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
    // misc
    guid,
    /// not a type: one past the last, and so the width of any table keyed by
    /// sql_type. Every enumerator above it has to stay dense and unnumbered
    /// for that to hold - the static_assert below the table checks it does.
    count_
  };

  /**
   * @brief how a sql type is stored in a generated buffer
   *
   * The category drives the generated storage declaration, getter, setter and
   * dump code - see dbgen4::generator::attr_* methods.
   */
  enum class sql_cat : std::uint8_t
  {
    atomic,   ///< scalar value stored by value
    c_string, ///< char array, null terminated
    w_string, ///< wchar_t array, null terminated
    b_string, ///< uint8_t array, no terminator
    structure ///< aggregate (date, time, timestamp, interval, guid)
  };

  // ------------------------------------------------------------------------
  // date / time / guid structures
  //
  // Layout compatible with the ODBC SQL_*_STRUCT types on purpose, so that the
  // db2 backend can bind them by a plain cast. db2_types.hpp static_asserts
  // that this stays true - if a compiler ever disagrees, the build breaks
  // there rather than silently corrupting data.
  // ------------------------------------------------------------------------

  struct date
  {
    int16_t  year;  // NOLINT(misc-non-private-member-variables-in-classes)
    uint16_t month; // NOLINT(misc-non-private-member-variables-in-classes)
    uint16_t day;   // NOLINT(misc-non-private-member-variables-in-classes)

    /**
     * @brief compare two dates chronologically
     *
     * Defaulted, and that is enough: the members are declared most significant
     * first, so the member wise comparison the compiler writes is calendar
     * order. == and != come from the first line, < <= > >= from the second.
     *
     * Layout compatibility with DATE_STRUCT is unaffected - neither operator
     * adds a member or a vtable, and db2_types.hpp still static_asserts it.
     */
    [[nodiscard]] bool operator==(const date&) const noexcept  = default;
    [[nodiscard]] auto operator<=>(const date&) const noexcept = default;
  };

  struct time
  {
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
  };

  struct timestamp
  {
    int16_t  year;
    uint16_t month;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint32_t fraction; ///< nanoseconds
  };

  enum class interval_kind : uint32_t // NOLINT(performance-enum-size)
  {
    year = 1,
    month,
    day,
    hour,
    minute,
    second,
    year_to_month,
    day_to_hour,
    day_to_minute,
    day_to_second,
    hour_to_minute,
    hour_to_second,
    minute_to_second
  };

  struct year_month
  {
    uint32_t year;
    uint32_t month;
  };

  struct day_second
  {
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t fraction;
  };

  struct interval
  {
    interval_kind kind;
    int16_t       sign; ///< 1 positive, 0 negative
    union
    {
      year_month year_month_;
      day_second day_second_;
    } value;
  };

  struct guid
  {
    uint32_t               data1;
    uint16_t               data2;
    uint16_t               data3;
    std::array<uint8_t, 8> data4; // NOLINT(readability-magic-numbers)
  };

  /**
   * @brief everything the generator needs to know about one sql type
   */
  struct sql_mapping
  {
    sql_type sql;           ///< primary key
    cstr_t   mnemonic;      ///< readable name, ends up in generated comments
    sql_cat  category;      ///< storage category
    cstr_t   cpp_type_name; ///< storage type (element type for the string categories)
    cstr_t   par_type_name; ///< type accepted by a generated setter
    cstr_t   ret_type_name; ///< type returned by a generated getter
  };

  /// how many entries a table keyed by sql_type needs
  inline constexpr size_t sql_type_count = static_cast<size_t>(sql_type::count_);

  using sql_mapping_table = std::array<sql_mapping, sql_type_count>;

  /**
   * @brief build the sql type table at compile time
   *
   * @return consteval
   */
  [[nodiscard]] consteval sql_mapping_table make_sql_to_cpp_table() noexcept
  {
    sql_mapping_table m{};

    auto add = [&m](sql_type s, cstr_t mn, sql_cat cat, cstr_t cpp_name, cstr_t par_name, cstr_t ret_name) constexpr
    {
      m.at(static_cast<size_t>(s)) = sql_mapping //
        {.sql           = s,                     //
         .mnemonic      = mn,
         .category      = cat,
         .cpp_type_name = cpp_name,
         .par_type_name = par_name,
         .ret_type_name = ret_name};
    };
    // clang-format off
      // === atomic ===
      add(sql_type::integer,    "integer",    sql_cat::atomic, "int32_t", "int32_t", "int32_t");
      add(sql_type::smallint,   "smallint",   sql_cat::atomic, "int16_t", "int16_t", "int16_t");
      add(sql_type::bigint,     "bigint",     sql_cat::atomic, "int64_t", "int64_t", "int64_t");
      add(sql_type::tiny_int,   "tinyint",    sql_cat::atomic, "int8_t",  "int8_t",  "int8_t");
      add(sql_type::float_,     "float",      sql_cat::atomic, "double",  "double",  "double");
      add(sql_type::real,       "real",       sql_cat::atomic, "float",   "float",   "float");
      add(sql_type::double_,    "double",     sql_cat::atomic, "double",  "double",  "double");
      add(sql_type::bit,        "bit",        sql_cat::atomic, "bool",    "bool",    "bool");
      // === 8 bit strings ===
      add(sql_type::char_,         "char",         sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::numeric,       "numeric",      sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::decimal,       "decimal",      sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::var_char,      "varchar",      sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::decfloat,      "decfloat",     sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::long_var_char, "longvarchar",  sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::clob,          "clob",         sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      add(sql_type::xml,           "xml",          sql_cat::c_string, "char", "rtl::cstr_t", "rtl::cstr_t");
      // === 16 bit strings ===
      add(sql_type::wchar,          "wchar",        sql_cat::w_string, "wchar_t", "rtl::wcstr_t", "rtl::wcstr_t");
      add(sql_type::wvar_char,      "wvarchar",     sql_cat::w_string, "wchar_t", "rtl::wcstr_t", "rtl::wcstr_t");
      add(sql_type::wlong_var_char, "wlongvarchar", sql_cat::w_string, "wchar_t", "rtl::wcstr_t", "rtl::wcstr_t");
      add(sql_type::dbclob,         "dbclob",       sql_cat::w_string, "wchar_t", "rtl::wcstr_t", "rtl::wcstr_t");
      // === binary strings ===
      add(sql_type::graphic,         "graphic",        sql_cat::b_string, "uint8_t", "rtl::bcstr_t", "rtl::bcstr_t");
      add(sql_type::var_graphic,     "vargraphic",     sql_cat::b_string, "uint8_t", "rtl::bcstr_t", "rtl::bcstr_t");
      add(sql_type::binary,          "binary",         sql_cat::b_string, "uint8_t", "rtl::bcstr_t", "rtl::bcstr_t");
      add(sql_type::var_binary,      "varbinary",      sql_cat::b_string, "uint8_t", "rtl::bcstr_t", "rtl::bcstr_t");
      add(sql_type::long_var_binary, "longvarbinary",  sql_cat::b_string, "uint8_t", "rtl::bcstr_t", "rtl::bcstr_t");
      add(sql_type::blob,            "blob",           sql_cat::b_string, "uint8_t", "rtl::bcstr_t", "rtl::bcstr_t");
      // === date / time ===
      add(sql_type::date,           "date",           sql_cat::structure, "rtl::date",      "rtl::date",      "rtl::date");
      add(sql_type::time,           "time",           sql_cat::structure, "rtl::time",      "rtl::time",      "rtl::time");
      add(sql_type::timestamp,      "timestamp",      sql_cat::structure, "rtl::timestamp", "rtl::timestamp", "rtl::timestamp");
      add(sql_type::type_date,      "type_date",      sql_cat::structure, "rtl::date",      "rtl::date",      "rtl::date");
      add(sql_type::type_time,      "type_time",      sql_cat::structure, "rtl::time",      "rtl::time",      "rtl::time");
      add(sql_type::type_timestamp, "type_timestamp", sql_cat::structure, "rtl::timestamp", "rtl::timestamp", "rtl::timestamp");
      // === intervals ===
      add(sql_type::interval_year,             "interval_year",             sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_month,            "interval_month",            sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_year_to_month,    "interval_year_to_month",    sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_day,              "interval_day",              sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_hour,             "interval_hour",             sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_minute,           "interval_minute",           sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_second,           "interval_second",           sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_day_to_hour,      "interval_day_to_hour",      sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_day_to_minute,    "interval_day_to_minute",    sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_day_to_second,    "interval_day_to_second",    sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_hour_to_minute,   "interval_hour_to_minute",   sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_hour_to_second,   "interval_hour_to_second",   sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      add(sql_type::interval_minute_to_second, "interval_minute_to_second", sql_cat::structure, "rtl::interval", "rtl::interval", "rtl::interval");
      // === misc ===
      add(sql_type::guid,    "guid",    sql_cat::structure, "rtl::guid", "rtl::guid", "rtl::guid");
      add(sql_type::unknown, "unknown", sql_cat::structure, "void",      "void",      "void");
    // clang-format on
    return m;
  }

  /**
   * @brief the sql type table, built once at compile time
   *
   */
  inline constexpr sql_mapping_table sql_to_cpp_table = make_sql_to_cpp_table();

  /// Every enumerator has to be spelled out in the table above; a new sql_type
  /// that nobody filled in would otherwise read back as a zeroed entry and
  /// generate code for type `unknown` without a word of complaint.
  static_assert(std::ranges::none_of(sql_to_cpp_table, [](const sql_mapping& m) { return m.mnemonic.empty(); }),
                "sql_to_cpp_table has a hole - every sql_type needs an add() line");

  /**
   * @brief look up the mapping of a sql type
   *
   * Never returns nullptr - a type outside the table falls back to
   * sql_type::unknown, which is always present.
   *
   * @param type sql type to look up
   * @return const sql_mapping* mapping, never nullptr
   */
  [[nodiscard]] constexpr const sql_mapping* get_sql_mapping(sql_type type) noexcept
  {
    const auto idx = static_cast<size_t>(type);
    if (idx < sql_type_count) return &sql_to_cpp_table.at(idx);
    return &sql_to_cpp_table.at(static_cast<size_t>(sql_type::unknown));
  }
  /**
   * @brief description of one result column or one statement parameter
   *
   * Filled in by the backend from whatever its native describe call returns.
   */
  struct meta_dscr
  {
    int16_t     index;       ///< 1 based position
    std::string name;        ///< column name as reported by the db, or the generated parameter name
    sql_type    type;        ///< type mapped to our vocabulary
    int32_t     native_type; ///< raw backend type code (ODBC SQL_* code, or PostgreSQL type OID)
    uint32_t    size;        ///< max width in characters or bytes
    int16_t     digits;      ///< digits after the decimal point
    int16_t     nullable;    ///< 0 no nulls, 1 nullable, 2 unknown
  };
  using meta_vec = std::vector<meta_dscr>;
} // namespace rtl
