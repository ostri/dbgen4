#pragma once

#include <sqlcli1.h>
#include <string>
#include <vector>
#include "rtl.hpp"
#include "cli_constants.hpp"

constexpr auto align_64 = 64;
constexpr auto align_16 = 16;
namespace rtl
{
  // db2_rtl.hpp – namespace rtl

  /**
   * @brief Description of a single result-set column
   */
  struct column_description
  {
    std::string   name;           ///< Column name as returned by the database
    sql_data_type type;           ///< Mapped type from dbgen4::sql_data_type
    SQLSMALLINT   sql_type;       ///< Raw ODBC SQL type code (e.g., SQL_INTEGER)
    SQLULEN       column_size;    ///< Maximum column size in characters/bytes
    SQLSMALLINT   decimal_digits; ///< Number of digits after decimal point (for numeric)
    SQLSMALLINT   nullable;       ///< SQL_NO_NULLS, SQL_NULLABLE, or SQL_NULLABLE_UNKNOWN
  } __attribute__((aligned(align_64)));

  /**
   * @brief Description of a single parameter in a prepared statement
   */
  struct parameter_description
  {
    SQLSMALLINT   parameter_number; ///< 1-based index of the parameter
    sql_data_type type;             ///< Mapped type from dbgen4::sql_data_type
    SQLSMALLINT   sql_type;         ///< Raw ODBC SQL type code
    SQLULEN       parameter_size;   ///< Maximum size of the parameter value
    SQLSMALLINT   decimal_digits;   ///< Decimal precision
    SQLSMALLINT   nullable;         ///< SQL_NO_NULLS or SQL_NULLABLE
  } __attribute__((aligned(align_16)));

  /**
   * @brief Result of parsing a SQL statement – contains only metadata
   */
  struct query_metadata
  {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    db_sts status = db_sts::error; ///< Execution status
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    std::vector<column_description> columns; ///< Result-set column metadata
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    std::vector<parameter_description> parameters; ///< Input parameter metadata

    /**
     * @brief Check if metadata extraction was successful
     * @return true if status is success or success_with_info
     */
    [[nodiscard]] bool success() const noexcept { return is_success(status); }
  } __attribute__((aligned(align_64)));
  class db_data_db2 : public db_data_root
  {
    static constexpr const std::size_t DB2_DATA_ALIGNMENT = 16;
  public:
    SQLHENV  env_handle{};  // NOLINT(misc-non-private-member-variables-in-classes)
    SQLHDBC  conn_handle{}; // NOLINT(misc-non-private-member-variables-in-classes)
    SQLHSTMT stmt_handle{}; // NOLINT(misc-non-private-member-variables-in-classes)

    db_data_db2() = default;
    ~db_data_db2() override;
    db_data_db2(const db_data_db2&)            = delete;
    db_data_db2& operator=(const db_data_db2&) = delete;
    db_data_db2(db_data_db2&&)                 = delete;
    db_data_db2& operator=(db_data_db2&&)      = delete;
  private:
    [[nodiscard]] auto log() const { return log::get(); }

  } __attribute__((aligned(DB2_DATA_ALIGNMENT)));

  class db_db2 : public db
  {
  public:
    db_db2();
    ~db_db2() override;
    db_db2(const db_db2&)            = delete;
    db_db2& operator=(const db_db2&) = delete;
    db_db2(db_db2&&)                 = delete;
    db_db2& operator=(db_db2&&)      = delete;

    /**
     * @brief Establishes a connection to the database.
     * @return db_sts Status code indicating the result of the connection attempt.
     *
     * This method should be overridden by derived classes to implement
     * database-specific connection logic.
     */
    db_sts connect(const std::string& name) override;
    db_sts connect(const std::string& host,
                   const std::string& port,
                   const std::string& database_name,
                   const std::string& user,
                   const std::string& password) override;
    /**
     * @brief Disconnects from the database.
     * @return db_sts Status code indicating the result of the disconnection attempt.
     * This method should be overridden by derived classes to implement
     * database-specific disconnection logic.
     */
    db_sts disconnect() override;
    /**
     * @brief Checks if the database connection is currently established.
     * @return true if connected, false otherwise.
     */
    [[nodiscard]] bool is_connected() const override;
    /**
     * @brief Commits the current transaction.
     * @return db_sts Status code indicating the result of the commit operation.
     */
    db_sts commit() override; // Zamenjano z override
    /**
     * @brief Roll back the current transaction
     * @return db_sts Status code indicating the result of the rollback operation
     *
     * This method should be overridden by derived classes to implement
     * database-specific rollback logic.
     */
    db_sts rollback() override;
    /**
     * @brief Parses a SQL statement and returns metadata about columns and parameters
     * @param query SQL statement (may contain ? placeholders)
     * @return query_metadata with status, column and parameter descriptions
     */
    query_metadata get_sql_metadata(const std::string& query);
  private:
    ///
    db_sts             internal_connect(const std::string& connStr);
    db_sts             internal_allocate_handles();
    [[nodiscard]] auto log() const { return log::get(); }


    // Metoda za izvajanje SQL poizvedb, ki vračajo rezultate
    std::vector<std::vector<std::string>> executeQuery(const std::string& query);

    // Metoda za izvajanje SQL ukazov brez rezultatov
    void executeNonQuery(const std::string& query);
    /// check what is wrong and report to the log
    void chk_error(SQLRETURN          ret,
                   SQLSMALLINT        handleType,
                   SQLHANDLE          handle,
                   const std::string& operation) const;
    /// access to the database attributes
    [[nodiscard]] db_data_db2* data() const { return dynamic_cast<db_data_db2*>(data_.get()); };
  }; // db_db2;
} // namespace rtl