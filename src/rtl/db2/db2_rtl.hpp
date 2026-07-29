#pragma once

#include <expected>
#include <string>
#include <vector>
#include "rtl.hpp"
#include "db2_types.hpp" // IWYU pragma: export - brings in sqlcli1.h
#include <common.hpp>

constexpr auto DATA_ALIGNMENT_128 = 128;
constexpr auto DATA_ALIGNMENT_64  = 64;
constexpr auto DATA_ALIGNMENT_16  = 16;

#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)

namespace rtl
{
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
    class log::log* log_() { return log::get(); }; /// Member variables

  }; //__attribute__((aligned(DATA_ALIGNMENT_64)));

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
    db_sts commit() override;
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
    e_qry_metadata get_sql_metadata(const std::string& sql) override;

    /**
     * @brief Binds a column in the result set to a variable.
     *
     * This method wraps SQLBindCol and binds a result set column to a buffer.
     * It uses the current statement handle from db_data_db2.
     *
     * @param column_number 1-based index of the column.
     * @param target_type C data type (SQL_C_* constant).
     * @param target_value Pointer to the buffer where data will be stored.
     * @param buffer_length Length of the buffer in bytes.
     * @param str_len_or_ind Pointer to length/indicator buffer.
     * @return db_sts Status of the operation.
     */
    db_sts bind_col(uint16_t column_number, int16_t target_type, SQLPOINTER target_value, int32_t buffer_length, int32_t* str_len_or_ind);
    /**
     * @brief Binds a parameter in the SQL statement to a variable.
     *
     * This method wraps SQLBindParameter and binds an input/output parameter.
     * It uses the current statement handle from db_data_db2.
     *
     * @param parameter_number 1-based index of the parameter.
     * @param input_output_type SQL_PARAM_INPUT, SQL_PARAM_OUTPUT, etc.
     * @param value_type C data type (SQL_C_* constant).
     * @param parameter_type SQL data type (use sql_type enum).
     * @param column_size Precision/column size.
     * @param decimal_digits Scale/decimal digits.
     * @param parameter_value_ptr Pointer to the parameter value buffer.
     * @param buffer_length Length of the buffer in bytes.
     * @param str_len_or_ind_ptr Pointer to length/indicator buffer.
     * @return db_sts Status of the operation.
     */
    db_sts bind_param(uint16_t parameter_number,
                      int16_t  input_output_type,
                      int16_t  value_type,
                      sql_type parameter_type,
                      uint32_t column_size,
                      int16_t  decimal_digits,
                      void*    parameter_value_ptr,
                      int32_t  buffer_length,
                      int32_t* str_len_or_ind_ptr);


    void free_stmt_handle() const; ///< release current statement handle NOLINT
    void free_conn_handle() const; ///< free connection handle NOLINT
    void free_env_handle() const;  ///< free environment handle NOLINT
  private:
    ///
    db_sts                 internal_connect(const std::string& connStr);
    db_sts                 internal_allocate_handles();
    static class log::log* log_() { return log::get(); }; /// Member variables
    /// access to the database attributes
    [[nodiscard]] db_data_db2* data() const;
    e_qry_metadata             error_cleanup(SQLRETURN ret, const std::string& msg, db_sts err_code);
  }; // db_db2;

} // namespace rtl