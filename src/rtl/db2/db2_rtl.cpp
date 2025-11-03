#include "log.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include <fmt/format.h>
#include <array>
#include <stdexcept>
namespace
{

  /// check what is wrong and report to the log
  void chk_error(SQLRETURN          ret,
                 SQLSMALLINT        handleType,
                 SQLHANDLE          handle,
                 const std::string& operation)
  {
    constexpr const int sql_state_len = 5 + 1;
    constexpr const int msg_len       = 1024 + 1;
    auto                res           = SQL_SUCCESS;
    log::get()->trace("{} status: {}", operation, ret);
    if (! is_success(static_cast<rtl::db_sts>(ret)))
    {
      std::array<SQLCHAR, sql_state_len> sqlState{};
      SQLINTEGER                         native_error;
      std::array<SQLCHAR, msg_len>       messageText{};
      SQLSMALLINT                        messageLength = 0;
      std::string                        err_msg{};
      SQLSMALLINT                        rec_number = 1;

      while (! is_no_data(static_cast<rtl::db_sts>(res)))
      {
        res = SQLGetDiagRec(handleType,
                            handle,
                            rec_number,
                            sqlState.data(),
                            &native_error,
                            messageText.data(),
                            messageText.size(),
                            &messageLength);

        err_msg = fmt::format(R"(
  Error in {} 
  db error {} 
  native err {}
)",
                              operation,
                              std::string(messageText.begin(), messageText.begin() + messageLength),
                              native_error);
        log::get()->error(err_msg);
        rec_number++;
      };
    }
  }
  void free_handle(SQLHSTMT h, SQLSMALLINT h_type, const char* info, const char* err)
  {
    if (h != 0)
    {
      auto ret = SQLFreeHandle(h_type, h);
      auto tmp = fmt::format(fmt::runtime(err), h);
      if (! is_success(static_cast<rtl::db_sts>(ret))) chk_error(ret, h_type, h, tmp);
    }
    log::get()->debug(fmt::runtime(info), h);
  }
}; // namespace
namespace rtl
{

  db_data_db2::~db_data_db2()
  {
    if (stmt_handle != 0) log()->critical("statement handle is not deallocated.");
    if (conn_handle != 0) log()->critical("connection handle is not deallocated.");
    if (env_handle != 0) log()->critical("environment handle is not deallocated.");
  }

  db_db2::db_db2() { this->data_ = std::make_unique<db_data_db2>(); }
  db_db2::~db_db2() { disconnect(); }


  db_sts db_db2::internal_connect(const std::string& connStr)
  {
    SQLCHAR     outConnStr[1024]; // NOLINT
    SQLSMALLINT outLen;
    log()->debug("Connecting with: {}", connStr);
    auto ret = SQLDriverConnect(data()->conn_handle,
                                nullptr,
                                (SQLCHAR*)connStr.c_str(), // NOLINT
                                SQL_NTS,
                                outConnStr, // NOLINT
                                sizeof(outConnStr),
                                &outLen,
                                SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLDriverConnect");
      SQLFreeHandle(SQL_HANDLE_DBC, data()->conn_handle);
      SQLFreeHandle(SQL_HANDLE_ENV, data()->env_handle);
      data()->conn_handle = 0;
      data()->env_handle  = 0;
      return db_sts::connection_error;
    }

    return db_sts::success;
  }

