#pragma once

#include <cstdint>
#include <memory>
// NOLINTNEXTLINE(unused-includes)
#include "log.hpp"

namespace rtl
{
  /**
   * @brief ODBC return codes mapped to enum class
   *
   * This enum class provides type-safe ODBC return codes without requiring ODBC headers.
   * Values are based on the SQL specification for ODBC return codes.
   */
  enum class db_sts : std::int16_t
  {
    // Success codes (positive)
    success           = 0,   ///< Successful completion
    success_with_info = 1,   ///< Successful completion with warnings
    no_data           = 100, ///< No data was found

    // Error codes (negative)
    error           = -1,  ///< General error
    invalid_handle  = -2,  ///< Invalid handle
    need_data       = -99, ///< More data needed
    still_executing = -3,  ///< Async operation still executing

    // Connection errors (-100 to -199)
    connection_error = -100, ///< General connection error
    connection_lost  = -101, ///< Connection was lost
    server_gone      = -102, ///< Server has gone away
    timeout          = -103, ///< Connection/operation timeout
    busy             = -104, ///< Database server busy
    access_denied    = -105, ///< Access denied (authentication)

    // Statement errors (-200 to -299)
    invalid_sql          = -200, ///< Invalid SQL statement
    syntax_error         = -201, ///< SQL syntax error
    constraint_violation = -202, ///< Constraint violation
    duplicate_key        = -203, ///< Duplicate key error
    truncated            = -204, ///< Data truncation occurred
    invalid_cursor       = -205, ///< Invalid cursor state

    // Transaction errors (-300 to -399)
    transaction_error     = -300, ///< General transaction error
    deadlock              = -301, ///< Deadlock detected
    serialization_failure = -302, ///< Serialization failure in transaction

    // Resource errors (-400 to -499)
    memory_error   = -400, ///< Memory allocation failure
    resource_error = -401, ///< General resource error
    disk_full      = -402, ///< Disk full error
    quota_exceeded = -403, ///< Resource quota exceeded

    // System and environment (-500 to -599)
    driver_not_found = -500, ///< Database driver not found
    env_error        = -501, ///< Environment error
    not_implemented  = -502, ///< Feature not implemented
    os_error         = -503, ///< Operating system error

    // Data conversion (-600 to -699)
    data_conversion_error = -600, ///< Data conversion error
    data_truncated        = -601, ///< Data was truncated
    invalid_parameter     = -602, ///< Invalid parameter value

    // Administrative (-700 to -799)
    admin_error   = -700, ///< Administrative error
    config_error  = -701, ///< Configuration error
    license_error = -702, ///< License error

    // Custom/extension (-1000 and below)
    custom_error = -1000, ///< Base for custom error codes
    unknown      = -9999  ///< Unknown/unspecified error
  };

