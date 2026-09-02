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
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

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
    /// the libpq conninfo string connect(conn_str) actually used - kept only so on_change() can
    /// open its own, second connection (see db_psql::on_change()'s own doc comment for why a
    /// dedicated connection is needed) without asking the caller to repeat host/port/database/user/
    /// password a second time. Carries the password in plain text for exactly as long as this
    /// object lives - same exposure connect()'s own conn_str already had transiently, never logged
    /// (see db_psql::connect(host, ...)'s own "logged before the password is appended" comment).
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    std::string conn_str;

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

    /// LISTEN/NOTIFY-backed change notification - see rtl::db::on_change()'s own doc comment for the
    /// caller-facing contract, and this override's own doc comment (psql_rtl.cpp) for the mechanism
    db_sts on_change(const std::string& table_name, const change_handler& handler) override;

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

    /// registers the trigger+function that turns a write on table_name into a `NOTIFY` - runs on
    /// the caller's own thread/connection, once per distinct table_name (idempotent via `CREATE OR
    /// REPLACE`/`DROP TRIGGER IF EXISTS`) - see on_change()'s own doc comment (psql_rtl.cpp)
    db_sts create_change_trigger(const std::string& table_name);
    /// starts listen_worker_ the first time on_change() is ever called on this connection - a
    /// no-op on every later call, same "start lazily, once" shape async_db's own worker has
    db_sts start_listen_worker();
    /// listen_worker_'s own body - blocks in poll() on its own PGconn's socket, dispatching
    /// PQnotifies() payloads to the matching registered handler until stop_listen_ is set
    void listen_loop();

    /// notify channel name for table_name - "dbgen4_on_change_<table_name>", one channel per table
    /// so listen_loop() can route a notify straight to its handler by channel name alone, no
    /// payload parsing needed for that part
    [[nodiscard]] static std::string channel_name(const std::string& table_name);

    std::mutex                                      listen_mtx_;            ///< guards handlers_ and listen_conn_
    std::unordered_map<std::string, change_handler> handlers_;              ///< channel name -> handler
    PGconn*                                         listen_conn_ = nullptr; ///< on_change()'s own dedicated connection, owned
    std::thread                                     listen_worker_;
    std::atomic<bool>                               stop_listen_{false};
  };

} // namespace rtl