  db_sts db_db2::internal_allocate_handles()
  {
    auto ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &data()->env_handle);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_ENV, data()->env_handle, "Env allocation failed");
      return db_sts::env_error;
    }

    log()->debug("ENV handle allocated: {}", data()->env_handle);

    // 3. Alociraj DBC
    ret = SQLAllocHandle(SQL_HANDLE_DBC, data()->env_handle, &data()->conn_handle);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLAllocHandle(DBC)");
      free_env_handle();
      return db_sts::connection_error;
    }
    log()->debug("Connection handle allocated: {}", data()->conn_handle);

    // Dodana nastavitev za izklop Auto-commit-a (SQL_AUTOCOMMIT_OFF)
    ret = SQLSetConnectAttr(data()->conn_handle,
                            SQL_ATTR_AUTOCOMMIT,
                            (SQLPOINTER)SQL_AUTOCOMMIT_OFF, // NOLINT
                            SQL_IS_INTEGER);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLSetConnectAttr(AUTOCOMMIT OFF)");
      return db_sts::config_error;
    }
    log()->debug("Autocommit disabled.");
    return db_sts::success;
  }
  spdlog::logger* db_db2::log() const { return log::get(); }
  ///
  db_sts db_db2::connect(const std::string& conn_str) { return internal_connect(conn_str); }
  db_sts db_db2::connect(const std::string& host,
                         uint16_t           port,
                         const std::string& name,
                         const std::string& user,
                         const std::string& pass)
  {
    auto ret = internal_allocate_handles();
    if (ret == db_sts::success)
    {
      std::string conn_str = "DRIVER={{IBM DB2 ODBC DRIVER}}; "
                             "PROTOCOL=TCPIP; "
                             "CURRENTFUNCTIONPATH=CURRENT PATH; ";
      // clang-format off
      conn_str += !host.empty() ? fmt::format("HOSTNAME={}; ", host) : "";
      conn_str += port != 0     ? fmt::format("PORT={}; ",     port) : "";
      conn_str += !name.empty() ? fmt::format("DATABASE={}; ", name) : "";
      conn_str += !user.empty() ? fmt::format("UID={}; ",      user) : "";
      conn_str += !pass.empty() ? fmt::format("PWD={}; ",      pass) : "";
      // clang-format on

      ret = internal_connect(conn_str);
      log()->info("Connected to database host:{}:{} db {} as user {}", host, port, name, user);
    }
    return ret;
  }
  db_sts db_db2::disconnect()
  {
    SQLRETURN ret = SQL_SUCCESS;
    if (data()->stmt_handle != 0)
    { // statement handle still active; rollback and disconnect
      rollback();
      free_stmt_handle();
    }
    if (data()->conn_handle != 0)
    {
      ret = SQLDisconnect(data()->conn_handle);
      if (! is_success(static_cast<db_sts>(ret)))
        chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "disconnecting from DB2 database");
      free_conn_handle();
      log()->info("Database disconnected");
    }

    if (data()->env_handle != 0) free_env_handle();
    return static_cast<db_sts>(ret);
  }

  bool db_db2::is_connected() const { return this->data()->conn_handle != 0; }

  // Implementacija metode commit()
  db_sts db_db2::commit()
  {
    if (data()->conn_handle == 0)
    {
      log()->error("Attempted to commit on a disconnected database.");
      return db_sts::connection_error;
    }

    SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, data()->conn_handle, SQL_COMMIT);
    chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "commit transaction");

    if (is_success(static_cast<db_sts>(ret)))
    {
      log()->info("Transaction committed successfully.");
    }
    else { log()->error("Transaction commit failed."); }

    return static_cast<db_sts>(ret);
  }

  // Implementacija metode rollback()
  db_sts db_db2::rollback()
  {
    if (data()->conn_handle == 0)
    {
      log()->error("Attempted to rollback on a disconnected database.");
      return db_sts::connection_error;
    }

    SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, data()->conn_handle, SQL_ROLLBACK);
    chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "rollback transaction");

    if (is_success(static_cast<db_sts>(ret)))
    {
      log()->info("Transaction rolled back successfully.");
    }
    else { log()->error("Transaction rollback failed."); }

    return static_cast<db_sts>(ret);
  }

  std::vector<std::vector<std::string>> db_db2::executeQuery(const std::string& query)
  {
    if (data()->conn_handle == 0) { throw std::runtime_error("No active database connection"); }

    // Inicializacija statement ročaja
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "allocating statement handle");

    // Izvedba poizvedbe
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast, google-readability-casting)
    ret = SQLExecDirect(data()->stmt_handle, (SQLCHAR*)query.c_str(), SQL_NTS);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "executing query");

    // Pridobivanje števila stolpcev
    SQLSMALLINT num_columns;
    ret = SQLNumResultCols(data()->stmt_handle, &num_columns);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "getting number of columns");

    // Shranjevanje rezultatov
    constexpr SQLSMALLINT                 buffer_size = 1024;
    std::array<SQLCHAR, buffer_size>      buffer{};
    SQLLEN                                indicator;
    std::vector<std::vector<std::string>> results;
    while ((ret = SQLFetch(data()->stmt_handle)) == SQL_SUCCESS)
    {
      std::vector<std::string> row;
      for (SQLSMALLINT i = 1; i <= num_columns; ++i)
      {
        ret =
          SQLGetData(data()->stmt_handle, i, SQL_C_CHAR, buffer.data(), buffer.size(), &indicator);
        chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "getting column data");
        row.push_back(indicator == SQL_NULL_DATA
                        ? "NULL"
                        : std::string(buffer.begin(), buffer.begin() + indicator));
      }
      results.push_back(row);
    }

    return results;
  }

  void db_db2::executeNonQuery(const std::string& sql)
  {
    if (data()->conn_handle == 0) { throw std::runtime_error("No active database connection"); }

    // Inicializacija statement ročaja

    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "allocating statement handle");

    // clang-format off
    // Izvedba ukaza
    ret = SQLExecDirect(
      data()->stmt_handle, 
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-type-const-cast)
      SQL_NTS);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "executing non-query");
    // clang-format on
  }
  // db2_rtl.cpp – inside namespace rtl

  qry_metadata db_db2::get_sql_metadata(const std::string& sql)
  {
    qry_metadata result;

    if (data()->conn_handle == 0)
    {
      log()->error("get_sql_metadata: No active database connection");
      result.status = db_sts::connection_error;
      return result;
    }

    // --- Allocate statement handle ---
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "allocating statement handle");
      result.status = db_sts::resource_error;
      return result;
    }

    // --- Prepare the statement (no execution) ---
    auto tmp = reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())); // NOLINT
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions, bugprone-narrowing-conversions)
    SQLINTEGER len = sql.size();
    ret            = SQLPrepare(data()->stmt_handle, tmp, len);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      auto msg = fmt::format("{} sql {}", "SQLPrepare", sql);
      chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, msg);
      SQLFreeHandle(SQL_HANDLE_STMT, data()->stmt_handle);
      data()->stmt_handle = 0;
      result.status       = db_sts::invalid_sql;
      return result;
    }

    // --- Describe input parameters ---
    SQLSMALLINT num_params = 0;
    ret                    = SQLNumParams(data()->stmt_handle, &num_params);
    auto msg               = fmt::format("{} sql {}", "SQLNumParams", sql);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, msg);

    for (SQLSMALLINT i = 1; i <= num_params; ++i)
    {
      meta_dscr param{};
      param.index = i;

      SQLSMALLINT data_type  = 0;
      SQLSMALLINT dec_digits = 0;
      SQLSMALLINT nullable   = 0;
      SQLULEN     param_size = 0;

      ret =
        SQLDescribeParam(data()->stmt_handle, i, &data_type, &param_size, &dec_digits, &nullable);

      if (is_success(static_cast<db_sts>(ret)))
      {
        param.sql_type = data_type;
        param.type     = static_cast<sql_type>(data_type);
        param.size     = param_size;
        param.digits   = dec_digits;
        param.nullable = nullable;
        result.params.push_back(param);
      }
      else
      {
        chk_error(ret,
                  SQL_HANDLE_STMT,
                  data()->stmt_handle,
                  fmt::format("SQLDescribeParam for parameter {}", i));
        // Continue — partial metadata is acceptable
      }
    }

    // --- Describe result-set columns ---
    SQLSMALLINT num_columns = 0;
    ret                     = SQLNumResultCols(data()->stmt_handle, &num_columns);
    msg                     = fmt::format("{} sql {}", "SQLNumResultCols", sql);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, msg);

    for (SQLSMALLINT i = 1; i <= num_columns; ++i)
    {
      meta_dscr                col{};
      std::array<SQLCHAR, 256> col_name{}; // NOLINT
      SQLSMALLINT              name_len       = 0;
      SQLSMALLINT              data_type      = 0;
      SQLSMALLINT              decimal_digits = 0;
      SQLSMALLINT              nullable       = 0;
      SQLULEN                  column_size    = 0;

      ret = SQLDescribeCol(data()->stmt_handle,
                           i,
                           col_name.data(),
                           col_name.size(),
                           &name_len,
                           &data_type,
                           &column_size,
                           &decimal_digits,
                           &nullable);

      if (is_success(static_cast<db_sts>(ret)))
      {
        col.name     = std::string(col_name.begin(), col_name.begin() + name_len);
        col.sql_type = data_type;
        col.type     = static_cast<sql_type>(data_type);
        col.size     = column_size;
        col.digits   = decimal_digits;
        col.nullable = nullable;
        result.columns.push_back(col);
      }
      else
      {
        chk_error(ret,
                  SQL_HANDLE_STMT,
                  data()->stmt_handle,
                  fmt::format("SQLDescribeCol for column {}", i));
      }
    }

    // --- Cleanup ---
    SQLFreeHandle(SQL_HANDLE_STMT, data()->stmt_handle);
    data()->stmt_handle = 0;

    result.status = db_sts::success;
    return result;
  }

  void db_db2::free_stmt_handle() const
  {
    auto            h      = data()->stmt_handle;
    constexpr auto  h_type = SQL_HANDLE_STMT;
    constexpr auto* info   = "statement handle {} deallocated.";
    constexpr auto* err    = "error deallocating statement handle {}";
    data()->stmt_handle    = 0;
    free_handle(h, h_type, info, err);
  }

  void db_db2::free_conn_handle() const
  {
    auto            h      = data()->conn_handle;
    constexpr auto  h_type = SQL_HANDLE_DBC;
    constexpr auto* info   = "connection handle {} deallocated.";
    constexpr auto* err    = "error deallocating connection handle {}";
    data()->conn_handle    = 0;

    free_handle(h, h_type, info, err);
  }

  void db_db2::free_env_handle() const
  {
    auto            h      = data()->env_handle;
    constexpr auto  h_type = SQL_HANDLE_ENV;
    constexpr auto* info   = "environment handle {} deallocated.";
    constexpr auto* err    = "error deallocating environment handle {}";
    data()->env_handle     = 0;

    free_handle(h, h_type, info, err);
  }


  bool qry_metadata::success() const noexcept { return is_success(status); }
}; // namespace rtl
