#include "db2_rtl.hpp"
#include "rtl.hpp"
#include <fmt/format.h>
#include <array>
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
    if (! is_success(static_cast<rtl::db_sts>(ret)))
    {
      std::array<SQLCHAR, sql_state_len> sqlState{};
      SQLINTEGER                         nativeError;
      std::array<SQLCHAR, msg_len>       messageText{};
      SQLSMALLINT                        messageLength = 0;
      std::string                        err_msg{};
      SQLSMALLINT                        rec_number = 1;
      SQLRETURN                          res        = SQL_SUCCESS;

      while (! is_no_data(static_cast<rtl::db_sts>(res)))
      {
        res = SQLGetDiagRec(handleType,
                            handle,
                            rec_number,
                            sqlState.data(),
                            &nativeError,
                            messageText.data(),
                            messageText.size(),
                            &messageLength);

        err_msg = fmt::format("Error in '{}': '{}' (SQLSTATE: {})\n",
                              operation,
                              std::string(messageText.begin(), messageText.begin() + messageLength),
                              std::string(sqlState.begin(), sqlState.end()));
        log::get()->error(err_msg);

        rec_number++;
      };
    }
  }
}; // namespace
namespace rtl
{

  db_data_db2::~db_data_db2()
  {
    if (env_handle != 0) SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
    if (conn_handle != 0) SQLFreeHandle(SQL_HANDLE_DBC, conn_handle);
    if (stmt_handle != 0) SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
  }

  db_db2::db_db2() { this->data_ = std::make_unique<db_data_db2>(); }
  db_db2::~db_db2() { disconnect(); }


  db_sts db_db2::internal_connect(const std::string& connStr)
  {
    SQLCHAR     outConnStr[1024]; // NOLINT
    SQLSMALLINT outLen;
    log()->info("Connecting with: {}", connStr);
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

    log()->info("Connected to DB2 successfully!");
    return db_sts::success;
  }

  db_sts db_db2::internal_allocate_handles()
  {
    auto ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &data()->env_handle);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      log()->error("SQLAllocHandle(ENV) failed: {}", ret);
      return db_sts::env_error;
    }

    log()->info("ENV handle allocated: {}", data()->env_handle);

    // 3. Alociraj DBC
    ret = SQLAllocHandle(SQL_HANDLE_DBC, data()->env_handle, &data()->conn_handle);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLAllocHandle(DBC)");
      SQLFreeHandle(SQL_HANDLE_ENV, data()->env_handle);
      data()->env_handle = 0;
      return db_sts::connection_error;
    }

    // Dodana nastavitev za izklop Auto-commit-a (SQL_AUTOCOMMIT_OFF)
    ret = SQLSetConnectAttr(data()->conn_handle,
                            SQL_ATTR_AUTOCOMMIT,
                            (SQLPOINTER)SQL_AUTOCOMMIT_OFF, // NOLINT
                            SQL_IS_INTEGER);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLSetConnectAttr(AUTOCOMMIT OFF)");
      return db_sts::config_error;
    }
    return db_sts::success;
  }
  ///
  db_sts db_db2::connect(const std::string& name)
  {
    return connect("localhost", "50000", name, "", "");
  }
  db_sts db_db2::connect(const std::string& host,
                         const std::string& port,
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
      conn_str += !port.empty() ? fmt::format("PORT={}; ",     port) : "";
      conn_str += !name.empty() ? fmt::format("DATABASE={}; ", name) : "";
      conn_str += !user.empty() ? fmt::format("UID={}; ",      user) : "";
      conn_str += !pass.empty() ? fmt::format("PWD={}; ",      pass) : "";
      // clang-format off

      return internal_connect(conn_str);
    }
    return ret;
  }
  db_sts db_db2::disconnect()
  {
    SQLRETURN ret = SQL_SUCCESS;
    if (data()->stmt_handle != 0)
    { // statement handle still active; rollback and disconnect

      rollback(); // TODO(ostri):
    }
    if (data()->conn_handle != 0)
    {
      ret = SQLDisconnect(data()->conn_handle);
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "disconnecting from DB2 database");
      SQLFreeHandle(SQL_HANDLE_DBC, data()->conn_handle);
      data()->conn_handle = 0;
      log()->info("Database disconnected");
    }
    else log()->warn("Disconnecting already disconnected database");

    if (data()->env_handle != 0)
    {
      SQLFreeHandle(SQL_HANDLE_ENV, data()->env_handle);
      data()->env_handle = 0;
    }
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

    if (is_success(static_cast<db_sts>(ret))) { log()->info("Transaction committed successfully."); }
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

    if (is_success(static_cast<db_sts>(ret))) { log()->info("Transaction rolled back successfully."); }
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

  void db_db2::executeNonQuery(const std::string& query)
  {
    if (data()->conn_handle == 0) { throw std::runtime_error("No active database connection"); }

    // Inicializacija statement ročaja

    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "allocating statement handle");

    // clang-format off
    // Izvedba ukaza
    ret = SQLExecDirect(
      data()->stmt_handle, 
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-type-const-cast)
      SQL_NTS);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "executing non-query");
    // clang-format on
  }
  // db2_rtl.cpp – inside namespace rtl

  qry_metadata db_db2::get_sql_metadata(const std::string& query)
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
    auto tmp = reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())); // NOLINT
    ret      = SQLPrepare(data()->stmt_handle, tmp, SQL_NTS);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLPrepare");
      SQLFreeHandle(SQL_HANDLE_STMT, data()->stmt_handle);
      data()->stmt_handle = 0;
      result.status       = db_sts::invalid_sql;
      return result;
    }

    // --- Describe input parameters ---
    SQLSMALLINT num_params = 0;
    ret                    = SQLNumParams(data()->stmt_handle, &num_params);
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLNumParams");

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
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLNumResultCols");

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

  bool qry_metadata::success() const noexcept { return is_success(status); }
}; // namespace rtl
