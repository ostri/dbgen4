// #pragma once
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <sqltypes.h>
#include <yaml-cpp/node/node.h>
#include <stacktrace>

// DATE_STRUCT → "YYYY-MM-DD"
template <>
struct fmt::formatter<DATE_STRUCT> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const DATE_STRUCT& dt, FormatContext& ctx) const
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

// TIME_STRUCT → "HH:MM:SS"
template <>
struct fmt::formatter<TIME_STRUCT> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const TIME_STRUCT& tm, FormatContext& ctx) const
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

// TIMESTAMP_STRUCT → "YYYY-MM-DD HH:MM:SS.fffffffff"
template <>
struct fmt::formatter<TIMESTAMP_STRUCT> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const TIMESTAMP_STRUCT& ts, FormatContext& ctx) const
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
// clang-format off
template <> struct fmt::formatter<YAML::Node> : fmt::ostream_formatter {}; //NOLINT
template <> struct fmt::formatter<std::stacktrace> : fmt::ostream_formatter{}; // NOLINT
// clang-format on