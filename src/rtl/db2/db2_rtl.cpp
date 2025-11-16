#include "cli_constants.hpp"
//  #include "common.hpp"
//  #include "log.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include <fmt/base.h>
#include <fmt/format.h>
#include <array>
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
// #include <stdexcept>
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
    log::get()->trace(fmt::runtime(info), h);
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

  spdlog::logger* db_data_db2::log() const { return log::get(); }

  db_db2::db_db2() { this->data_ = std::make_unique<db_data_db2>(); }
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
      chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "SQLDriverConnect");
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
      /// must be here. we don't want to show the password in log
      log()->debug("connection string: '{}'", conn_str);
      conn_str += !pass.empty() ? fmt::format("PWD={}; ",      pass) : "";
      // clang-format on

      ret = internal_connect(conn_str);
      log()->info("Connected to db: host:'{}:{}' db:'{}' as user '{}'", host, port, name, user);
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

  /**
   * @brief db rolback transaction
   *
   * @return db_sts
   */
  db_sts db_db2::rollback()
  {
    if (data()->conn_handle == 0)
    {
      log()->error("Attempted to rollback on a disconnected database.");
      return db_sts::connection_error;
    }

    SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, data()->conn_handle, SQL_ROLLBACK);
    chk_error(ret, SQL_HANDLE_DBC, data()->conn_handle, "rollback transaction");

    if (is_success(static_cast<db_sts>(ret))) [[likely]]
    {
      log()->info("Transaction rolled back successfully.");
    }
    else { log()->error("Transaction rollback failed."); }

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
    chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, msg);
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
      log()->error("get_sql_metadata: No active database connection");
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
    log()->debug("Parameter set has {} parameters", num_params);

    for (SQLSMALLINT i = 1; i <= num_params; ++i)
    {
      meta_dscr par{};
      ret = SQLDescribeParam(
        data()->stmt_handle, i, &par.odbc_type, &par.size, &par.digits, &par.nullable);

      if (is_success(static_cast<db_sts>(ret)))
      {
        par.index = i;
        par.name  = fmt::format("par_{}", i);
        par.type  = static_cast<sql_type>(par.odbc_type);
        result.add_par_dscr(par);
      }
      else [[unlikely]]
      {
        auto msg = fmt::format("SQLDescribeParam for parameter {}", i);
        return error_cleanup(ret, msg, db_sts::invalid_sql);
      }
    }

    // --- result-set columns ---
    SQLSMALLINT num_columns = 0;
    ret                     = SQLNumResultCols(data()->stmt_handle, &num_columns);
    if (! is_success(static_cast<db_sts>(ret))) [[unlikely]]
    {
      auto msg = fmt::format("{} sql {}", "SQLNumResultCols", sql);
      return error_cleanup(ret, msg, db_sts::invalid_sql);
    }
    log()->debug("Result set has {} columns", num_columns);
    for (SQLSMALLINT i = 1; i <= num_columns; ++i)
    {
      meta_dscr                    col{};
      std::array<SQLCHAR, 128 + 1> col_name{}; // NOLINT
      SQLSMALLINT                  name_len = 0;

      ret = SQLDescribeCol(data()->stmt_handle,
                           i,
                           col_name.data(),
                           col_name.size(),
                           &name_len,
                           &col.odbc_type,
                           &col.size,
                           &col.digits,
                           &col.nullable);

      if (is_success(static_cast<db_sts>(ret))) [[likely]]
      {
        col.index = i;
        col.name  = dbgen4::lowercse(std::string(col_name.begin(), col_name.begin() + name_len));
        col.type  = static_cast<sql_type>(col.odbc_type);
        result.add_col_dscr(col);
      }
      else [[unlikely]]
      {
        auto msg = fmt::format("SQLDescribeCol for column {}", i);
        return error_cleanup(ret, msg, db_sts::invalid_sql);
      }
    }

    // --- Cleanup ---
    rollback();
    free_stmt_handle();

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


  // db_sts qry_metadata::status() const { return status_; }

  meta_vec qry_metadata::columns() const { return columns_; }

  // void qry_metadata::set_columns(const meta_vec& columns) { columns_ = columns; }

  meta_vec qry_metadata::params() const { return params_; }

  // void qry_metadata::set_params(const meta_vec& params) { params_ = params; }

  // std::string qry_metadata::dscr() const { return dscr_; }

  // void qry_metadata::set_dscr(const std::string& dscr) { dscr_ = dscr; }

  void qry_metadata::add_col_dscr(const meta_dscr& dscr) { columns_.push_back(dscr); }

  void qry_metadata::add_par_dscr(const meta_dscr& dscr) { params_.push_back(dscr); }

  // std::string qry_metadata::sql() const { return sql_; }

  // qry_metadata::qry_metadata(std::string sql,

  //                            meta_vec columns,
  //                            meta_vec params)
  // : sql_(std::move(sql))

  // , columns_(std::move(columns))
  // , params_(std::move(params))
  // {
  // }

  // std::string qry_metadata::id() const { return id_; }

  //  void qry_metadata::set_id(const std::string& id) { id_ = id; }

  // void qry_metadata::set_status(const db_sts& status) { status_ = status; }

  // void qry_metadata::set_sql(const std::string& sql) { sql_ = sql; }

  // bool qry_metadata::is_success() const noexcept
  // {
  //   return ((db_sts::success == status_) || (db_sts::success_with_info == status_));
  // }

  std::string qry_metadata::dump_meta_vector(const char*     fmt,
                                             const char*     header,
                                             const meta_vec& v) const
  {
    if (! v.empty())
    {
      std::string msg = header;
      for (auto col : v)
      {
        msg += fmt::format(fmt::runtime(fmt),
                           col.index,
                           col.name,
                           ME::enum_name(col.type),
                           get_sql_mapping(col.type)->c_mnemonic,
                           col.odbc_type,
                           col.size,
                           col.digits,
                           col.nullable != 0 ? "yes" : "no");
      }
      return msg;
    }
    return {};
  }
  std::string qry_metadata::dump() const
  {
    constexpr const char* fmt     = "      {:>3} {:<20} {:<18} {:<20} {:>9} {:>4} {:>6} {:^8}\n";
    auto                  msg_hdr = fmt::format(
      fmt, "ndx", "column name", "col type", "cli id", "ODBC type", "size", "digits", "nullable");
    auto col = dump_meta_vector(fmt, msg_hdr.c_str(), columns_);
    auto par = dump_meta_vector(fmt, msg_hdr.c_str(), params_);
    auto msg = fmt::format(R"(
     columns: {}
{}
    parameters: {}
{}
  )",
                           //                           sql_,
                           //                           ME::enum_name<db_sts>(status_),
                           columns_.size(),
                           col,
                           params_.size(),
                           par);
    return msg;
  }


  // db_sts db_db2::bind_col(uint16_t   column_number,
  //                         int16_t    target_type,
  //                         SQLPOINTER target_value,
  //                         int32_t    buffer_length,
  //                         int32_t*   str_len_or_ind)
  // {
  //   if (! is_connected() || data()->stmt_handle == 0)
  //   {
  //     log()->error("bind_col: No active connection or statement handle");
  //     return db_sts::invalid_handle;
  //   }

  //   SQLRETURN ret = SQLBindCol(
  //     data()->stmt_handle, column_number, target_type, target_value, buffer_length,
  //     str_len_or_ind);

  //   if (! is_success(static_cast<db_sts>(ret)))
  //   {
  //     chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLBindCol");
  //     return static_cast<db_sts>(ret);
  //   }

  //   log()->debug("Column {} bound successfully", column_number);
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
  //     log()->error("bind_param: No active connection or statement handle");
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
  //     chk_error(ret, SQL_HANDLE_STMT, data()->stmt_handle, "SQLBindParameter");
  //     return static_cast<db_sts>(ret);
  //   }

  //   log()->debug("Parameter {} bound successfully", parameter_number);
  //   return db_sts::success;
  // }
}; // namespace rtl
