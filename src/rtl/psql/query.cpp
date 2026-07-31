// query.cpp - text conversions for the psql runtime
#include "query.hpp"
/// formatter for rtl::guid, reached only through fmt::format's template
/// machinery in format_guid(). include-cleaner cannot see that path and calls
/// the header unused - removing it does not compile, so the pragma stays.
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace rtl::detail
{
  namespace
  {
    /// hex digit to value, 0xff when the character is not a hex digit
    constexpr uint8_t hex_val(char c) noexcept
    {
      if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
      if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
      if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
      return 0xff;
    }

    /// read exactly n digits starting at pos, advancing it past them
    template <typename T>
    bool read_digits(std::string_view sv, size_t& pos, size_t n, T& out) noexcept
    {
      if (pos + n > sv.size()) return false;
      T v = 0;
      for (size_t i = 0; i < n; ++i)
      {
        const char c = sv[pos + i];
        if (c < '0' || c > '9') return false;
        v = static_cast<T>(v * 10 + (c - '0'));
      }
      pos += n;
      out = v;
      return true;
    }

    bool expect(std::string_view sv, size_t& pos, char c) noexcept
    {
      if (pos >= sv.size() || sv[pos] != c) return false;
      ++pos;
      return true;
    }

    /// fractional seconds ".ffffff" -> nanoseconds; optional
    void read_fraction(std::string_view sv, size_t& pos, uint32_t& nanos) noexcept
    {
      nanos = 0;
      if (pos >= sv.size() || sv[pos] != '.') return;
      ++pos;
      uint32_t scale = 100000000; // first digit is 1e8 ns
      while (pos < sv.size() && sv[pos] >= '0' && sv[pos] <= '9')
      {
        nanos += static_cast<uint32_t>(sv[pos] - '0') * scale;
        scale /= 10;
        ++pos;
      }
    }
  } // namespace

  // --------------------------------------------------------------------
  // date / time / timestamp - PostgreSQL text format is ISO 8601 by default
  // --------------------------------------------------------------------
  bool parse_date(std::string_view sv, rtl::date& out) noexcept
  {
    size_t   p = 0;
    uint16_t y = 0;
    uint16_t m = 0;
    uint16_t d = 0;
    if (! read_digits(sv, p, 4, y) || ! expect(sv, p, '-')) return false;
    if (! read_digits(sv, p, 2, m) || ! expect(sv, p, '-')) return false;
    if (! read_digits(sv, p, 2, d)) return false;
    out = rtl::date{.year = static_cast<int16_t>(y), .month = m, .day = d};
    return true;
  }

  bool parse_time(std::string_view sv, rtl::time& out) noexcept
  {
    size_t   p  = 0;
    uint16_t hh = 0;
    uint16_t mm = 0;
    uint16_t ss = 0;
    if (! read_digits(sv, p, 2, hh) || ! expect(sv, p, ':')) return false;
    if (! read_digits(sv, p, 2, mm) || ! expect(sv, p, ':')) return false;
    if (! read_digits(sv, p, 2, ss)) return false;
    out = rtl::time{.hour = hh, .minute = mm, .second = ss};
    return true;
  }

  bool parse_timestamp(std::string_view sv, rtl::timestamp& out) noexcept
  {
    size_t   p  = 0;
    uint16_t y  = 0;
    uint16_t mo = 0;
    uint16_t d  = 0;
    uint16_t hh = 0;
    uint16_t mm = 0;
    uint16_t ss = 0;
    uint32_t ns = 0;
    if (! read_digits(sv, p, 4, y) || ! expect(sv, p, '-')) return false;
    if (! read_digits(sv, p, 2, mo) || ! expect(sv, p, '-')) return false;
    if (! read_digits(sv, p, 2, d)) return false;
    /// PostgreSQL separates date and time with a space, ISO 8601 uses 'T'
    if (p < sv.size() && (sv[p] == ' ' || sv[p] == 'T')) ++p;
    if (! read_digits(sv, p, 2, hh) || ! expect(sv, p, ':')) return false;
    if (! read_digits(sv, p, 2, mm) || ! expect(sv, p, ':')) return false;
    if (! read_digits(sv, p, 2, ss)) return false;
    read_fraction(sv, p, ns);
    /// any trailing time zone offset is dropped - rtl::timestamp has nowhere
    /// to keep it, so a timestamptz arrives already converted to the session
    /// time zone by the server
    out = rtl::timestamp{.year = static_cast<int16_t>(y), .month = mo, .day = d, .hour = hh, .minute = mm, .second = ss, .fraction = ns};
    return true;
  }

  bool parse_guid(std::string_view sv, rtl::guid& out) noexcept
  {
    /// "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    std::array<uint8_t, 16> bytes{};
    size_t                  b = 0;
    for (size_t i = 0; i < sv.size() && b < bytes.size();)
    {
      if (sv[i] == '-')
      {
        ++i;
        continue;
      }
      if (i + 1 >= sv.size()) return false;
      const uint8_t hi = hex_val(sv[i]);
      const uint8_t lo = hex_val(sv[i + 1]);
      if (hi == 0xff || lo == 0xff) return false;
      bytes.at(b++) = static_cast<uint8_t>((hi << 4) | lo);
      i += 2;
    }
    if (b != bytes.size()) return false;

    // NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-pro-bounds-constant-array-index)
    out.data1 =
      (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) | (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
    out.data2 = static_cast<uint16_t>((bytes[4] << 8) | bytes[5]);
    out.data3 = static_cast<uint16_t>((bytes[6] << 8) | bytes[7]);
    for (size_t i = 0; i < 8; ++i) out.data4[i] = bytes[8 + i];
    // NOLINTEND(readability-magic-numbers,cppcoreguidelines-pro-bounds-constant-array-index)
    return true;
  }

  std::string format_date(const rtl::date& v) { return fmt::format("{:04d}-{:02d}-{:02d}", v.year, v.month, v.day); }

  std::string format_time(const rtl::time& v) { return fmt::format("{:02d}:{:02d}:{:02d}", v.hour, v.minute, v.second); }

  std::string format_timestamp(const rtl::timestamp& v)
  {
    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:09d}", v.year, v.month, v.day, v.hour, v.minute, v.second, v.fraction);
  }

  std::string format_guid(const rtl::guid& v) { return fmt::format("{}", v); }

  // --------------------------------------------------------------------
  // bytea - PostgreSQL text format is "\x" followed by hex digits
  // --------------------------------------------------------------------
  size_t decode_bytea(std::string_view sv, std::byte* dst, size_t capacity) noexcept
  {
    if (sv.size() >= 2 && sv[0] == '\\' && (sv[1] == 'x' || sv[1] == 'X')) sv.remove_prefix(2);
    size_t n = 0;
    for (size_t i = 0; i + 1 < sv.size() && n < capacity; i += 2)
    {
      const uint8_t hi = hex_val(sv[i]);
      const uint8_t lo = hex_val(sv[i + 1]);
      if (hi == 0xff || lo == 0xff) break;
      dst[n++] = static_cast<std::byte>((hi << 4) | lo); // NOLINT
    }
    return n;
  }

  std::string encode_bytea(const std::byte* src, size_t len)
  {
    std::string out;
    out.reserve((len * 2) + 2);
    out += "\\x";
    for (size_t i = 0; i < len; ++i) out += fmt::format("{:02x}", static_cast<unsigned>(src[i])); // NOLINT
    return out;
  }

  // --------------------------------------------------------------------
  // storing a server value into a generated buffer slot
  // --------------------------------------------------------------------
  bool store_value(const buffer_dscr_const& dscr, const buffer_dscr_init& init, size_t row, std::string_view text) noexcept
  {
    void* slot = row_ptr(init, row);

    switch (dscr.category)
    {
    case sql_cat::atomic:
    {
      /// if/else rather than switch: only a handful of the sql_type
      /// enumerators can reach an atomic slot, and -Wswitch-enum would demand
      /// a case for all forty odd of them
      const auto t = dscr.type;
      if (t == sql_type::smallint) return parse_int(text, *static_cast<int16_t*>(slot));
      if (t == sql_type::integer) return parse_int(text, *static_cast<int32_t*>(slot));
      if (t == sql_type::bigint) return parse_int(text, *static_cast<int64_t*>(slot));
      if (t == sql_type::tiny_int) return parse_int(text, *static_cast<int8_t*>(slot));
      if (t == sql_type::real) return parse_float(text, *static_cast<float*>(slot));
      if (t == sql_type::float_ || t == sql_type::double_) return parse_float(text, *static_cast<double*>(slot));
      if (t == sql_type::bit)
      {
        /// PostgreSQL renders boolean as "t" or "f"
        *static_cast<bool*>(slot) = (! text.empty() && (text[0] == 't' || text[0] == 'T' || text[0] == '1'));
        return true;
      }
      return false;
    }
    case sql_cat::c_string:
    {
      /// the generated array is column_size + 1 bytes, the extra one is the
      /// safety null - a longer value is truncated rather than overrun
      auto*        dst      = static_cast<char*>(slot);
      const size_t capacity = dscr.column_size;
      const size_t n        = std::min(text.size(), capacity);
      std::memcpy(dst, text.data(), n);
      dst[n]                  = '\0';                    // NOLINT
      init.indicator_ptr[row] = static_cast<int32_t>(n); // NOLINT
      return true;
    }
    case sql_cat::b_string:
    {
      const size_t n          = decode_bytea(text, static_cast<std::byte*>(slot), dscr.column_size);
      init.indicator_ptr[row] = static_cast<int32_t>(n); // NOLINT
      return true;
    }
    case sql_cat::structure:
    {
      const auto t = dscr.type;
      if (t == sql_type::date || t == sql_type::type_date) return parse_date(text, *static_cast<rtl::date*>(slot));
      if (t == sql_type::time || t == sql_type::type_time) return parse_time(text, *static_cast<rtl::time*>(slot));
      if (t == sql_type::timestamp || t == sql_type::type_timestamp) return parse_timestamp(text, *static_cast<rtl::timestamp*>(slot));
      if (t == sql_type::guid) return parse_guid(text, *static_cast<rtl::guid*>(slot));
      /// intervals are not converted: PostgreSQL's interval output is far
      /// richer than rtl::interval can hold, and quietly dropping the parts
      /// that do not fit would be worse than refusing
      return false;
    }
    case sql_cat::w_string:
      /// PostgreSQL is UTF-8 throughout and never reports a 16 bit character
      /// type, so reaching here means the neutral mapping produced something
      /// this backend cannot honour
      return false;
    default: return false;
    }
  }

  // --------------------------------------------------------------------
  // rendering a generated buffer slot as text for the server
  // --------------------------------------------------------------------
  std::string load_value(const buffer_dscr_const& dscr, const buffer_dscr_init& init, size_t row)
  {
    const void* slot = row_ptr(init, row);

    switch (dscr.category)
    {
    case sql_cat::atomic:
    {
      const auto t = dscr.type;
      if (t == sql_type::smallint) return fmt::format("{}", *static_cast<const int16_t*>(slot));
      if (t == sql_type::integer) return fmt::format("{}", *static_cast<const int32_t*>(slot));
      if (t == sql_type::bigint) return fmt::format("{}", *static_cast<const int64_t*>(slot));
      if (t == sql_type::tiny_int) return fmt::format("{}", *static_cast<const int8_t*>(slot));
      if (t == sql_type::real) return fmt::format("{}", *static_cast<const float*>(slot));
      if (t == sql_type::float_ || t == sql_type::double_) return fmt::format("{}", *static_cast<const double*>(slot));
      if (t == sql_type::bit) return *static_cast<const bool*>(slot) ? "true" : "false";
      return {};
    }
    case sql_cat::c_string:
    {
      const auto len = static_cast<size_t>(init.indicator_ptr[row]); // NOLINT
      return std::string(static_cast<const char*>(slot), std::min(len, static_cast<size_t>(dscr.column_size)));
    }
    case sql_cat::b_string:
    {
      const auto len = static_cast<size_t>(init.indicator_ptr[row]); // NOLINT
      return encode_bytea(static_cast<const std::byte*>(slot), std::min(len, static_cast<size_t>(dscr.column_size)));
    }
    case sql_cat::structure:
    {
      const auto t = dscr.type;
      if (t == sql_type::date || t == sql_type::type_date) return format_date(*static_cast<const rtl::date*>(slot));
      if (t == sql_type::time || t == sql_type::type_time) return format_time(*static_cast<const rtl::time*>(slot));
      if (t == sql_type::timestamp || t == sql_type::type_timestamp) return format_timestamp(*static_cast<const rtl::timestamp*>(slot));
      if (t == sql_type::guid) return format_guid(*static_cast<const rtl::guid*>(slot));
      return {};
    }
    case sql_cat::w_string:
    default: return {};
    }
  }

} // namespace rtl::detail
