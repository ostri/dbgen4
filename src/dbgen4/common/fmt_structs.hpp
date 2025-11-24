// #pragma once
// // db2_fmt_support.hpp – KONČNA VERZIJA: vse na hex + wchar_t kot string/hex

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <sqltypes.h>
#include <yaml-cpp/node/node.h>

// // ===================================================================
// // 1. Helperja za hex izpis (hitra, brez alokacij)
// // ===================================================================
// static std::string_view to_hex(const void* data, std::size_t size)
// {
//   static thread_local std::string buffer;
//   buffer.clear();
//   buffer.reserve(size * 2);

//   static constexpr char hex[] = "0123456789abcdef";
//   const auto*           bytes = static_cast<const unsigned char*>(data);

//   for (std::size_t i = 0; i < size; ++i)
//   {
//     buffer += hex[bytes[i] >> 4];
//     buffer += hex[bytes[i] & 0xF];
//   }
//   return buffer;
// }

// // ===================================================================
// // 2. wchar_t[N] → UTF-8 string (če so ASCII/Latin1) ALI hex (varno)
// // ===================================================================
// template <std::size_t N>
// static std::string_view wchar_to_string(const wchar_t (&ws)[N])
// {
//   static thread_local std::string buf;
//   buf.clear();
//   buf.reserve(N);

//   for (std::size_t i = 0; i < N && ws[i] != L'\0'; ++i)
//   {
//     wchar_t wc = ws[i];
//     if (wc < 0x80) { buf += static_cast<char>(wc); }
//     else
//     {
//       // Double-byte znak → fallback na hex
//       return to_hex(ws, (std::strlen(reinterpret_cast<const char*>(ws)) + 1) * sizeof(wchar_t));
//     }
//   }
//   return buf;
// }

// // Če hočeš VEDNO hex za GRAPHIC tipe (najbolj varno):
// // #define GRAPHIC_AS_HEX

// #ifdef GRAPHIC_AS_HEX
// template <std::size_t N>
// static std::string_view wchar_to_string(const wchar_t (&ws)[N])
// {
//   return to_hex(ws, N * sizeof(wchar_t));
// }
// #endif

// // ===================================================================
// // 3. Wrapper za vse byte array-e (BLOB, BINARY, VARBINARY, decimal val[])
// // ===================================================================
// template <typename T>
// static std::string_view bytes_to_hex(const T& container)
// {
//   return to_hex(container.data(), container.size() * sizeof(container[0]));
// }

// // Posebej za fixed unsigned char[N]
// template <std::size_t N>
// static std::string_view bytes_to_hex(const unsigned char (&arr)[N])
// {
//   return to_hex(arr, N);
// }

// ===================================================================
// 4. Formatterji – samo za tipe, ki jih fmt ne zna (ostalo že dela)
// ===================================================================
// clang-format off
// template <> struct fmt::formatter<DATE_STRUCT> : fmt::ostream_formatter{}; //NOLINT
// template <> struct fmt::formatter<TIME_STRUCT> : fmt::ostream_formatter{}; //NOLINT
// template <> struct fmt::formatter<TIMESTAMP_STRUCT> : fmt::ostream_formatter{}; //NOLINT
// clang-format on

// // std::vector<uint8_t> in podobni že delajo z ostream_formatter, ampak bomo raje hex
// template <typename T, typename A>
// struct fmt::formatter<std::vector<T, A>> : fmt::formatter<std::string_view>
// {
//   auto format(const std::vector<T, A>& v, format_context& ctx) const
//   {
//     return fmt::formatter<std::string_view>::format(bytes_to_hex(v), ctx);
//   }
// };

// // ===================================================================
// // 5. UPORABA V TVOJI KODI (t1.hpp linija 244)
// // ===================================================================

// // Namesto:
// // col_blob_.at(el), col_binary_.at(el), col_varbinary_.at(el)
// // col_graphic_.at(el), col_vargraphic_.at(el), col_dbclob_.at(el)
// // col_decimal_.at(el).val   // če je SQL_NUMERIC_STRUCT

// // Uporabi:
// bytes_to_hex(col_blob_.at(el)), bytes_to_hex(col_binary_.at(el)),
//   bytes_to_hex(col_varbinary_.at(el)), wchar_to_string(col_graphic_.at(el)),
//   wchar_to_string(col_vargraphic_.at(el)), wchar_to_string(col_dbclob_.at(el)),
//   bytes_to_hex(col_decimal_.at(el)) // če je unsigned char[16] ali .val

//   // Primer celotne vrstice:
//   s
//   += fmt::format(fmt,
//                  el,
//                  col_smallint_.at(el),
//                  col_integer_.at(el),
//                  col_bigint_.at(el),
//                  bytes_to_hex(col_decimal_.at(el)), // ← hex
//                  col_real_.at(el),
//                  col_double_.at(el),
//                  col_decfloat_.at(el),
//                  col_char_.at(el),
//                  col_varchar_.at(el),
//                  col_clob_.at(el),
//                  wchar_to_string(col_graphic_.at(el)),    // ← string ali hex
//                  wchar_to_string(col_vargraphic_.at(el)), // ← string ali hex
//                  wchar_to_string(col_dbclob_.at(el)),     // ← string ali hex
//                  bytes_to_hex(col_blob_.at(el)),          // ← hex
//                  bytes_to_hex(col_binary_.at(el)),        // ← hex
//                  bytes_to_hex(col_varbinary_.at(el)),     // ← hex
//                  col_date_.at(el),
//                  col_time_.at(el),
//                  col_timestamp_.at(el),
//                  col_boolean_.at(el),
//                  col_xml_.at(el));
// fmt_db2.hpp

// DATE_STRUCT → "YYYY-MM-DD"
template <>
struct fmt::formatter<DATE_STRUCT> : fmt::formatter<std::string_view> // NOLINT
{
  template <typename FormatContext>
  auto format(const DATE_STRUCT& dt, FormatContext& ctx) const
  {
    // fmt::format_to je najhitrejši način (izogiba se std::string alokaciji kjer ni nujno)
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
    // Odrežemo odvečne ničle na koncu (ni nujno, ampak lepše)
    char* end = buffer + written;                         // NOLINT
    while (end > buffer + 20 && *(end - 1) == '0') --end; // 20 = "YYYY-MM-DD HH:MM:SS." NOLINT
    if (end == buffer + 20) end += 1;                     // če ni bilo fraction, odstrani piko NOLINT
    else if (*(end - 1) == '.') --end;                    // NOLINT
    return formatter<std::string_view>::format(std::string_view(buffer, end - buffer), ctx); // NOLINT
  }
};
template <>
struct fmt::formatter<YAML::Node> : fmt::ostream_formatter // NOLINT
{
}; // NOLINT