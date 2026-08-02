// psql_rtl.cpp
#include "psql_rtl.hpp"
#include "rtl.hpp"
#include <common.hpp> // rtl::lowercase
#include <fmt/format.h>
#include <memory>
#include <string>

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
    if (conn != nullptr) log_()->critical("libpq connection is not closed.");
  }

  db_psql::db_psql() { this->data_ = std::make_unique<db_data_psql>(); }
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
      log_()->error("Already connected - disconnect first.");
      return db_sts::connection_error;
    }

    PGconn* conn = PQconnectdb(conn_str.c_str());
    if (conn == nullptr)
    {
      log_()->critical("PQconnectdb returned no connection object - out of memory?");
      return db_sts::memory_error;
    }
    if (PQstatus(conn) != CONNECTION_OK)
    {
      log_()->error("Connection failed: {}", PQerrorMessage(conn));
      PQfinish(conn);
      return db_sts::connection_error;
    }
    data()->conn = conn;

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
    log_()->debug("connection string: '{}'", conn_str);
    conn_str += ! password.empty()      ? fmt::format("password={} ", password)    : "";
    // clang-format on

    auto ret = connect(conn_str);
    if (ret == db_sts::success) log_()->info("Connected to db: host:'{}:{}' db:'{}' as user '{}'", host, port, database_name, user);
    return ret;
  }

  db_sts db_psql::disconnect()
  {
    auto* d = data();
    if (d == nullptr || d->conn == nullptr) return db_sts::success;

    rollback();
    PQfinish(d->conn);
    d->conn = nullptr;
    log_()->info("Database disconnected");
    return db_sts::success;
  }

  db_sts db_psql::exec_command(const char* sql)
  {
    if (! is_connected())
    {
      log_()->error("'{}' attempted on a disconnected database.", sql);
      return db_sts::connection_error;
    }
    const result_guard res{PQexec(data()->conn, sql)};
    if (res.status() != PGRES_COMMAND_OK)
    {
      log_()->error("'{}' failed: {}", sql, res.error());
      return db_sts::transaction_error;
    }
    return db_sts::success;
  }

  db_sts db_psql::begin_transaction()
  {
    auto ret = exec_command("BEGIN");
    if (ret == db_sts::success) log_()->debug("Transaction opened (autocommit off).");
    return ret;
  }

  db_sts db_psql::commit()
  {
    auto ret = exec_command("COMMIT");
    if (ret != db_sts::success) return ret;
    log_()->debug("Transaction committed successfully.");
    return begin_transaction(); // stay inside a transaction, as db2 does
  }

  db_sts db_psql::rollback()
  {
    auto ret = exec_command("ROLLBACK");
    if (ret != db_sts::success) return ret;
    log_()->debug("Transaction rolled back.");
    return begin_transaction();
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
      log_()->error("get_sql_metadata: No active database connection");
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
        log_()->error("Invalid sql '{}': {}", sql, prep.error());
        return std::unexpected(db_sts::invalid_sql);
      }
    }

    const result_guard desc{PQdescribePrepared(data()->conn, describe_stmt_name)};
    if (desc.status() != PGRES_COMMAND_OK)
    {
      log_()->error("Describe failed for sql '{}': {}", sql, desc.error());
      return std::unexpected(db_sts::invalid_sql);
    }

    qry_metadata result{};

    // --- parameters ---
    const int num_params = PQnparams(desc.get());
    log_()->debug("Parameter set has {} parameters", num_params);
    for (int i = 0; i < num_params; ++i)
    {
      const auto oid = static_cast<psql::oid_t>(PQparamtype(desc.get(), i));

      meta_dscr par{};
      par.index       = static_cast<int16_t>(i + 1);
      par.name        = fmt::format("par_{}", i + 1);
      par.native_type = static_cast<int32_t>(oid);
      par.type        = psql::from_oid(oid);
      /// libpq reports no modifier for parameters, only the resolved type
      par.size     = psql::column_width(oid, -1, -1);
      par.digits   = 0;
      par.nullable = 1; ///< PostgreSQL does not report parameter nullability
      if (par.type == sql_type::unknown)
        log_()->warn("Parameter {} has an unmapped type OID {}. Generated code will not compile.", i + 1, oid);
      result.add_par_dscr(par);
    }

    // --- result columns ---
    const int num_columns = PQnfields(desc.get());
    log_()->debug("Result set has {} columns", num_columns);
    for (int i = 0; i < num_columns; ++i)
    {
      const auto oid    = static_cast<psql::oid_t>(PQftype(desc.get(), i));
      const auto typmod = PQfmod(desc.get(), i);
      const auto size   = PQfsize(desc.get(), i);

      meta_dscr col{};
      col.index       = static_cast<int16_t>(i + 1);
      col.name        = rtl::lowercase(PQfname(desc.get(), i));
      col.native_type = static_cast<int32_t>(oid);
      col.type        = psql::from_oid(oid);
      col.size        = psql::column_width(oid, typmod, size);
      col.digits      = psql::column_scale(oid, typmod);
      /// PQdescribePrepared does not carry nullability - assume nullable
      col.nullable = 1;
      if (col.type == sql_type::unknown)
        log_()->warn("Column '{}' has an unmapped type OID {}. Generated code will not compile.", col.name, oid);
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
  std::unique_ptr<db> make_db() { return std::make_unique<db_psql>(); }

  std::string_view backend_name() noexcept { return "psql"; }

  uint16_t default_port() noexcept { return 5432; } // NOLINT(readability-magic-numbers)

} // namespace rtl
