// psql_rtl.cpp
#include "psql_rtl.hpp"
#include "rtl.hpp"
#include <common.hpp> // rtl::lowercase
#include <fmt/format.h>
#include <poll.h>
#include <cerrno>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>

namespace
{
  /// name given to the throw-away prepared statement used for describing
  constexpr const char* describe_stmt_name = "dbgen4_describe";

  /**
   * @brief RAII holder for a PGresult
   *
   * libpq hands out results that must be cleared on every path, including the
   * error ones - which is exactly where it is easiest to forget.
   */
  class result_guard
  {
  public:
    explicit result_guard(PGresult* r) noexcept
    : res_(r)
    {
    }
    ~result_guard() { PQclear(res_); }
    result_guard(const result_guard&)            = delete;
    result_guard& operator=(const result_guard&) = delete;
    result_guard(result_guard&&)                 = delete;
    result_guard& operator=(result_guard&&)      = delete;

    [[nodiscard]] PGresult*      get() const noexcept { return res_; }
    [[nodiscard]] ExecStatusType status() const noexcept { return PQresultStatus(res_); }
    [[nodiscard]] std::string    error() const { return (res_ != nullptr) ? PQresultErrorMessage(res_) : "no result"; }
  private:
    PGresult* res_;
  };
} // namespace

namespace rtl
{
  db_data_psql::~db_data_psql()
  {
    if (conn != nullptr) logger_.critical("libpq connection is not closed.");
  }

  db_psql::db_psql(logger::Logger& log)
  : db(log)
  { this->data_ = std::make_unique<db_data_psql>(log); }
  db_psql::~db_psql() { disconnect(); }

  db_data_psql* db_psql::data() const { return dynamic_cast<db_data_psql*>(data_.get()); }

  PGconn* db_psql::get_conn() const noexcept { return data()->conn; }

  bool db_psql::is_connected() const
  {
    auto* d = data();
    return d != nullptr && d->conn != nullptr && PQstatus(d->conn) == CONNECTION_OK;
  }

  db_sts db_psql::connect(const std::string& conn_str)
  {
    if (data()->conn != nullptr)
    {
      log_().error("Already connected - disconnect first.");
      return db_sts::connection_error;
    }

    PGconn* conn = PQconnectdb(conn_str.c_str());
    if (conn == nullptr)
    {
      log_().critical("PQconnectdb returned no connection object - out of memory?");
      return db_sts::memory_error;
    }
    if (PQstatus(conn) != CONNECTION_OK)
    {
      log_().error("Connection failed: {}", PQerrorMessage(conn));
      PQfinish(conn);
      return db_sts::connection_error;
    }
    data()->conn     = conn;
    data()->conn_str = conn_str; ///< see db_data_psql::conn_str's own doc comment for why

    /// mirror the db2 backend: no autocommit, the generator only reads metadata
    /// and rolls back, so nothing it does can ever touch the database
    return begin_transaction();
  }

  db_sts db_psql::connect(const std::string& host,
                          uint16_t           port,
                          const std::string& database_name,
                          const std::string& user,
                          const std::string& password)
  {
    // clang-format off
    std::string conn_str;
    conn_str += ! host.empty()          ? fmt::format("host={} ",   host)          : "";
    conn_str += port != 0               ? fmt::format("port={} ",   port)          : "";
    conn_str += ! database_name.empty() ? fmt::format("dbname={} ", database_name) : "";
    conn_str += ! user.empty()          ? fmt::format("user={} ",   user)          : "";
    /// logged before the password is appended - it must never reach the log
    log_().debug("connection string: '{}'", conn_str);
    conn_str += ! password.empty()      ? fmt::format("password={} ", password)    : "";
    // clang-format on

    auto ret = connect(conn_str);
    if (ret == db_sts::success)
    {
      set_connection_info(host, port, database_name, user);
      log_().debug("db connected:     {}", connection());
    }
    return ret;
  }

