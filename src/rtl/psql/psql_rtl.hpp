// psql_rtl.hpp
#pragma once
/**
 * @file
 * @brief PostgreSQL backend built on libpq
 *
 * Where the db2 backend asks an ODBC driver to describe a statement, this one
 * uses the PostgreSQL extended query protocol directly: PQprepare followed by
 * PQdescribePrepared yields the exact type OID of every parameter and every
 * result column, with no driver guesswork in between.
 */

#include "rtl.hpp"
#include "psql_types.hpp"
#include <libpq-fe.h>
#include <string>

namespace rtl
{
  /**
   * @brief the libpq connection handle, hidden behind db_data_root
   */
  class db_data_psql : public db_data_root
  {
  public:
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PGconn* conn{nullptr}; ///< libpq connection, owned

    db_data_psql() = default;
    ~db_data_psql() override;
    db_data_psql(const db_data_psql&)            = delete;
    db_data_psql& operator=(const db_data_psql&) = delete;
    db_data_psql(db_data_psql&&)                 = delete;
    db_data_psql& operator=(db_data_psql&&)      = delete;
  private:
    static class log::log* log_() { return log::get(); };
  };

  class db_psql : public db
  {
  public:
    db_psql();
    ~db_psql() override;
    db_psql(const db_psql&)            = delete;
    db_psql& operator=(const db_psql&) = delete;
    db_psql(db_psql&&)                 = delete;
    db_psql& operator=(db_psql&&)      = delete;

    /// connect using a libpq conninfo string
    db_sts connect(const std::string& conn_str) override;
    db_sts connect(const std::string& host,
                   uint16_t           port,
                   const std::string& database_name,
                   const std::string& user,
                   const std::string& password) override;
    db_sts             disconnect() override;
    [[nodiscard]] bool is_connected() const override;
    db_sts             commit() override;
    db_sts             rollback() override;

    /**
     * @brief describe a statement without executing it
     *
     * Parameters are written as $1, $2, ... in PostgreSQL - the yaml file is
     * expected to carry a psql specific statement for anything with parameters.
     */
    e_qry_metadata get_sql_metadata(const std::string& sql) override;
  private:
    static class log::log* log_() { return log::get(); };
    [[nodiscard]] db_data_psql* data() const;
    /// run a statement that returns no rows (BEGIN/COMMIT/ROLLBACK)
    db_sts exec_command(const char* sql);
    /// start the explicit transaction that mirrors the db2 backend's autocommit-off
    db_sts begin_transaction();
  };

} // namespace rtl
