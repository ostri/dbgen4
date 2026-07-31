// utf8.hpp
#pragma once
#include <string>
#include <string_view>
// #include <cstddef>

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
    const int size_needed =
      ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);

    if (size_needed <= 0)
    {
      return {}; // neveljavna UTF-16 sekvenca
    }

    std::string result(static_cast<std::size_t>(size_needed), '\0');
    ::WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, wstr.data(), static_cast<int>(wstr.size()), result.data(), size_needed, nullptr, nullptr);

    return result;

#else
    // Linux/macOS/BSD: wchar_t = UTF-32 → ročna UTF-8 kodirna zanka
    std::string result;
    result.reserve(wstr.size() + (wstr.size() >> 1U));

    // NOLINTBEGIN(readability-magic-numbers)
    // Vse konstante so iz RFC 3629 – standardne meje za UTF-8 kodiranje
    for (const wchar_t wc : wstr)
    {
      const auto c = static_cast<char32_t>(wc);

      if (c < 0x80U) { result.push_back(static_cast<char>(c)); }
      else if (c < 0x800U)
      {
        result.push_back(static_cast<char>(0xC0U | (c >> 6U)));
        result.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
      }
      else if (c < 0x10000U)
      {
        result.push_back(static_cast<char>(0xE0U | (c >> 12U)));
        result.push_back(static_cast<char>(0x80U | ((c >> 6U) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
      }
      else if (c <= 0x10FFFFU)
      {
        result.push_back(static_cast<char>(0xF0U | (c >> 18U)));
        result.push_back(static_cast<char>(0x80U | ((c >> 12U) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | ((c >> 6U) & 0x3FU)));
        result.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
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