  db_sts db_psql::disconnect()
  {
    auto* d = data();
    if (d == nullptr || d->conn == nullptr) return db_sts::success;

    /// stop_listen_worker()-equivalent, inlined: on_change() may never have been called, in which
    /// case listen_worker_ is not joinable and this is a no-op - see start_listen_worker()'s own
    /// "lazily, once" doc comment
    stop_listen_.store(true, std::memory_order_relaxed);
    if (listen_worker_.joinable()) listen_worker_.join();
    if (listen_conn_ != nullptr)
    {
      PQfinish(listen_conn_);
      listen_conn_ = nullptr;
    }

    rollback();
    PQfinish(d->conn);
    d->conn = nullptr;
    log_().debug("db disconnected: {}", connection());
    return db_sts::success;
  }

  db_sts db_psql::exec(const std::string& sql) { return exec_command(sql.c_str()); }

  db_sts db_psql::exec_command(const char* sql)
  {
    if (! is_connected())
    {
      log_().error("'{}' attempted on a disconnected database.", sql);
      return db_sts::connection_error;
    }
    const result_guard res{PQexec(data()->conn, sql)};
    if (res.status() != PGRES_COMMAND_OK)
    {
      log_().error("'{}' failed: {}", sql, res.error());
      return db_sts::transaction_error;
    }
    return db_sts::success;
  }

  db_sts db_psql::begin_transaction()
  {
    auto ret = exec_command("BEGIN");
    if (ret == db_sts::success) log_().debug("Transaction opened (autocommit off).");
    return ret;
  }

  db_sts db_psql::commit()
  {
    auto ret = exec_command("COMMIT");
    if (ret != db_sts::success) return ret;
    log_().debug("Transaction committed successfully.");
    return begin_transaction(); // stay inside a transaction, as db2 does
  }

  db_sts db_psql::rollback()
  {
    auto ret = exec_command("ROLLBACK");
    if (ret != db_sts::success) return ret;
    log_().debug("Transaction rolled back.");
    return begin_transaction();
  }

  /**
   * @brief VACUUM ANALYZE table_name, wrapped in a bare COMMIT/BEGIN pair sent via exec_command()
   * directly, NOT commit()/begin_transaction() (see rtl::db::refresh_statistics()'s own doc
   * comment for the caller-facing contract).
   *
   * PostgreSQL rejects VACUUM (plain or combined with ANALYZE) with "VACUUM cannot run inside a
   * transaction block" - confirmed directly. This connection is ALWAYS inside a transaction from
   * connect() onward (see that method's own "mirror the db2 backend: no autocommit" comment), so
   * VACUUM ANALYZE needs that transaction closed first. commit() itself does not do this: it
   * immediately reopens one right after COMMIT ("stay inside a transaction, as db2 does" - see
   * that method's own comment above) before a caller's own next statement would ever run, which
   * would leave the connection back inside a transaction by the time VACUUM ANALYZE itself ran -
   * confirmed directly (ach's own cb_ach_exporter::on_init(), first attempt, hit exactly this).
   * exec_command("COMMIT") has no such reopen (it is what commit() itself calls, before commit()'s
   * own extra begin_transaction()), so the connection is genuinely outside a transaction for
   * exactly the VACUUM ANALYZE statement, then explicitly put back into one afterwards
   * ("BEGIN") to restore the invariant every other caller on this connection relies on (a plain
   * SELECT/INSERT is never expected to open its own transaction). The trailing BEGIN runs even if
   * VACUUM ANALYZE itself failed - an un-reopened connection is the more urgent problem for every
   * later caller either way, so that failure (if BEGIN also fails) is reported instead of the (by
   * then moot) VACUUM ANALYZE one.
   *
   * table_name is NOT escaped/quoted - see rtl::db::refresh_statistics()'s own doc comment on why
   * (a compile-time-known name, never end-user input).
   */
  db_sts db_psql::refresh_statistics(const std::string& table_name)
  {
    if (const auto ret = exec_command("COMMIT"); ret != db_sts::success) return ret;
    const auto vacuum_ret = exec_command(fmt::format("VACUUM ANALYZE {}", table_name).c_str());
    const auto begin_ret  = exec_command("BEGIN");
    if (begin_ret != db_sts::success) return begin_ret;
    return vacuum_ret;
  }

  std::string db_psql::channel_name(const std::string& table_name) { return fmt::format("dbgen4_on_change_{}", table_name); }

