// odbc_error.cpp
#include "odbc_error.hpp"
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>

namespace rtl
{
  odbc_error::odbc_error(SQLRETURN r, SQLHANDLE h, handle_type_enum t, SQLSMALLINT rec) noexcept
  : ret_(r)
  {
    SQLCHAR     state[6]                    = {}; // NOLINT
    SQLCHAR     msg[SQL_MAX_MESSAGE_LENGTH] = {}; // NOLINT
    SQLSMALLINT msg_len                     = 0;

    const int16_t type = magic_enum::enum_integer(t);
    auto    dr =
      SQLGetDiagRec(type, h, rec, state, &native_error_, msg, sizeof(msg), &msg_len); // NOLINT
    if (SQL_SUCCEEDED(dr))
    {
      sql_state_ = std::string(reinterpret_cast<char*>(state), 5); // NOLINT
      message_   = sqlchar_to_utf8(msg, msg_len);                  // NOLINT
    }
    else
    {
      message_   = "ODBC diagnostic retrieval failed";
      sql_state_ = "HY000";
    }
  }

  odbc_error odbc_error::client(std::string message) noexcept
  {
    odbc_error e;
    e.ret_       = SQL_ERROR;
    e.message_   = std::move(message);
    e.sql_state_ = "HY010";
    return e;
  }

  std::string odbc_error::sqlchar_to_utf8(const SQLCHAR* src, size_t len) noexcept
  {
    if (len <= 0) return {};
#ifdef _UNICODE
    const wchar_t* wsrc = reinterpret_cast<const wchar_t*>(src);
    int            wlen = len / sizeof(wchar_t);
    int utf8_len        = WideCharToMultiByte(CP_UTF8, 0, wsrc, wlen, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) return {};
    std::string utf8(utf8_len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wsrc, wlen, utf8.data(), utf8_len, nullptr, nullptr);
    return utf8;
#else
    return {reinterpret_cast<const char*>(src), len}; // NOLINT
#endif
  }
} // namespace rtl
