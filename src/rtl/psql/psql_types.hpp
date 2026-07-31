// psql_types.hpp
#pragma once
/**
 * @file
 * @brief translation between PostgreSQL type OIDs and the neutral rtl vocabulary
 *
 * The OIDs of built-in types are part of the PostgreSQL wire protocol and are
 * therefore fixed for all time - they are spelled out here rather than pulled
 * from catalog/pg_type_d.h, which ships only with the server development
 * package and is not needed to talk to a server.
 */

#include "sql_types.hpp"
#include <cstdint>
#include <utility>

namespace rtl::psql
{
  using oid_t = uint32_t;

  /// OIDs of the built-in types we know how to map
  enum class pg_oid : oid_t // NOLINT(performance-enum-size)
  {
    boolean     = 16,
    bytea       = 17,
    char_       = 18,
    name        = 19,
    int8        = 20,
    int2        = 21,
    int4        = 23,
    text        = 25,
    oid         = 26,
    json        = 114,
    xml         = 142,
    float4      = 700,
    float8      = 701,
    bpchar      = 1042, ///< char(n), blank padded
    varchar     = 1043,
    date        = 1082,
    time        = 1083,
    timestamp   = 1114,
    timestamptz = 1184,
    interval    = 1186,
    timetz      = 1266,
    bit         = 1560,
    varbit      = 1562,
    numeric     = 1700,
    uuid        = 2950,
    jsonb       = 3802
  };

  /**
   * @brief map a PostgreSQL type OID to the neutral vocabulary
   *
   * Anything we do not know about - and that includes every user defined type,
   * enum, domain and array - becomes sql_type::unknown. The caller reports it;
   * silently guessing a type would produce code that compiles and corrupts.
   *
   * @param oid type OID as reported by PQftype / PQparamtype
   * @return sql_type neutral type
   */
  [[nodiscard]] constexpr sql_type from_oid(oid_t oid) noexcept
  {
    switch (static_cast<pg_oid>(oid))
    {
    // --- atomic ---
    case pg_oid::boolean: return sql_type::bit;
    case pg_oid::int2: return sql_type::smallint;
    case pg_oid::int4: return sql_type::integer;
    case pg_oid::int8: // NOLINT(bugprone-branch-clone)
      return sql_type::bigint;
    /// oid is an unsigned 32 bit value - integer would overflow on the high half
    case pg_oid::oid: return sql_type::bigint;
    case pg_oid::float4: return sql_type::real;
    case pg_oid::float8: return sql_type::double_;
    // --- character data ---
    case pg_oid::char_:
    case pg_oid::bpchar: return sql_type::char_;
    case pg_oid::name:
    case pg_oid::varchar: return sql_type::var_char;
    /// text, json and jsonb have no declared upper bound
    case pg_oid::text:
    case pg_oid::json:
    case pg_oid::jsonb: return sql_type::long_var_char;
    case pg_oid::xml: return sql_type::xml;
    /// numeric travels as text so that no precision is lost on the way out
    case pg_oid::numeric: return sql_type::numeric;
    // --- binary data ---
    case pg_oid::bytea: // NOLINT(bugprone-branch-clone)
      return sql_type::var_binary;
    case pg_oid::bit:
    case pg_oid::varbit: return sql_type::var_binary;
    // --- date / time ---
    case pg_oid::date: return sql_type::type_date;
    case pg_oid::time:
    case pg_oid::timetz: return sql_type::type_time;
    case pg_oid::timestamp:
    case pg_oid::timestamptz: return sql_type::type_timestamp;
    case pg_oid::interval: return sql_type::interval_day_to_second;
    // --- misc ---
    case pg_oid::uuid: return sql_type::guid;
    default: return sql_type::unknown;
    }
  }