  /**
   * @brief Check if a db_status code indicates success
   * @param status The status code to check
   * @return true if the status indicates success (including success_with_info)
   */
  constexpr bool is_success(db_sts status) noexcept
  {
    return status == db_sts::success || status == db_sts::success_with_info;
  }
  /**
   * @brief Check if a db_status code indicates no data
   * @param status The status code to check
   * @return true if the status is no_data
   */
  constexpr bool is_no_data(db_sts status) noexcept { return status == db_sts::no_data; }
  /**
   * @brief Convert db_status to string representation
   * @param status The status code to convert
   * @return const char* String representation of the status code
   */
  constexpr const char* db_status_to_string(db_sts status) noexcept
  {
    switch (status)
    {
    case db_sts::success: return "Success";
    case db_sts::success_with_info: return "Success with info";
    case db_sts::no_data: return "No data";
    case db_sts::error: return "Error";
    case db_sts::invalid_handle: return "Invalid handle";
    case db_sts::need_data: return "Need data";
    case db_sts::still_executing: return "Still executing";
    case db_sts::connection_error: return "Connection error";
    case db_sts::connection_lost: return "Connection lost";
    case db_sts::server_gone: return "Server gone";
    case db_sts::timeout: return "Timeout";
    case db_sts::busy: return "Database busy";
    case db_sts::access_denied: return "Access denied";
    case db_sts::invalid_sql: return "Invalid SQL";
    case db_sts::syntax_error: return "Syntax error";
    case db_sts::constraint_violation: return "Constraint violation";
    case db_sts::duplicate_key: return "Duplicate key";
    case db_sts::truncated: return "Data truncated";
    case db_sts::invalid_cursor: return "Invalid cursor state";
    case db_sts::transaction_error: return "Transaction error";
    case db_sts::deadlock: return "Deadlock detected";
    case db_sts::serialization_failure: return "Serialization failure";
    case db_sts::memory_error: return "Memory error";
    case db_sts::resource_error: return "Resource error";
    case db_sts::disk_full: return "Disk full";
    case db_sts::quota_exceeded: return "Quota exceeded";
    case db_sts::driver_not_found: return "Driver not found";
    case db_sts::env_error: return "Environment error";
    case db_sts::not_implemented: return "Not implemented";
    case db_sts::os_error: return "OS error";
    case db_sts::data_conversion_error: return "Data conversion error";
    case db_sts::data_truncated: return "Data truncated";
    case db_sts::invalid_parameter: return "Invalid parameter";
    case db_sts::admin_error: return "Administrative error";
    case db_sts::config_error: return "Configuration error";
    case db_sts::license_error: return "License error";
    case db_sts::custom_error: return "Custom error";
    case db_sts::unknown: return "Unknown error";
    default: return "Undefined error";
    }
  }


  /// Root class for all database data structures
  /// Provides common functionality and interface for database objects
  /// empty on purpose, to be extended for specific database data implementations
  class db_data_root
  {
  protected:
    // NOLINTNEXTLINE(cert-err58-cpp)
    inline static const auto log = log::get();
  public:
    db_data_root()                               = default;
    virtual ~db_data_root()                      = default;
    db_data_root(const db_data_root&)            = delete;
    db_data_root& operator=(const db_data_root&) = delete;
    db_data_root(db_data_root&&)                 = delete;
    db_data_root& operator=(db_data_root&&)      = delete;
  };
  /**
   * @brief Root class for all database implementations
   *
   * Provides common functionality and interface for database engines.
   * This class is intended to be extended for specific database implementations.
   * It manages the connection lifecycle and basic transaction operations.
   */
  class db
  {
  protected:
    // NOLINTNEXTLINE(cert-err58-cpp)
    inline static const auto log = log::get();
  public:
    db() = default;
    virtual ~db();
    db(const db&)            = delete;
    db& operator=(const db&) = delete;
    db(db&&)                 = delete;
    db& operator=(db&&)      = delete;

    /**
     * @brief Establishes a connection to the database.
     * @return db_sts Status code indicating the result of the connection attempt.
     *
     * This method should be overridden by derived classes to implement
     * database-specific connection logic.
     */
    virtual db_sts connect(const std::string& name);
    virtual db_sts connect(const std::string& host,
                           const std::string& port,
                           const std::string& database_name,
                           const std::string& user,
                           const std::string& password);
    /**
     * @brief Disconnects from the database.
     * @return db_sts Status code indicating the result of the disconnection attempt.
     * This method should be overridden by derived classes to implement
     * database-specific disconnection logic.
     */
    virtual db_sts disconnect() { return db_sts::success; }
    /**
     * @brief Checks if the database connection is currently established.
     * @return true if connected, false otherwise.
     */
    [[nodiscard]] virtual bool        is_connected() const;
    virtual db_sts                    commit();
    virtual db_sts                    rollback();
    [[nodiscard]] const db_data_root* data() const;
  protected:
    /**
     * @brief Pointer to database-specific data implementation.
     *
     * Ownership: This unique_ptr owns the lifetime of the implementation-specific
     * data structure for the database connection. Derived classes should assign
     * their own implementation of db_data_root to this pointer to store
     * connection handles, state, or other backend-specific data.
     */
    // clang-format off
      /// NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
      std::unique_ptr<db_data_root> data_; //NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    // clang-format on
  };

}; // namespace rtl
