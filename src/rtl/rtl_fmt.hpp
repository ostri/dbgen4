// rtl_fmt.hpp
#pragma once
/**
 * @file
 * @brief fmt formatters for the neutral rtl structure types
 *
 * Included by generated code so that a date/time/interval column can be
 * printed by the generated dump() method. Backend neutral - no driver header.
 *
 * Every formatter here inherits parse() from formatter<std::string_view>, so a
 * format spec applies to these types as it would to a string: "{:>14}" pads a
 * date to fourteen columns.
 */

#include "sql_types.hpp"
#include <fmt/format.h>
#include <algorithm>
#include <cstdio>
#include <string_view>

namespace rtl::fmt_detail
{
  /**
   * @brief how much snprintf actually wrote, never more than the buffer holds
   *
   * snprintf returns the length it *would* have needed. For a value wider than
   * the format expects - a year past 9999, or a garbage hour read out of a
   * buffer the driver never filled - that is more than the buffer holds, and
   * handing the count straight to a string_view reads past the terminator.
   * A negative return, meaning an encoding error, becomes an empty view.
   */
  constexpr size_t written_len(int written, size_t capacity) noexcept
  {
    if (written < 0 || capacity == 0) return 0;
    return std::min(static_cast<size_t>(written), capacity - 1); // less the terminator
  }

  /**
   * @brief drop the trailing zeroes of a fractional second
   *
   * @param first_digit offset of the first fraction digit, so that everything
   *                    before it - including the dot - is left alone. It is
   *                    passed in rather than assumed, because the width of the
   *                    part in front is not fixed once a field prints wider
   *                    than its format.
   *
   * One digit always survives: `09:05:03.0` rather than a dangling
   * `09:05:03.` that no reader would accept.
   */
  constexpr size_t trim_fraction(const char* buffer, size_t len, size_t first_digit) noexcept
  {
    while (len > first_digit && buffer[len - 1] == '0') --len; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return (len == first_digit) ? first_digit + 1 : len;
  }

  /// append ".fffffffff" and trim it, returning the new total length
  template <size_t capacity>
  size_t append_fraction(char (&buffer)[capacity], size_t len, uint32_t fraction) noexcept // NOLINT(hicpp-avoid-c-arrays)
  {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const int    written = std::snprintf(buffer + len, capacity - len, ".%09u", fraction); // NOLINT
    const size_t total   = len + written_len(written, capacity - len);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return trim_fraction(buffer, total, len + 1); // +1 steps over the dot
  }
} // namespace rtl::fmt_detail

// rtl::date -> "YYYY-MM-DD"
template <>
struct fmt::formatter<rtl::date> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::date& dt, FormatContext& ctx) const
  {
    char      buffer[16];                     // "YYYY-MM-DD" and room for a field that prints wider NOLINT
    const int written = std::snprintf(buffer, // NOLINT
                                      sizeof(buffer),
                                      "%04d-%02d-%02d",
                                      static_cast<int>(dt.year),
                                      static_cast<int>(dt.month),
                                      static_cast<int>(dt.day));
    // NOLINTNEXTLINE(hicpp-no-array-decay)
    const std::string_view text(buffer, rtl::fmt_detail::written_len(written, sizeof(buffer)));
    return formatter<std::string_view>::format(text, ctx);
  }
};

// rtl::time -> "HH:MM:SS"
template <>
struct fmt::formatter<rtl::time> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::time& tm, FormatContext& ctx) const
  {
    char      buffer[16];                     // "HH:MM:SS" and room to spare NOLINT
    const int written = std::snprintf(buffer, // NOLINT
                                      sizeof(buffer),
                                      "%02d:%02d:%02d",
                                      static_cast<int>(tm.hour),
                                      static_cast<int>(tm.minute),
                                      static_cast<int>(tm.second));
    // NOLINTNEXTLINE(hicpp-no-array-decay)
    const std::string_view text(buffer, rtl::fmt_detail::written_len(written, sizeof(buffer)));
    return formatter<std::string_view>::format(text, ctx);
  }
};