  /**
   * @brief map a neutral type back to the OID to declare a parameter as
   *
   * @param type neutral type
   * @return oid_t type OID, 0 ("unspecified") when we have no opinion, which
   *         lets the server infer the type from context
   */
  [[nodiscard]] constexpr oid_t to_oid(sql_type type) noexcept
  {
    switch (type)
    {
    case sql_type::bit: return static_cast<oid_t>(pg_oid::boolean);
    case sql_type::smallint: return static_cast<oid_t>(pg_oid::int2);
    case sql_type::integer: return static_cast<oid_t>(pg_oid::int4);
    case sql_type::bigint: return static_cast<oid_t>(pg_oid::int8);
    case sql_type::tiny_int: return static_cast<oid_t>(pg_oid::int2); // no 8 bit integer in PostgreSQL
    case sql_type::real: return static_cast<oid_t>(pg_oid::float4);
    case sql_type::float_:
    case sql_type::double_: return static_cast<oid_t>(pg_oid::float8);
    case sql_type::char_: return static_cast<oid_t>(pg_oid::bpchar);
    case sql_type::var_char: return static_cast<oid_t>(pg_oid::varchar);
    case sql_type::long_var_char:
    case sql_type::clob: return static_cast<oid_t>(pg_oid::text);
    case sql_type::xml: return static_cast<oid_t>(pg_oid::xml);
    case sql_type::numeric:
    case sql_type::decimal:
    case sql_type::decfloat: return static_cast<oid_t>(pg_oid::numeric);
    case sql_type::binary:
    case sql_type::var_binary:
    case sql_type::long_var_binary:
    case sql_type::blob: return static_cast<oid_t>(pg_oid::bytea);
    case sql_type::date:
    case sql_type::type_date: return static_cast<oid_t>(pg_oid::date);
    case sql_type::time:
    case sql_type::type_time: return static_cast<oid_t>(pg_oid::time);
    case sql_type::timestamp:
    case sql_type::type_timestamp: return static_cast<oid_t>(pg_oid::timestamp);
    case sql_type::guid: return static_cast<oid_t>(pg_oid::uuid);
    case sql_type::interval_year:
    case sql_type::interval_month:
    case sql_type::interval_year_to_month:
    case sql_type::interval_day:
    case sql_type::interval_hour:
    case sql_type::interval_minute:
    case sql_type::interval_second:
    case sql_type::interval_day_to_hour:
    case sql_type::interval_day_to_minute:
    case sql_type::interval_day_to_second:
    case sql_type::interval_hour_to_minute:
    case sql_type::interval_hour_to_second:
    case sql_type::interval_minute_to_second: return static_cast<oid_t>(pg_oid::interval);
    /// PostgreSQL has no 16 bit character types - everything is UTF-8
    case sql_type::wchar:
    case sql_type::wvar_char:
    case sql_type::wlong_var_char:
    case sql_type::dbclob:
    case sql_type::graphic:
    case sql_type::var_graphic:
    case sql_type::unknown:
    /// the table width, not a type anyone can bind - see db2_types::to_odbc()
    case sql_type::count_:
    default: return 0; // unspecified - let the server infer
    }
  }

  /**
   * @brief width of a column in characters or bytes
   *
   * PostgreSQL reports a storage size and a type modifier rather than a width.
   * For the varlena character types the modifier carries the declared length
   * plus a four byte header; for everything else the fixed size is the width.
   *
   * @param oid type OID
   * @param typmod type modifier as reported by PQfmod, -1 when there is none
   * @param size storage size as reported by PQfsize, -1 for variable length
   * @return uint32_t declared width, or 0 when the type carries none
   */
  [[nodiscard]] constexpr uint32_t column_width(oid_t oid, int32_t typmod, int32_t size) noexcept
  {
    constexpr uint32_t varlena_header = 4;
    /// 0 means "the database declares no width" - the generator substitutes
    /// its --max-field-len, or whatever the yaml file says for this column
    constexpr uint32_t unbounded     = 0;
    constexpr uint32_t uuid_text_len = 36;
    constexpr uint32_t numeric_len   = 64; ///< generous - numeric arrives as text

    /// switching on the raw value rather than pg_oid on purpose - this only
    /// cares about a handful of types, and -Wswitch-enum would demand the rest
    switch (oid)
    {
    case static_cast<oid_t>(pg_oid::bpchar):
    case static_cast<oid_t>(pg_oid::varchar):
      return std::cmp_greater(typmod, varlena_header) ? static_cast<uint32_t>(typmod) - varlena_header : unbounded;
    case static_cast<oid_t>(pg_oid::numeric): return numeric_len;
    case static_cast<oid_t>(pg_oid::uuid): return uuid_text_len;
    case static_cast<oid_t>(pg_oid::text):
    case static_cast<oid_t>(pg_oid::json):
    case static_cast<oid_t>(pg_oid::jsonb):
    case static_cast<oid_t>(pg_oid::xml):
    case static_cast<oid_t>(pg_oid::bytea):
    case static_cast<oid_t>(pg_oid::bit):
    case static_cast<oid_t>(pg_oid::varbit):
    case static_cast<oid_t>(pg_oid::name): return unbounded;
    default: return (size > 0) ? static_cast<uint32_t>(size) : unbounded;
    }
  }

  /**
   * @brief digits after the decimal point, for numeric/decimal columns
   *
   * @param oid type OID
   * @param typmod type modifier as reported by PQfmod
   * @return int16_t scale, 0 when not applicable
   */
  [[nodiscard]] constexpr int16_t column_scale(oid_t oid, int32_t typmod) noexcept
  {
    constexpr uint32_t varlena_header = 4;
    constexpr uint32_t scale_mask     = 0xffff;
    if (static_cast<pg_oid>(oid) != pg_oid::numeric || std::cmp_less(typmod, varlena_header)) return 0;
    return static_cast<int16_t>((typmod - varlena_header) & scale_mask);
  }

} // namespace rtl::psql
