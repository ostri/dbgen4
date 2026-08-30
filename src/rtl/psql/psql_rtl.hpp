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
#include "psql_database.hpp" // IWYU pragma: export
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

    explicit db_data_psql(logger::Logger& log)
    : db_data_root(log)
    {
    }
    ~db_data_psql() override;
    db_data_psql(const db_data_psql&)            = delete;
    db_data_psql& operator=(const db_data_psql&) = delete;
    db_data_psql(db_data_psql&&)                 = delete;
    db_data_psql& operator=(db_data_psql&&)      = delete;
  };

  class db_psql final : public db, public database, public schema::describer
  {
  public:
    explicit db_psql(logger::Logger& log);
    ~db_psql() override;
    db_psql(const db_psql&)            = delete;
    db_psql& operator=(const db_psql&) = delete;
    db_psql(db_psql&&)                 = delete;
    db_psql& operator=(db_psql&&)      = delete;

    /// connect using a libpq conninfo string
    db_sts             connect(const std::string& conn_str) override;
    db_sts             connect(const std::string& host,
                               uint16_t           port,
                               const std::string& database_name,
                               const std::string& user,
                               const std::string& password) override;
    db_sts             disconnect() override;
    [[nodiscard]] bool is_connected() const override;
    db_sts             commit() override;
    db_sts             rollback() override;

    /// this same connection, viewed through rtl::schema::describer - see db::as_describer()
    [[nodiscard]] schema::describer* as_describer() noexcept override { return this; }

    /// run a statement that takes no parameters and returns no rows - see rtl::db::exec()
    db_sts exec(const std::string& sql) override;

    /// VACUUM ANALYZE <table_name>, outside any transaction block - see rtl::db::refresh_statistics()'s
    /// own doc comment and this method's own doc comment (psql_rtl.cpp) for why
    db_sts refresh_statistics(const std::string& table_name) override;

    /// same object, upcast to rtl::database - see rtl::db::as_database()'s own doc comment
    [[nodiscard]] database& as_database() noexcept override { return *this; }

    /// --- rtl::database, so that generated queries can run on this connection
    [[nodiscard]] PGconn* get_conn() const noexcept override;
    /// not owner - borrowed pointer to the shared Logger, never deleted here
    [[nodiscard]] logger::Logger* get_logger() const noexcept override { return &log_(); }

    /**
     * @brief --- rtl::schema::describer - describe a statement without executing it
     *
     * Parameters are written as $1, $2, ... in PostgreSQL - the yaml file is
     * expected to carry a psql specific statement for anything with parameters.
     */
    e_qry_metadata get_sql_metadata(const std::string& sql) override;
  private:
    [[nodiscard]] db_data_psql* data() const;
    /// run a statement that returns no rows (BEGIN/COMMIT/ROLLBACK)
    db_sts exec_command(const char* sql);
    /// start the explicit transaction that mirrors the db2 backend's autocommit-off
    db_sts begin_transaction();
  };

} // namespace rtl
