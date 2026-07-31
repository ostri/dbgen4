// odbc_error.hpp
#pragma once
/**
 * @file
 * @brief the error type the db2 backend reports, and the handle kinds it reads from
 *
 * Split out of query.hpp: this says what a failure looks like, query.hpp says
 * how statements run. Everything that handles an error from this backend needs
 * the former; only the runtime itself needs the latter.
 */
#include <sqlcli1.h>
#include <cstdint>
#include <expected>
#include <string>

namespace rtl
{
  enum class handle_type_enum : int16_t // NO LINT(performance-enum-size)
  {
    env  = SQL_HANDLE_ENV, //< the environment handle
    conn = SQL_HANDLE_DBC, //< the connection handle
    stmt = SQL_HANDLE_STMT //< the statement handle
  };

  // ====================================================================
  // odbc_error — Unicode-safe (UTF-8), SI-ready
  // ====================================================================
  struct odbc_error
  {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    /// SQL_ERROR rather than no initialiser: the private default constructor
    /// leaves this member indeterminate otherwise, and reading it - which
    /// anything handling the error will do - is undefined. Defaulting to the
    /// failure value is also the safe direction for a type that only exists to
    /// report one.
    SQLRETURN   ret_ = SQL_ERROR;  //< db return code
    std::string message_;          //< UTF-8
    std::string sql_state_;        //< ASCII
    SQLINTEGER  native_error_ = 0; //< driver-specific error code
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    odbc_error(SQLRETURN r, SQLHANDLE h, handle_type_enum t, SQLSMALLINT rec = 1) noexcept;

    /**
     * @brief an error the runtime found itself, with no driver diagnostic behind it
     *
     * The other constructor asks the driver what went wrong. When the fault is
     * on this side of the call there is nothing to ask, and going through
     * SQLGetDiagRec anyway yields "ODBC diagnostic retrieval failed", which
     * says nothing about the actual mistake. HY010 is the ODBC state for a
     * function sequence error, which is exactly what these are.
     */
    static odbc_error client(std::string message) noexcept;
  private:
    odbc_error() noexcept = default;
    static std::string sqlchar_to_utf8(const SQLCHAR* src, size_t len) noexcept;
  } __attribute__((aligned(128))); // NOLINT

  using e_void = std::expected<void, odbc_error>;
} // namespace rtl
