// utf8.hpp
#pragma once
#include <string>
#include <string_view>
#include <cstddef>

#ifdef _WIN32
#  include <windows.h> // WideCharToMultiByte
#endif

namespace dbgen4
{

  inline std::string to_utf8(std::wstring_view wstr) noexcept
  {
    if (wstr.empty()) [[unlikely]] { return {}; }

#ifdef _WIN32
    // Windows: wchar_t = UTF-16 → najhitrejši sistemski klic
    const int size_needed = ::WideCharToMultiByte(CP_UTF8,
                                                  WC_ERR_INVALID_CHARS,
                                                  wstr.data(),
                                                  static_cast<int>(wstr.size()),
                                                  nullptr,
                                                  0,
                                                  nullptr,
                                                  nullptr);

    if (size_needed <= 0)
    {
      return {}; // neveljavna UTF-16 sekvenca
    }

    std::string result(static_cast<std::size_t>(size_needed), '\0');
    ::WideCharToMultiByte(CP_UTF8,
                          WC_ERR_INVALID_CHARS,
                          wstr.data(),
                          static_cast<int>(wstr.size()),
                          result.data(),
                          size_needed,
                          nullptr,
                          nullptr);

    return result;

#else
    // Linux/macOS/BSD: wchar_t = UTF-32 → ročna UTF-8 kodirna zanka
    std::string result;
    result.reserve(wstr.size() + (wstr.size() >> 1u));

    // NOLINTBEGIN(readability-magic-numbers)
    // Vse konstante so iz RFC 3629 – standardne meje za UTF-8 kodiranje
    for (const wchar_t wc : wstr)
    {
      const char32_t c = static_cast<char32_t>(wc);

      if (c < 0x80u) { result.push_back(static_cast<char>(c)); }
      else if (c < 0x800u)
      {
        result.push_back(static_cast<char>(0xC0u | (c >> 6u)));
        result.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
      }
      else if (c < 0x10000u)
      {
        result.push_back(static_cast<char>(0xE0u | (c >> 12u)));
        result.push_back(static_cast<char>(0x80u | ((c >> 6u) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
      }
      else if (c <= 0x10FFFFu)
      {
        result.push_back(static_cast<char>(0xF0u | (c >> 18u)));
        result.push_back(static_cast<char>(0x80u | ((c >> 12u) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | ((c >> 6u) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
      }
      else
      {
        // Neveljaven Unicode → replacement character
        result.append("\xEF\xBF\xBD");
      }
    }
    // NOLINTEND(readability-magic-numbers)

    return result;
#endif
  }

} // namespace dbgen4