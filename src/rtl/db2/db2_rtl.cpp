#include "db2_rtl.hpp"
#include "rtl.hpp"
#include <fmt/format.h>
#include <array>
// #include <sstream>
// #include <iomanip>
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

  void db_db2::checkError(SQLRETURN          ret,
                          SQLSMALLINT        handleType,
                          SQLHANDLE          handle,
                          const std::string& operation) const
  {
    constexpr const int sql_state_len = 5 + 1;
    constexpr const int msg_len       = 1024 + 1;
    if (! is_success(static_cast<db_sts>(ret)))
    {
      std::array<SQLCHAR, sql_state_len> sqlState{};
      SQLINTEGER                         nativeError;
      std::array<SQLCHAR, msg_len>       messageText{};
      SQLSMALLINT                        messageLength = 0;
      std::string                        err_msg{};
      SQLSMALLINT                        rec_number = 1;
      SQLRETURN                          res        = SQL_SUCCESS;

      while (! is_no_data(static_cast<db_sts>(res)))
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
        l->error(err_msg);

        rec_number++;
      };
    }
  }

  // db_sts db_db2::connect(const std::string& host,
  //                        const std::string& database_name,
  //                        const std::string& user,
  //                        const std::string& password)
  // {
  //   // Inicializacija ODBC okolja
  //   SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &data()->env_handle);
  //   checkError(ret, SQL_HANDLE_ENV, data()->env_handle, "allocating environment handle");
  //   // clang-format off
  //   // Nastavitev verzije ODBC na 3
  //   SQLINTEGER ver = SQL_OV_ODBC3_80;
  //   ret = SQLSetEnvAttr(
  //     data()->env_handle,
  //     SQL_ATTR_ODBC_VERSION,
  //     reinterpret_cast<SQLPOINTER>(&ver),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  //     0);
  //   checkError(ret, SQL_HANDLE_ENV, data()->env_handle, "setting ODBC version");
  //   // clang-format on

  //   // Inicializacija povezave
  //   ret = SQLAllocHandle(SQL_HANDLE_DBC, data()->env_handle, &data()->conn_handle);
  //   checkError(ret, SQL_HANDLE_DBC, data()->conn_handle, "allocating connection handle");

  //   // Sestavljanje povezovalnega niza za DB2
  //   auto connStr =
  //     fmt::format("DRIVER={{IBM DB2 ODBC DRIVER}};HOSTNAME={};DATABASE={};UID={};PWD={};",
  //                 host,
  //                 database_name,
  //                 user,
  //                 password);
  //   // clang-format off
  //   // Povezava z bazo podatkov
  //   ret = SQLDriverConnect(data()->conn_handle,
  //                          nullptr,
  //                          reinterpret_cast<SQLCHAR*>(connStr.data()), //
  //                          NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) SQL_NTS, nullptr,
  //                          0,
  //                          nullptr,
  //                          SQL_DRIVER_NOPROMPT);
  //   checkError(ret, SQL_HANDLE_DBC, data()->conn_handle, "connecting to DB2 database");
  //   // clang-format on
  //   return static_cast<db_sts>(ret);
  // }
  db_sts db_db2::connect(const std::string& host,
                         const std::string& database_name,
                         const std::string& user,
                         const std::string& password)
  {
    SQLRETURN ret;

    // 1. Alociraj ENV
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &data()->env_handle);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      l->error("SQLAllocHandle(ENV) failed: {}", ret);
      return db_sts::env_error;
    }

    l->info("ENV handle allocated: {}", data()->env_handle);

    // 2. BREZ SQLSetEnvAttr → verzija nastavljena v db2cli.ini

    // 3. Alociraj DBC
    ret = SQLAllocHandle(SQL_HANDLE_DBC, data()->env_handle, &data()->conn_handle);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      checkError(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLAllocHandle(DBC)");
      SQLFreeHandle(SQL_HANDLE_ENV, data()->env_handle);
      data()->env_handle = 0;
      return db_sts::connection_error;
    }

    // 4. Connection string – uporabi pravilen DRIVER ime
    std::string connStr = fmt::format("DRIVER={{IBM DB2 ODBC DRIVER}}; "
                                      "HOSTNAME={}; "
                                      "PORT=50000; "
                                      "DATABASE={}; "
                                      "UID={}; "
                                      "PWD={}; "
                                      "PROTOCOL=TCPIP; "
                                      "CURRENTFUNCTIONPATH=CURRENT PATH;",
                                      host,
                                      database_name,
                                      user,
                                      password);

    l->info("Connecting with: {}", connStr);

    // 5. Poveži se
    SQLCHAR     outConnStr[1024]; // NOLINT
    SQLSMALLINT outLen;
    ret = SQLDriverConnect(data()->conn_handle,
                           nullptr,
                           (SQLCHAR*)connStr.c_str(), // NOLINT
                           SQL_NTS,
                           outConnStr, // NOLINT
                           sizeof(outConnStr),
                           &outLen,
                           SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
      checkError(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLDriverConnect");
      SQLFreeHandle(SQL_HANDLE_DBC, data()->conn_handle);
      SQLFreeHandle(SQL_HANDLE_ENV, data()->env_handle);
      data()->conn_handle = 0;
      data()->env_handle  = 0;
      return db_sts::connection_error;
    }

    l->info("Connected to DB2 successfully!");
    return db_sts::success;
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
      checkError(ret, SQL_HANDLE_DBC, data()->conn_handle, "disconnecting from DB2 database");
      SQLFreeHandle(SQL_HANDLE_DBC, data()->conn_handle);
      data()->conn_handle = 0;
      l->info("Database disconnected");
    }
    else l->warn("Disconnecting already disconnected database");

    if (data()->env_handle != 0)
    {
      SQLFreeHandle(SQL_HANDLE_ENV, data()->env_handle);
      data()->env_handle = 0;
    }
    return static_cast<db_sts>(ret);
  }

  bool db_db2::is_connected() const { return this->data()->conn_handle != 0; }

  std::vector<std::vector<std::string>> db_db2::executeQuery(const std::string& query)
  {
    if (data()->conn_handle == 0) { throw std::runtime_error("No active database connection"); }

    // Inicializacija statement ročaja
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    checkError(ret, SQL_HANDLE_STMT, data()->stmt_handle, "allocating statement handle");

    // Izvedba poizvedbe
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast, google-readability-casting)
    ret = SQLExecDirect(data()->stmt_handle, (SQLCHAR*)query.c_str(), SQL_NTS);
    checkError(ret, SQL_HANDLE_STMT, data()->stmt_handle, "executing query");

    // Pridobivanje števila stolpcev
    SQLSMALLINT num_columns;
    ret = SQLNumResultCols(data()->stmt_handle, &num_columns);
    checkError(ret, SQL_HANDLE_STMT, data()->stmt_handle, "getting number of columns");

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
        checkError(ret, SQL_HANDLE_STMT, data()->stmt_handle, "getting column data");
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
    checkError(ret, SQL_HANDLE_STMT, data()->stmt_handle, "allocating statement handle");

    // clang-format off
    // Izvedba ukaza
    ret = SQLExecDirect(
      data()->stmt_handle, 
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-type-const-cast)
      SQL_NTS);
    checkError(ret, SQL_HANDLE_STMT, data()->stmt_handle, "executing non-query");
    // clang-format on
  }
}; // namespace rtl