// rtl::timestamp -> "YYYY-MM-DD HH:MM:SS.fffffffff" with trailing zeroes trimmed
template <>
struct fmt::formatter<rtl::timestamp> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::timestamp& ts, FormatContext& ctx) const
  {
    // The date and time go down first so that the offset of the fraction is
    // measured rather than assumed - that is what lets trim_fraction stop at
    // the right place even when a field prints wider than its format.
    char         buffer[48];                     // NOLINT
    const int    written = std::snprintf(buffer, // NOLINT
                                         sizeof(buffer),
                                         "%04d-%02d-%02d %02d:%02d:%02d",
                                         static_cast<int>(ts.year),
                                         static_cast<int>(ts.month),
                                         static_cast<int>(ts.day),
                                         static_cast<int>(ts.hour),
                                         static_cast<int>(ts.minute),
                                         static_cast<int>(ts.second));
    const size_t head    = rtl::fmt_detail::written_len(written, sizeof(buffer));
    const size_t total   = rtl::fmt_detail::append_fraction(buffer, head, ts.fraction);
    // NOLINTNEXTLINE(hicpp-no-array-decay)
    return formatter<std::string_view>::format(std::string_view(buffer, total), ctx);
  }
};

// rtl::guid -> "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
template <>
struct fmt::formatter<rtl::guid> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const rtl::guid& g, FormatContext& ctx) const
  {
    char buffer[40]; // 36 significant, rounded up NOLINT
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    const int written = std::snprintf(buffer, // NOLINT
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
    // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    // NOLINTNEXTLINE(hicpp-no-array-decay)
    const std::string_view text(buffer, rtl::fmt_detail::written_len(written, sizeof(buffer)));
    return formatter<std::string_view>::format(text, ctx);
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
    const char* sign  = (iv.sign == 0) ? "-" : "";
    size_t      total = 0;
    // Every kind is named rather than swept up by the default, because
    // -Werror=switch-enum is what makes a new interval_kind break the build
    // here instead of silently formatting as a day/second interval. The default
    // below is unreachable and only satisfies -Wswitch-default.
    switch (iv.kind)
    {
    case rtl::interval_kind::year:
    case rtl::interval_kind::month:
    case rtl::interval_kind::year_to_month:
    {
      // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
      const int written = std::snprintf(buffer, // NOLINT
                                        sizeof(buffer),
                                        "%s%u-%u",
                                        sign,
                                        iv.value.year_month_.year,
                                        iv.value.year_month_.month);
      // NOLINTEND(cppcoreguidelines-pro-type-union-access)
      total = rtl::fmt_detail::written_len(written, sizeof(buffer));
      break;
    }
    case rtl::interval_kind::day:
    case rtl::interval_kind::hour:
    case rtl::interval_kind::minute:
    case rtl::interval_kind::second:
    case rtl::interval_kind::day_to_hour:
    case rtl::interval_kind::day_to_minute:
    case rtl::interval_kind::day_to_second:
    case rtl::interval_kind::hour_to_minute:
    case rtl::interval_kind::hour_to_second:
    case rtl::interval_kind::minute_to_second:
    default:
    {
      // Same two step shape as rtl::timestamp, and for the same reason: the
      // fraction is trimmed, which needs its offset to be known rather than
      // guessed. Before this it was the one type that printed all nine digits.
      // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
      const int    written = std::snprintf(buffer, // NOLINT
                                           sizeof(buffer),
                                           "%s%u %02u:%02u:%02u",
                                           sign,
                                           iv.value.day_second_.day,
                                           iv.value.day_second_.hour,
                                           iv.value.day_second_.minute,
                                           iv.value.day_second_.second);
      const size_t head    = rtl::fmt_detail::written_len(written, sizeof(buffer));
      total                = rtl::fmt_detail::append_fraction(buffer, head, iv.value.day_second_.fraction);
      // NOLINTEND(cppcoreguidelines-pro-type-union-access)
      break;
    }
    }
    // NOLINTNEXTLINE(hicpp-no-array-decay)
    return formatter<std::string_view>::format(std::string_view(buffer, total), ctx);
  }
};
