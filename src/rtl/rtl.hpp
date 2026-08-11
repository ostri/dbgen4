#pragma once

#include <cassert>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <logger/logger.hpp> // IWYU pragma: keep
#include "sql_types.hpp"     // IWYU pragma: export

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
  constexpr bool is_success(db_sts status) noexcept { return status == db_sts::success || status == db_sts::success_with_info; }
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


  /**
   * @brief Result of describing a SQL statement - metadata only, no rows
   *
   * Backend neutral: whatever the driver reports is already translated into
   * rtl::sql_type by the backend before it lands here.
   */
  class qry_metadata
  {
  public:
    qry_metadata() = default;
    /// getters
    [[nodiscard]] meta_vec    columns() const;
    [[nodiscard]] meta_vec    params() const;
    [[nodiscard]] std::string dump() const;
    /// setters
    void add_col_dscr(const meta_dscr& dscr);
    void add_par_dscr(const meta_dscr& dscr);
  private:
    [[nodiscard]] std::string dump_meta_vector(const char* fmt, const char* header, const meta_vec& v) const;
    meta_vec                  columns_; ///< Result-set column metadata
    meta_vec                  params_;  ///< Input parameter metadata
  };
  using e_qry_metadata = std::expected<qry_metadata, db_sts>;

  /// Root class for all database data structures
  /// Provides common functionality and interface for database objects
  /// empty on purpose, to be extended for specific database data implementations
  class db_data_root
  {
  public:
    explicit db_data_root(logger::Logger& log)
    : logger_(log)
    {
    }
    virtual ~db_data_root()                      = default;
    db_data_root(const db_data_root&)            = delete;
    db_data_root& operator=(const db_data_root&) = delete;
    db_data_root(db_data_root&&)                 = delete;
    db_data_root& operator=(db_data_root&&)      = delete;
  protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
    logger::Logger& logger_; ///< reference to the shared Logger, not owner
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
  public:
    explicit db(logger::Logger& log)
    : logger_(log)
    {
    }
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
    virtual db_sts connect(const std::string& conn_str);
    virtual db_sts connect(const std::string& host,
                           uint16_t           port,
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
    [[nodiscard]] virtual bool is_connected() const;
    virtual db_sts             commit();
    virtual db_sts             rollback();
    /**
     * @brief Describe a SQL statement without executing it
     *
     * The generator relies on this to learn the shape of every statement in
     * the yaml file. Each backend implements it with its own describe call
     * (ODBC SQLDescribeCol/Param, libpq PQdescribePrepared, ...).
     *
     * @param sql statement, possibly carrying parameter placeholders
     * @return e_qry_metadata column and parameter descriptions, or an error
     */
    virtual e_qry_metadata get_sql_metadata(const std::string& sql);
    /**
     * @brief run a statement that takes no parameters and returns no rows
     *
     * For DDL/utility statements a generated query<> is overkill for -
     * dbgen4's own generator uses this to run a statement's before/after sql
     * (see data_statement::before_sql()/after_sql()), and it is equally
     * usable by application code that just needs to run one plain statement
     * without the ceremony of preparing a rtl::query<no_params, no_results>.
     * Not a substitute for query<>'s batch execute or fetch - this is for
     * one-shot statements only.
     *
     * @param sql statement to run, with no parameter placeholders
     * @return db_sts::success, or the backend's own error status
     */
    virtual db_sts                    exec(const std::string& sql);
    [[nodiscard]] const db_data_root* data() const;
    [[nodiscard]] logger::Logger&     log_() const { return logger_; } /// Member variables

    /**
     * @brief "host:{} port:{} database:{} user:{}" for whatever connect() last succeeded with
     *
     * Empty until connect(host, port, database_name, user, password) succeeds
     * at least once - never includes the password. Meant for the connect/
     * disconnect log lines every backend's own connect()/disconnect() prints
     * (see db_psql::connect()/disconnect(), db_db2::connect()/disconnect()),
     * so an application-level caller wrapping its own "Connected to ..."
     * message around the same connect() call does not have to repeat host/
     * port/database/user itself.
     */
    [[nodiscard]] std::string connection() const { return connection_info_; }
  protected:
    /// sets what connection() returns - called by a backend's own connect()
    /// once it knows the connection actually succeeded, never before
    void set_connection_info(const std::string& host, uint16_t port, const std::string& database_name, const std::string& user)
    { connection_info_ = fmt::format("host:{} port:{} database:{} user:{}", host, port, database_name, user); }
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
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
    logger::Logger& logger_; ///< reference to the shared Logger, not owner
  private:
    std::string connection_info_; ///< see connection()/set_connection_info() above
  };
  /**
   * @brief create the database object of the backend that is linked in
   *
   * Declared here, defined by whichever backend library the executable links
   * against (db2_rtl, psql_rtl, ...). This keeps rtl free of any knowledge of
   * the concrete backends and spares the caller a #ifdef.
   *
   * @param log Logger every database operation logs through
   * @return std::unique_ptr<db> connected-to-nothing database object
   */
  [[nodiscard]] std::unique_ptr<db> make_db(logger::Logger& log);

  /**
   * @brief name of the backend compiled into this executable, e.g. "db2"
   *
   * Used to warn when the requested sql dialect does not match the backend
   * that can actually describe the statements.
   */
  [[nodiscard]] std::string_view backend_name() noexcept;

  /**
   * @brief port this backend's server listens on out of the box
   *
   * 50000 for DB2, 5432 for PostgreSQL - used as the command line default so
   * that each executable does the expected thing without being told.
   */
  [[nodiscard]] uint16_t default_port() noexcept;

  /**
   * @brief a 64-bit, monotonically increasing, Twitter-snowflake-style id
   *
   * Layout (MSB to LSB): 41 bits milliseconds since 2025-01-01T00:00:00Z,
   * 10 bits thread, 12 bits sequence. The sequence resets to 0 every time
   * the millisecond changes and increments (mod 4096) within it, so up to
   * 4096 ids can be generated per thread per millisecond before the call
   * blocks-in-place (busy-waits) for the next millisecond to roll over -
   * comfortably enough for a caller inserting one eng_state/cc_state row
   * per invocation. Ordering only holds within one thread value: two
   * different threads can produce ids out of timestamp order relative to
   * each other, same as real snowflake ids across nodes.
   *
   * @param thread caller-chosen 10-bit slot (0-1023) - callers that never
   *        run concurrently with each other can all safely pass 0; the
   *        one place this actually matters is two threads that might
   *        generate an id in the same millisecond
   * @return a new, unique, mostly-increasing id
   */
  [[nodiscard]] uint64_t unique_id(uint16_t thread) noexcept;

  /**
   * @brief Set the value to the n-th row
   *
   * @tparam CharT type of the array element (char, wchar_t or uint8_t)
   * @tparam NetCapacity
   * @param value value to be set
   * @param row row in buffer, to set the value
   * @param len_vec length of the value (for strings)
   * @param data_vec place to set the new value
   */
  template <typename CharT, size_t NetCapacity, size_t dim = 1>
  void set_value(const std::basic_string_view<CharT>&      value,
                 size_t                                    row,
                 std::span<int32_t>                        len_vec,
                 std::span<std::array<CharT, NetCapacity>> data_vec)
  {
    // clang-format off
    static_assert(
      std::is_same_v<CharT, char> ||
      std::is_same_v<CharT, wchar_t> ||
      std::is_same_v<CharT, unsigned char> || // uint8_t is sometimes typedef for unsigned char
      std::is_same_v<CharT, std::uint8_t>,
      "CharT must be char, wchar_t or uint8_t only!");
      assert(len_vec.size() == data_vec.size()); //Both dimensions must be the same.
    // clang-format on
    if (value.size() > NetCapacity) [[unlikely]]
      throw std::out_of_range(fmt::format("Value is too long. provided: {} maximum allowed {}", value.size(), NetCapacity));
    if (data_vec.size() < row) [[unlikely]]
      throw std::out_of_range(fmt::format("Row is too long. provided: {} maximum allowed {}", row, data_vec.size()));

    len_vec[row] = static_cast<int32_t>(value.size()); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    auto& buf    = data_vec[row]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) - std::span has no .at()
    value.copy(buf.data(), value.size(), 0);
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index)
    // value.size() is bounded by NetCapacity (checked above), but that bound
    // is a run time value here, not a compile time constant - std::array's
    // operator[] wants the latter to avoid this diagnostic, which a fixed
    // NetCapacity does not give it.
    // clang-format off
    if constexpr (std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>) buf[value.size()] = CharT(0); // safety null // NOLINT(readability-inconsistent-ifelse-braces)
    // clang-format on
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index)
    else
    {
      // Binary string - no need for trailing zero
    };
  }

}; // namespace rtl
