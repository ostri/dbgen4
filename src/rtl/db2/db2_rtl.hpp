#pragma once

#include <sqlcli1.h>
#include <string>
#include <vector>
#include "rtl.hpp"
#include "cli_constants.hpp"
#include <common.hpp>

constexpr auto DATA_ALIGNMENT_128 = 128;
constexpr auto DATA_ALIGNMENT_64  = 64;
constexpr auto DATA_ALIGNMENT_16  = 16;

namespace ME  = magic_enum; // NOLINT(misc-unused-alias-decls)
namespace spd = spdlog;     // NOLINT(misc-unused-alias-decls)
namespace rtl
{
  // db2_rtl.hpp – namespace rtl

  /**
   * @brief Description of a single result-set column
   */
  struct meta_dscr
  {
    int16_t     index;  ///< 1-based index of the parameter
    std::string name;   ///< Column name as returned by the db
                        ///<  or the parameter name provided to the database
    sql_type type;      ///< Mapped type from dbgen4::sql_type
    int16_t  odbc_type; ///< Raw ODBC SQL type code (e.g., SQL_INTEGER)
    uint32_t size;      ///< Maximum column size in characters/bytes
    int16_t  digits;    ///< Number of digits after decimal point (for numeric)
    int16_t  nullable;  ///< SQL_NO_NULLS, SQL_NULLABLE, or SQL_NULLABLE_UNKNOWN
  } __attribute__((aligned(DATA_ALIGNMENT_64)));
  using meta_vec = std::vector<meta_dscr>;

  // /**
  //  * @brief Description of a single parameter in a prepared statement
  //  */
  // struct param_dscr
  // {
  //   SQLSMALLINT   param_number; ///< 1-based index of the parameter
  //   sql_data_type type;         ///< Mapped type from dbgen4::sql_data_type
  //   SQLSMALLINT   sql_type;     ///< Raw ODBC SQL type code
  //   SQLULEN       param_size;   ///< Maximum size of the parameter value
  //   SQLSMALLINT   dec_digits;   ///< Decimal precision
  //   SQLSMALLINT   nullable;     ///< SQL_NO_NULLS or SQL_NULLABLE
  // } __attribute__((aligned(DATA_ALIGNMENT_16)));


  /**
   * @brief Result of parsing a SQL statement – contains only metadata
   */
  class qry_metadata
  {
  public:
    /// getters
    [[nodiscard]] std::string id() const;
    [[nodiscard]] std::string sql() const;
    [[nodiscard]] db_sts      status() const;
    [[nodiscard]] meta_vec    columns() const;
    [[nodiscard]] meta_vec    params() const;
    [[nodiscard]] bool        is_success() const noexcept;
    [[nodiscard]] std::string dump() const;
    std::string dump_meta_vector(const char* fmt, const char* header, const meta_vec& v) const;
    /// setters
    void set_id(const std::string& id);
    void set_sql(const std::string& sql);
    void set_status(const db_sts& status);
    void set_columns(const meta_vec& columns_);
    void set_params(const meta_vec& params_);
    void add_col_dscr(const meta_dscr& dscr);
    void add_par_dscr(const meta_dscr& dscr);
  private:
    std::string id_;
    std::string sql_;
    db_sts      status_{db_sts::error}; ///< Execution status
    meta_vec    columns_;               ///< Result-set column metadata
    meta_vec    params_;                ///< Input parameter metadata
  } __attribute__((aligned(DATA_ALIGNMENT_128)));

  class db_data_db2 : public db_data_root
  {
  public:
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    SQLHENV  env_handle{};  ///< environment handle
    SQLHDBC  conn_handle{}; ///< connection handle
    SQLHSTMT stmt_handle{}; ///< statement handle

    // NOLINTEND(misc-non-private-member-variables-in-classes)

    db_data_db2() = default;
    ~db_data_db2() override;
    db_data_db2(const db_data_db2&)            = delete;
    db_data_db2& operator=(const db_data_db2&) = delete;
    db_data_db2(db_data_db2&&)                 = delete;
    db_data_db2& operator=(db_data_db2&&)      = delete;
  private:
    [[nodiscard]] spdlog::logger* log() const;

  } __attribute__((aligned(DATA_ALIGNMENT_64)));

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
    db_sts connect(const std::string& conn_str) override;
    db_sts connect(const std::string& host,
                   uint16_t           port,
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
    qry_metadata get_sql_metadata(const std::string& sql);

    void free_stmt_handle() const; ///< release current statement handle NOLINT
    void free_conn_handle() const; ///< free connection handle NOLINT
    void free_env_handle() const;  ///< free environment handle NOLINT
  private:
    ///
    db_sts                        internal_connect(const std::string& connStr);
    db_sts                        internal_allocate_handles();
    [[nodiscard]] spdlog::logger* log() const;


    // // Metoda za izvajanje SQL poizvedb, ki vračajo rezultate
    // std::vector<std::vector<std::string>> executeQuery(const std::string& query);

    // // Metoda za izvajanje SQL ukazov brez rezultatov
    // void executeNonQuery(const std::string& sql);

    /// access to the database attributes
    [[nodiscard]] db_data_db2* data() const;

    qry_metadata error_cleanup(SQLRETURN ret, const std::string& msg, db_sts err_code);
  }; // db_db2;

} // namespace rtl