  /**
   * @brief CREATE OR REPLACE FUNCTION + CREATE TRIGGER wiring table_name's own INSERT/UPDATE/DELETE
   * into `NOTIFY channel_name(table_name)` - see on_change()'s own doc comment for the whole picture.
   *
   * Both statements are idempotent (`CREATE OR REPLACE FUNCTION`, `DROP TRIGGER IF EXISTS` before
   * `CREATE TRIGGER`), so calling on_change() again for a table_name already wired is harmless - the
   * trigger/function are simply redefined to the same body. Runs on THIS connection (the caller's
   * own), not listen_conn_ - DDL has nothing to do with the dedicated listening connection, which
   * exists only to receive the NOTIFY this trigger sends, on whichever connection(s) are LISTENing.
   *
   * `pg_notify()`'s own payload carries the operation (`TG_OP` - `'INSERT'`/`'UPDATE'`/`'DELETE'`)
   * so listen_loop() does not need a second round trip to learn it.
   *
   * Commits on success, rolls back on failure - own its own unit of work rather than leaving that
   * to the caller (unlike a plain exec_command(), which leaves the surrounding transaction open,
   * same as every other statement on this connection). on_change() itself is a "do this and it is
   * durably registered, or it is not registered at all" call, not one step of a caller-managed
   * transaction the way a generated qry's insert/update is - a caller with no other reason to call
   * commit()/rollback() around this specific call would otherwise leave the DDL sitting in an open
   * transaction indefinitely (confirmed directly: a plugin that calls on_change() once at startup
   * and never commits() afterwards left the trigger/function uncommitted, invisible to any other
   * session, until the connection eventually closed and rolled it back).
   *
   * table_name is NOT escaped/quoted - same convention as refresh_statistics()'s own table_name
   * (see rtl::db::refresh_statistics()'s own doc comment): a compile-time-known name, never
   * end-user input.
   */
  db_sts db_psql::create_change_trigger(const std::string& table_name)
  {
    const auto channel      = channel_name(table_name);
    const auto function_sql = fmt::format("CREATE OR REPLACE FUNCTION {0}_fn() RETURNS trigger AS $$ "
                                          "BEGIN PERFORM pg_notify('{0}', TG_OP); RETURN NULL; END; $$ LANGUAGE plpgsql",
                                          channel);
    if (const auto ret = exec_command(function_sql.c_str()); ret != db_sts::success)
    {
      rollback();
      return ret;
    }

    const auto drop_sql = fmt::format("DROP TRIGGER IF EXISTS {0}_trg ON {1}", channel, table_name);
    if (const auto ret = exec_command(drop_sql.c_str()); ret != db_sts::success)
    {
      rollback();
      return ret;
    }

    const auto create_sql = fmt::format("CREATE TRIGGER {0}_trg AFTER INSERT OR UPDATE OR DELETE ON {1} "
                                        "FOR EACH ROW EXECUTE FUNCTION {0}_fn()",
                                        channel,
                                        table_name);
    if (const auto ret = exec_command(create_sql.c_str()); ret != db_sts::success)
    {
      rollback();
      return ret;
    }

    return commit();
  }

  /**
   * @brief start listen_worker_ the first time on_change() is called on this connection - a no-op
   * every time after (listen_worker_.joinable() is already true), same "lazily, once" shape
   * async_db's own worker construction has, except this one is started from inside on_change()
   * itself rather than from a factory, since on_change() itself can be called more than once.
   */
  db_sts db_psql::start_listen_worker()
  {
    if (listen_worker_.joinable()) return db_sts::success; ///< already running

    /// its own connection, deliberately not data()->conn: LISTEN state and PQnotifies() polling
    /// must not compete with the main connection's own query traffic, the same "one connection, one
    /// thing at a time" constraint async_db's own file comment states for query execution
    PGconn* conn = PQconnectdb(data()->conn_str.c_str());
    if (conn == nullptr)
    {
      log_().critical("on_change: PQconnectdb (listen connection) returned no connection object - out of memory?");
      return db_sts::memory_error;
    }
    if (PQstatus(conn) != CONNECTION_OK)
    {
      log_().error("on_change: listen connection failed: {}", PQerrorMessage(conn));
      PQfinish(conn);
      return db_sts::connection_error;
    }
    listen_conn_ = conn;

    try
    {
      listen_worker_ = std::thread([this] { listen_loop(); });
    }
    catch (const std::system_error& e)
    {
      log_().error("on_change: could not start the listen worker thread: {}", e.what());
      PQfinish(listen_conn_);
      listen_conn_ = nullptr;
      return db_sts::os_error;
    }
    return db_sts::success;
  }

