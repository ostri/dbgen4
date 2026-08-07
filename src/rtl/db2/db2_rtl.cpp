//  #include "common.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include <fmt/base.h>
#include <fmt/format.h>
#include <array>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)

// #include <stdexcept>
namespace
{

  /// check what is wrong and report to the log
  void chk_error(SQLRETURN ret, SQLSMALLINT handleType, SQLHANDLE handle, const std::string& operation, logger::Logger& log)
  {
    constexpr const int sql_state_len = 5 + 1;
    constexpr const int msg_len       = 1024 + 1;
    auto                res           = SQL_SUCCESS;
    log.trace("{} status: {}", operation, ret);
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
        res = SQLGetDiagRec(
          handleType, handle, rec_number, sqlState.data(), &native_error, messageText.data(), messageText.size(), &messageLength);

        err_msg = fmt::format(R"(
  Error in {}
  db error {}
  native err {}
)",
                              operation,
                              std::string(messageText.begin(), messageText.begin() + messageLength), // NOLINT
                              native_error);
        log.error(err_msg);
        rec_number++;
      };
    }
  }
  void free_handle(SQLHSTMT h, SQLSMALLINT h_type, const char* info, const char* err, logger::Logger& log)
  {
    if (h != 0)
    {
      auto ret = SQLFreeHandle(h_type, h);
      auto tmp = fmt::format(fmt::runtime(err), h);
      if (! is_success(static_cast<rtl::db_sts>(ret))) chk_error(ret, h_type, h, tmp, log);
    }
    log.trace(fmt::runtime(info), h);
  }
}; // namespace
namespace rtl
{

  db_data_db2::~db_data_db2()
  {
    if (stmt_handle != 0) logger_.critical("statement handle is not deallocated.");
    if (conn_handle != 0) logger_.critical("connection handle is not deallocated.");
    if (env_handle != 0) logger_.critical("environment handle is not deallocated.");
  }

  db_db2::db_db2(logger::Logger& log)
  : db(log)
  , database()
  { this->data_ = std::make_unique<db_data_db2>(log); }
  db_db2::~db_db2() { disconnect(); }


  db_sts db_db2::internal_connect(const std::string& connStr)
  {
    SQLCHAR     outConnStr[1024]; // NOLINT
    SQLSMALLINT outLen;
    auto        ret = SQLDriverConnect(data()->conn_handle,
                                       nullptr,
                                       (SQLCHAR*)connStr.c_str(), // NOLINT
                                       SQL_NTS,
                                       outConnStr, // NOLINT
                                       sizeof(outConnStr),
                                       &outLen,
                                       SQL_DRIVER_NOPROMPT);

    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLDriverConnect", log_());
      free_conn_handle();
      free_env_handle();
      return db_sts::connection_error;
    }

