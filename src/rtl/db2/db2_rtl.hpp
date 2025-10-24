#pragma once

#include <sqlcli1.h>
// #include <sql.h>
// #include <sqlext.h>
// #include <sqltypes.h>
#include <string>
// #include <memory>
// #include <stdexcept>
#include <vector>
#include "rtl.hpp"
namespace rtl
{

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
    db_sts connect(const std::string& host,
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
    db_sts commit() override { return db_sts::success; }
    /**
     * @brief Roll back the current transaction
     * @return db_sts Status code indicating the result of the rollback operation
     *
     * This method should be overridden by derived classes to implement
     * database-specific rollback logic.
     */
    db_sts rollback() override { return db_sts::success; }
  private:
    // Metoda za izvajanje SQL poizvedb, ki vračajo rezultate
    std::vector<std::vector<std::string>> executeQuery(const std::string& query);

    // Metoda za izvajanje SQL ukazov brez rezultatov
    void executeNonQuery(const std::string& query);


    // Pomožna metoda za preverjanje ODBC napak
    void                       checkError(SQLRETURN          ret,
                                          SQLSMALLINT        handleType,
                                          SQLHANDLE          handle,
                                          const std::string& operation) const;
    [[nodiscard]] db_data_db2* data() const { return dynamic_cast<db_data_db2*>(data_.get()); };
  }; // db_db2;
} // namespace rtl