  /**
   * @brief listen_worker_'s own body - poll()s listen_conn_'s socket, and on every wakeup drains
   * PQnotifies(), dispatching each one to the handler registered (in handlers_) for its channel.
   *
   * PQsocket()/PQconsumeInput()/PQnotifies() is libpq's own documented pattern for receiving
   * NOTIFYs without polling the database itself - PQconsumeInput() reads whatever the server has
   * sent into libpq's own buffer (does not block once the socket is readable), and PQnotifies()
   * then pops one already-buffered notification at a time, returning nullptr once none are left.
   *
   * poll()'s own timeout (1s) is only a bound on how promptly stop_listen_ is noticed after
   * disconnect() sets it - not a polling interval for notifications themselves, which are only
   * ever discovered by the socket actually becoming readable.
   */
  void db_psql::listen_loop()
  {
    while (! stop_listen_.load(std::memory_order_relaxed))
    {
      pollfd pfd{.fd = PQsocket(listen_conn_), .events = POLLIN, .revents = 0};
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers) - 1s stop-check granularity
      const int ready = poll(&pfd, 1, 1000);
      if (ready < 0)
      {
        if (errno == EINTR) continue;
        log_().error("on_change: poll() on the listen connection failed: {}", std::error_code(errno, std::generic_category()).message());
        return;
      }
      if (ready == 0) continue; ///< timed out - just a chance to re-check stop_listen_

      if (PQconsumeInput(listen_conn_) == 0)
      {
        log_().error("on_change: PQconsumeInput failed: {}", PQerrorMessage(listen_conn_));
        return;
      }

      for (PGnotify* n = PQnotifies(listen_conn_); n != nullptr; n = PQnotifies(listen_conn_))
      {
        const std::string channel(n->relname);
        const std::string op(n->extra); ///< TG_OP: "INSERT"/"UPDATE"/"DELETE" - see create_change_trigger()
        PQfreemem(n);

        change_handler handler;
        {
          const std::scoped_lock lock{listen_mtx_};
          if (const auto it = handlers_.find(channel); it != handlers_.end()) handler = it->second;
        }
        if (! handler) continue; ///< notify for a channel nobody (any more) cares about

        if (op == "INSERT") handler(change_op::insert);
        else if (op == "UPDATE") handler(change_op::update);
        else if (op == "DELETE") handler(change_op::remove);
        else log_().warn("on_change: unrecognized TG_OP '{}' on channel '{}'", op, channel);
      }
    }
  }

  /**
   * @brief LISTEN/NOTIFY-backed rtl::db::on_change() - see that method's own doc comment for the
   * caller-facing contract.
   *
   * Three parts, in order: (1) create_change_trigger() wires table_name's own writes into a
   * `NOTIFY` on this connection (DDL, ordinary transaction), (2) start_listen_worker() lazily opens
   * the dedicated listening connection and its worker thread the first time this is called,
   * (3) `LISTEN <channel>` is issued on listen_conn_ and handler is recorded in handlers_ under
   * that channel name - listen_loop() (already running by now, or about to be) picks it up from
   * there without any further coordination.
   *
   * handler runs on listen_worker_'s own thread, never on the caller's - a caller whose handler
   * touches shared state must synchronize it itself, the same obligation any other callback-based
   * API places on its caller.
   */
  db_sts db_psql::on_change(const std::string& table_name, const change_handler& handler)
  {
    if (const auto ret = create_change_trigger(table_name); ret != db_sts::success) return ret;
    if (const auto ret = start_listen_worker(); ret != db_sts::success) return ret;

    const auto channel = channel_name(table_name);
    {
      const result_guard res{PQexec(listen_conn_, fmt::format("LISTEN {}", channel).c_str())};
      if (res.status() != PGRES_COMMAND_OK)
      {
        log_().error("on_change: LISTEN {} failed: {}", channel, res.error());
        return db_sts::connection_error;
      }
    }

    const std::scoped_lock lock{listen_mtx_};
    handlers_[channel] = handler; ///< a copy - handler itself is a caller-owned const reference
    return db_sts::success;
  }

  /**
   * @brief describe a statement using the extended query protocol
   *
   * PQprepare parses and plans the statement server side without running it;
   * PQdescribePrepared then reports the resolved type of every parameter and
   * every result column. Both are exact - unlike ODBC's SQLDescribeParam,
   * there is no guessing involved.
   */
  e_qry_metadata db_psql::get_sql_metadata(const std::string& sql)
  {
    if (! is_connected()) [[unlikely]]
    {
      log_().error("get_sql_metadata: No active database connection");
      return std::unexpected(db_sts::connection_error);
    }

    /// a previous describe may have left the statement name taken
    {
      const result_guard drop{PQexec(data()->conn, "DEALLOCATE ALL")};
    }

    {
      /// param types all zero - let the server infer them from the statement
      const result_guard prep{PQprepare(data()->conn, describe_stmt_name, sql.c_str(), 0, nullptr)};
      if (prep.status() != PGRES_COMMAND_OK)
      {
        log_().error("Invalid sql '{}': {}", sql, prep.error());
        return std::unexpected(db_sts::invalid_sql);
      }
    }

    const result_guard desc{PQdescribePrepared(data()->conn, describe_stmt_name)};
    if (desc.status() != PGRES_COMMAND_OK)
    {
      log_().error("Describe failed for sql '{}': {}", sql, desc.error());
      return std::unexpected(db_sts::invalid_sql);
    }

    qry_metadata result{};

    // --- parameters ---
    const int num_params = PQnparams(desc.get());
    log_().debug("Parameter set has {} parameters", num_params);
    for (int i = 0; i < num_params; ++i)
    {
      const auto oid = static_cast<psql::oid_t>(PQparamtype(desc.get(), i));

      schema::meta_dscr par{};
      par.index       = static_cast<int16_t>(i + 1);
      par.name        = fmt::format("par_{}", i + 1);
      par.native_type = static_cast<int32_t>(oid);
      par.type        = psql::from_oid(oid);
      /// libpq reports no modifier for parameters, only the resolved type
      par.size     = psql::column_width(oid, -1, -1);
      par.digits   = 0;
      par.nullable = 1; ///< PostgreSQL does not report parameter nullability
      if (par.type == schema::sql_type::unknown)
        log_().warn("Parameter {} has an unmapped type OID {}. Generated code will not compile.", i + 1, oid);
      result.add_par_dscr(par);
    }

    // --- result columns ---
    const int num_columns = PQnfields(desc.get());
    log_().debug("Result set has {} columns", num_columns);
    for (int i = 0; i < num_columns; ++i)
    {
      const auto oid    = static_cast<psql::oid_t>(PQftype(desc.get(), i));
      const auto typmod = PQfmod(desc.get(), i);
      const auto size   = PQfsize(desc.get(), i);

      schema::meta_dscr col{};
      col.index       = static_cast<int16_t>(i + 1);
      col.name        = rtl::lowercase(PQfname(desc.get(), i));
      col.native_type = static_cast<int32_t>(oid);
      col.type        = psql::from_oid(oid);
      col.size        = psql::column_width(oid, typmod, size);
      col.digits      = psql::column_scale(oid, typmod);
      /// PQdescribePrepared does not carry nullability - assume nullable
      col.nullable = 1;
      if (col.type == schema::sql_type::unknown)
        log_().warn("Column '{}' has an unmapped type OID {}. Generated code will not compile.", col.name, oid);
      result.add_col_dscr(col);
    }

    {
      const result_guard drop{PQexec(data()->conn, "DEALLOCATE ALL")};
    }
    return result;
  }

  // ------------------------------------------------------------------------
  // backend registration - see the declarations in rtl.hpp
  // ------------------------------------------------------------------------
  std::unique_ptr<db> make_db(logger::Logger& log) { return std::make_unique<db_psql>(log); }

  std::string_view backend_name() noexcept { return "psql"; }

  uint16_t default_port() noexcept { return 5432; } // NOLINT(readability-magic-numbers)

} // namespace rtl