    return db_sts::success;
  }

  db_sts db_db2::internal_allocate_handles()
  {
    auto ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &data()->env_handle);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_ENV, data()->env_handle, "Env allocation failed", log_());
      return db_sts::env_error;
    }

    log_().debug("ENV handle allocated: {}", data()->env_handle);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, data()->env_handle, &data()->conn_handle);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLAllocHandle(DBC)", log_());
      free_env_handle();
      return db_sts::connection_error;
    }
    log_().debug("Connection handle allocated: {}", data()->conn_handle);
    ret = SQLSetConnectAttr(data()->conn_handle,
                            SQL_ATTR_AUTOCOMMIT,
                            (SQLPOINTER)SQL_AUTOCOMMIT_OFF, // NOLINT
                            SQL_IS_INTEGER);
    if (! is_success(static_cast<db_sts>(ret)))
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLSetConnectAttr(AUTOCOMMIT OFF)", log_());
      return db_sts::config_error;
    }
    log_().debug("Autocommit disabled.");
    return db_sts::success;
  }
  ///
  db_sts db_db2::connect(const std::string& conn_str) { return internal_connect(conn_str); }
  db_sts db_db2::connect(const std::string& host, uint16_t port, const std::string& name, const std::string& user, const std::string& pass)
  {
    auto ret = internal_allocate_handles();
    if (ret == db_sts::success)
    {
      /// PATCH2=15 forces the period as the decimal separator.
      ///
      /// Without it the driver formats DECIMAL and NUMERIC using the client's
      /// LC_NUMERIC, so the same column reads back as "1234.56" under a C
      /// locale and "1234,56" under sl_SI - a value that changes with the
      /// environment of whoever runs the program. These types travel as text,
      /// so that difference reaches the caller.
      ///
      /// SQL_ATTR_DECIMAL_SEP is the attribute that looks like it should do
      /// this; the driver rejects it with HY024 whether it is given a string
      /// or a character code. SQL_ATTR_PRESERVE_LOCALE is accepted and has no
      /// effect on the output. PATCH2=15 is what actually works.
      std::string conn_str = "DRIVER={{IBM DB2 ODBC DRIVER}}; "
                             "PROTOCOL=TCPIP; "
                             "PATCH2=15; "
                             "CURRENTFUNCTIONPATH=CURRENT PATH; ";
      // clang-format off
      conn_str += !host.empty() ? fmt::format("HOSTNAME={}; ", host) : "";
      conn_str += port != 0     ? fmt::format("PORT={}; ",     port) : "";
      conn_str += !name.empty() ? fmt::format("DATABASE={}; ", name) : "";
      conn_str += !user.empty() ? fmt::format("UID={}; ",      user) : "";
      /// must be here. we don't want to show the password in log
      log_().debug("connection string: '{}'", conn_str);
      conn_str += !pass.empty() ? fmt::format("PWD={}; ",      pass) : "";
      // clang-format on

      ret = internal_connect(conn_str);
      log_().info("Connected to db: host:'{}:{}' db:'{}' as user '{}'", host, port, name, user);
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
        chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "disconnecting from DB2 database", log_());
      free_conn_handle();
      log_().info("Database disconnected");
    }

    if (data()->env_handle != 0) free_env_handle();
    return static_cast<db_sts>(ret);
  }

  bool db_db2::is_connected() const { return this->data()->conn_handle != 0; }

  /**
   * @brief commit transaction
   *
   * @return db_sts
   */
  db_sts db_db2::commit()
  {
    if (data()->conn_handle == 0)
    {
      log_().error("Attempted to commit on a disconnected database.");
      return db_sts::connection_error;
    }

    const SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, data()->conn_handle, SQL_COMMIT);
    chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "commit transaction", log_());

    if (is_success(static_cast<db_sts>(ret))) { log_().debug("Transaction committed successfully."); }
    else
    {
      log_().error("Transaction commit failed.");
    }

    return static_cast<db_sts>(ret);
  }

  /**
   * @brief db rollback transaction
   *
   * @return db_sts
   */
  db_sts db_db2::rollback()
  {
    if (data()->conn_handle == 0)
    {
      log_().error("Attempted to rollback on a disconnected database.");
      return db_sts::connection_error;
    }

    const SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, data()->conn_handle, SQL_ROLLBACK);
    chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "rollback transaction", log_());

    if (is_success(static_cast<db_sts>(ret))) [[likely]] { log_().info("Transaction rolled back successfully."); }
    else
    {
      log_().error("Transaction rollback failed.");
    }

    return static_cast<db_sts>(ret);
  }

  db_data_db2* db_db2::data() const { return dynamic_cast<db_data_db2*>(data_.get()); };
  /**
   * @brief cleaning stuff before finishing the operation
   *
   * @param ret dli function error code
   * @param msg additional message to be reported
   * @param err_code error code to be reported back
   * @return qry_metadata
   */
  e_qry_metadata db_db2::error_cleanup(SQLRETURN ret, const std::string& msg, db_sts err_code)
  {
    // qry_metadata result{};
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, msg, log_());
    rollback();
    free_stmt_handle();
    return std::unexpected(err_code);
  }

  /**
   * @brief return provided sql statement meta data or error code
   *
   * The method collects the parameter and/or column data from the provided sql statement.
   * Alter all dat ais collected the transaction is rolled back.
   * @param sql sql statement to be analyzed to get the meta data
   * @return qry_metadata metadata or error code
   */
  e_qry_metadata db_db2::get_sql_metadata(const std::string& sql)
  {
    qry_metadata result{};

    if (data()->conn_handle == 0) [[unlikely]]
    {
      log_().error("get_sql_metadata: No active database connection");
      return std::unexpected(db_sts::connection_error);
    }

    // --- Allocate statement handle ---
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
      return error_cleanup(ret, "allocating statement handle", db_sts::resource_error);
    // --- Prepare the statement (no execution) ---
    ret = SQLPrepare(data()->stmt_handle,                                        //
                     reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), // NOLINT
                     SQL_NTS);
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
    {
      auto msg = fmt::format("{} sql {}", "SQLPrepare", sql);
      return error_cleanup(ret, msg, db_sts::invalid_sql);
    }
    // --- Describe input parameters ---
    SQLSMALLINT num_params = 0;
    ret                    = SQLNumParams(data()->stmt_handle, &num_params);
    // auto msg               = fmt::format("{} sql {}", "SQLNumParams", sql);
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
    {
      auto msg = fmt::format("{} sql {}", "SQLNumResultCols", sql);
      return error_cleanup(ret, msg, db_sts::invalid_sql);
    }
    log_().debug("Parameter set has {} parameters", num_params);

    for (SQLSMALLINT i = 1; i <= num_params; ++i)
    {
      // the driver writes through pointers of its own width - describe into
      // native typed locals, then narrow deliberately into meta_dscr
      SQLSMALLINT native_type = 0;
      SQLULEN     size        = 0;
      SQLSMALLINT digits      = 0;
      SQLSMALLINT nullable    = 0;

      ret = SQLDescribeParam(data()->stmt_handle, i, &native_type, &size, &digits, &nullable);
      if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
      {
        auto msg = fmt::format("SQLDescribeParam for parameter {}", i);
        return error_cleanup(ret, msg, db_sts::invalid_sql);
      }

      meta_dscr par{};
      par.index       = i;
      par.name        = fmt::format("par_{}", i);
      par.native_type = native_type;
      par.type        = db2::from_odbc(native_type);
      par.size        = static_cast<uint32_t>(size);
      par.digits      = digits;
      par.nullable    = nullable;
      if (par.type == sql_type::unknown)
        log_().warn("Parameter {} has an unmapped db type code {}. Generated code will not compile.", i, native_type);
      result.add_par_dscr(par);
    }

    // --- result-set columns ---
    SQLSMALLINT num_columns = 0;
    ret                     = SQLNumResultCols(data()->stmt_handle, &num_columns);
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
    {
      auto msg = fmt::format("{} sql {}", "SQLNumResultCols", sql);
      return error_cleanup(ret, msg, db_sts::invalid_sql);
    }
    log_().debug("Result set has {} columns", num_columns);
    for (SQLSMALLINT i = 1; i <= num_columns; ++i)
    {
      std::array<SQLCHAR, 128 + 1> col_name{}; // NOLINT
      SQLSMALLINT                  name_len = 0;
      // describe into native typed locals - see the parameter loop above
      SQLSMALLINT native_type = 0;
      SQLULEN     size        = 0;
      SQLSMALLINT digits      = 0;
      SQLSMALLINT nullable    = 0;

      ret = SQLDescribeCol(data()->stmt_handle, i, col_name.data(), col_name.size(), &name_len, &native_type, &size, &digits, &nullable);
      if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
      {
        auto msg = fmt::format("SQLDescribeCol for column {}", i);
        return error_cleanup(ret, msg, db_sts::invalid_sql);
      }

      meta_dscr col{};
      col.index       = i;
      col.name        = rtl::lowercase(std::string(col_name.begin(), col_name.begin() + name_len)); // NOLINT
      col.native_type = native_type;
      col.type        = db2::from_odbc(native_type);
      col.size        = static_cast<uint32_t>(size);
      col.digits      = digits;
      col.nullable    = nullable;
      if (col.type == sql_type::unknown)
        log_().warn("Column '{}' has an unmapped db type code {}. Generated code will not compile.", col.name, native_type);
      result.add_col_dscr(col);
    }

    // --- Cleanup ---
    rollback();
    free_stmt_handle();

    return result;
  }

  db_sts db_db2::exec(const std::string& sql)
  {
    if (data()->conn_handle == 0) [[unlikely]]
    {
      log_().error("exec: No active database connection");
      return db_sts::connection_error;
    }

    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, data()->conn_handle, &data()->stmt_handle);
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
    {
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "allocating statement handle", log_());
      return db_sts::resource_error;
    }

    ret = SQLExecDirect(data()->stmt_handle, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS); // NOLINT
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
    {
      chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, fmt::format("SQLExecDirect sql '{}'", sql), log_());
      rollback();
      free_stmt_handle();
      return db_sts::invalid_sql;
    }

    free_stmt_handle();
    return db_sts::success;
  }

  void db_db2::free_stmt_handle() const
  {
    auto            h      = data()->stmt_handle;
    constexpr auto  h_type = SQL_HANDLE_STMT;
    constexpr auto* info   = "statement handle {} deallocated.";
    constexpr auto* err    = "error deallocating statement handle {}";
    data()->stmt_handle    = 0;
    free_handle(h, h_type, info, err, log_());
  }

  void db_db2::free_conn_handle() const
  {
    auto            h      = data()->conn_handle;
    constexpr auto  h_type = SQL_HANDLE_DBC;
    constexpr auto* info   = "connection handle {} deallocated.";
    constexpr auto* err    = "error deallocating connection handle {}";
    data()->conn_handle    = 0;

    free_handle(h, h_type, info, err, log_());
  }

  void db_db2::free_env_handle() const
  {
    auto            h      = data()->env_handle;
    constexpr auto  h_type = SQL_HANDLE_ENV;
    constexpr auto* info   = "environment handle {} deallocated.";
    constexpr auto* err    = "error deallocating environment handle {}";
    data()->env_handle     = 0;

    free_handle(h, h_type, info, err, log_());
  }


  // db_sts db_db2::bind_col(uint16_t   column_number,
  //                         int16_t    target_type,
  //                         SQLPOINTER target_value,
  //                         int32_t    buffer_length,
  //                         int32_t*   str_len_or_ind)
  // {
  //   if (! is_connected() || data()->stmt_handle == 0)
  //   {
  //     log_().error("bind_col: No active connection or statement handle");
  //     return db_sts::invalid_handle;
  //   }

  //   SQLRETURN ret = SQLBindCol(
  //     data()->stmt_handle, column_number, target_type, target_value, buffer_length,
  //     str_len_or_ind);

  //   if (! is_success(static_cast<db_sts>(ret)))
  //   {
  //     chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLBindCol", log_());
  //     return static_cast<db_sts>(ret);
  //   }

  //   log_().debug("Column {} bound successfully", column_number);
  //   return db_sts::success;
  // }


  // db_sts db_db2::bind_param(uint16_t parameter_number,
  //                           int16_t  input_output_type,
  //                           int16_t  value_type,
  //                           sql_type parameter_type,
  //                           uint32_t column_size,
  //                           int16_t  decimal_digits,
  //                           void*    parameter_value_ptr,
  //                           int32_t  buffer_length,
  //                           int32_t* str_len_or_ind_ptr)
  // {
  //   if (! is_connected() || data()->stmt_handle == 0)
  //   {
  //     log_().error("bind_param: No active connection or statement handle");
  //     return db_sts::invalid_handle;
  //   }

  //   SQLRETURN ret = SQLBindParameter(data()->stmt_handle,
  //                                    parameter_number,
  //                                    input_output_type,
  //                                    value_type,
  //                                    static_cast<SQLSMALLINT>(parameter_type), // Cast from
  //                                    sql_type column_size, decimal_digits, parameter_value_ptr,
  //                                    buffer_length,
  //                                    str_len_or_ind_ptr);

  //   if (! is_success(static_cast<db_sts>(ret)))
  //   {
  //     chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLBindParameter", log_());
  //     return static_cast<db_sts>(ret);
  //   }

  //   log_().debug("Parameter {} bound successfully", parameter_number);
  //   return db_sts::success;
  // }

  // ------------------------------------------------------------------------
  // backend registration - see the declarations in rtl.hpp
  // ------------------------------------------------------------------------
  SQLHDBC db_db2::get_conn() const noexcept { return data()->conn_handle; }

  std::unique_ptr<db> make_db(logger::Logger& log) { return std::make_unique<db_db2>(log); }

  std::string_view backend_name() noexcept { return "db2"; }

  /// the port a DB2 instance listens on unless it was configured otherwise
  uint16_t default_port() noexcept
  {
    constexpr uint16_t db2_default_listener_port = 50000;
    return db2_default_listener_port;
  }

}; // namespace rtl
