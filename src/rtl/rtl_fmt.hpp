// rtl_fmt.hpp
#pragma once
/**
 * @file
 * @brief fmt formatters for the neutral rtl structure types
 *
 * Included by generated code so that a date/time/interval column can be
 * printed by the generated dump() method. Backend neutral - no driver header.
 */

#include "sql_types.hpp"
#include <fmt/format.h>
#include <cstdio>
#include <string_view>

// rtl::date -> "YYYY-MM-DD"
template <>
struct fmt::formatter<rtl::date> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::date& dt, FormatContext& ctx) const
  {
    char buffer[11]; // "YYYY-MM-DD\0" NOLINT
    // NOLINTBEGIN(readability-magic-numbers)
    int written = std::snprintf(buffer, // NOLINT
                                sizeof(buffer),
                                "%04d-%02d-%02d",
                                static_cast<int>(dt.year),
                                static_cast<int>(dt.month),
                                static_cast<int>(dt.day));
    // NOLINTEND(readability-magic-numbers)
    return formatter<std::string_view>::format(std::string_view(buffer, written), ctx); // NOLINT
  }
};

// rtl::time -> "HH:MM:SS"
template <>
struct fmt::formatter<rtl::time> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::time& tm, FormatContext& ctx) const
  {
    char buffer[9]; // "HH:MM:SS\0"  NOLINT
    // NOLINTBEGIN(readability-magic-numbers)
    int written = std::snprintf(buffer, // NOLINT
                                sizeof(buffer),
                                "%02d:%02d:%02d",
                                static_cast<int>(tm.hour),
                                static_cast<int>(tm.minute),
                                static_cast<int>(tm.second));
    // NOLINTEND(readability-magic-numbers)
    return formatter<std::string_view>::format(std::string_view(buffer, written), ctx); // NOLINT
  }
};

// rtl::timestamp -> "YYYY-MM-DD HH:MM:SS.fffffffff" with trailing zeroes trimmed
template <>
struct fmt::formatter<rtl::timestamp> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::timestamp& ts, FormatContext& ctx) const
  {
    char buffer[32]; // NOLINT
    // NOLINTBEGIN(readability-magic-numbers)
    int written = std::snprintf(buffer, // NOLINT
                                sizeof(buffer),
                                "%04d-%02d-%02d %02d:%02d:%02d.%09u",
                                static_cast<int>(ts.year),
                                static_cast<int>(ts.month),
                                static_cast<int>(ts.day),
                                static_cast<int>(ts.hour),
                                static_cast<int>(ts.minute),
                                static_cast<int>(ts.second),
                                ts.fraction);
    // NOLINTEND(readability-magic-numbers)
    // removing trailing zeroes
    char* end = buffer + written;                         // NOLINT
    while (end > buffer + 20 && *(end - 1) == '0') --end; // 20 = "YYYY-MM-DD HH:MM:SS." NOLINT
    if (end == buffer + 20) end += 1;                     // remove fraction dot NOLINT
    else if (*(end - 1) == '.') --end;                    // NOLINT
    return formatter<std::string_view>::format(std::string_view(buffer, end - buffer), ctx); // NOLINT
  }
};

// rtl::guid -> "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
template <>
struct fmt::formatter<rtl::guid> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::guid& g, FormatContext& ctx) const
  {
    char buffer[37]; // NOLINT
    // NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-pro-bounds-constant-array-index)
    int written = std::snprintf(buffer, // NOLINT
                                sizeof(buffer),
                                "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                                g.data1,
                                g.data2,
                                g.data3,
                                g.data4[0],
                                g.data4[1],
                                g.data4[2],
                                g.data4[3],
                                g.data4[4],
                                g.data4[5],
                                g.data4[6],
                                g.data4[7]);
    // NOLINTEND(readability-magic-numbers,cppcoreguidelines-pro-bounds-constant-array-index)
    return formatter<std::string_view>::format(std::string_view(buffer, written), ctx); // NOLINT
  }
};

// rtl::interval -> "[sign]Y-M" for year/month kinds, "[sign]D HH:MM:SS.fffffffff" otherwise
template <>
struct fmt::formatter<rtl::interval> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::interval& iv, FormatContext& ctx) const
  {
    char        buffer[64]; // NOLINT
    const char* sign = (iv.sign == 0) ? "-" : "";
    int         written = 0;
    switch (iv.kind)
    {
    case rtl::interval_kind::year:
    case rtl::interval_kind::month:
    case rtl::interval_kind::year_to_month:
      written = std::snprintf(buffer, // NOLINT
                              sizeof(buffer),
                              "%s%u-%u",
                              sign,
                              iv.value.year_month_.year,
                              iv.value.year_month_.month);
      break;
    default:
      // NOLINTBEGIN(readability-magic-numbers)
      written = std::snprintf(buffer, // NOLINT
                              sizeof(buffer),
                              "%s%u %02u:%02u:%02u.%09u",
                              sign,
                              iv.value.day_second_.day,
                              iv.value.day_second_.hour,
                              iv.value.day_second_.minute,
                              iv.value.day_second_.second,
                              iv.value.day_second_.fraction);
      // NOLINTEND(readability-magic-numbers)
      break;
    }
    return formatter<std::string_view>::format(std::string_view(buffer, written), ctx); // NOLINT
  }
};
