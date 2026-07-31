// query.h
#pragma once
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
// namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "no_params.hpp"
#include "no_results.hpp"
#include "parameter_root.hpp"
#include "result_root.hpp"
// #include "buffer_dscr.hpp"
#include "db2_types.hpp"
#include "db2_database.hpp"
#include "odbc_error.hpp"
// #include "log.hpp"
//  #include <span>
#include <string>
#include <expected>
#include <memory>
#include <type_traits>
#include <format>
// #include <algorithm>
#include <string_view>


namespace rtl
{

  // ====================================================================
  // query — C++23, dscr, deducing this, snake_case
  // ====================================================================
  template <typename params = no_params, typename results = no_results>
  class query
  {
    static_assert(std::is_base_of_v<parameter_root, params>, "Template parameter 'params' must inherit from parameter_root.");
    static_assert(std::is_base_of_v<result_root, results>, "Template parameter 'results' must inherit from result_root.");
    static constexpr bool has_params  = params::has_parameters();
    static constexpr bool has_results = results::has_results();
    const database*       db_         = nullptr;
    SQLHSTMT              stmt_       = SQL_NULL_HSTMT;
    std::string           sql_;

    [[no_unique_address]] std::conditional_t<has_params, std::shared_ptr<params>, std::monostate>   par_;
    [[no_unique_address]] std::conditional_t<has_results, std::shared_ptr<results>, std::monostate> res_;

    /// what layout_generation() said when prepare() bound the buffers; see check_layout()
    [[no_unique_address]] std::conditional_t<has_params, uint64_t, std::monostate>  bound_par_layout_ = {};
    [[no_unique_address]] std::conditional_t<has_results, uint64_t, std::monostate> bound_res_layout_ = {};

    [[no_unique_address]] std::conditional_t<has_params, SQLULEN, std::monostate>  params_processed_ = {};
    [[no_unique_address]] std::conditional_t<has_results, SQLULEN, std::monostate> rows_fetched_     = {};
    SQLLEN                                                                         affected_rows_    = 0;
  public:
    explicit query(const database* db, std::string_view sql);
    ~query();
    query(const query&)                               = delete;
    query& operator=(const query&)                    = delete;
    query(query&&) noexcept                           = default;
    query&                operator=(query&&) noexcept = default;
    [[nodiscard]] bool    is_prepared() const noexcept;                          //< has prepare() been called successfully?
    [[nodiscard]] int64_t affected_rows() const noexcept;                        //< number of row affected by the last execute;
                                                                                 //< -1 when unavailable
    [[nodiscard]] e_void                          prepare() noexcept;            //< prepare the statement and bind the buffers
    [[nodiscard]] e_void                          check_layout() const noexcept; //< refuse if buffers changed since last prepare
    [[nodiscard]] e_void                          execute() noexcept;            //< execute the statement with the current parameter values
    [[nodiscard]] std::expected<bool, odbc_error> fetch() noexcept;

    /// A ternary cannot do this: both of its arms must have the same type, and
    /// here one of them is std::monostate. The choice has to be made at compile
    /// time with if constexpr, or the branch that does not apply is still type
    /// checked and fails.
    [[nodiscard]] std::conditional_t<has_params, std::shared_ptr<params>, std::monostate> get_param() noexcept
    {
      if constexpr (has_params) return par_;
      else return std::monostate{};
    }


    /**
     * @brief the result buffer, writable
     *
     * Separate from get_result() because that one hands out a const pointer for
     * reading fetched rows, while sizing the buffer is a write and has to
     * happen before prepare().
     */
    [[nodiscard]] std::conditional_t<has_results, std::shared_ptr<results>, std::monostate> get_result_buffer() noexcept
    {
      if constexpr (has_results) return res_;
      else return std::monostate{};
    }
    [[nodiscard]] std::conditional_t<has_results, std::shared_ptr<const results>, std::monostate> get_result() const noexcept
    {
      if constexpr (has_results) return res_;
      else return std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_params, SQLULEN, std::monostate> params_processed() const noexcept
    {
      if constexpr (has_params) return params_processed_;
      else return std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_results, SQLULEN, std::monostate> rows_fetched() const noexcept
    {
      if constexpr (has_results) return rows_fetched_;
      else return std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_results, size_t, std::monostate> occupied_count() const noexcept
    {
      if constexpr (has_results) return res_->occupied();
      else return std::monostate{};
    }

    void reset(this query& self) noexcept
    {
      if (self.is_prepared())
      {
        // SQLFreeStmt(self.stmt_, SQL_UNBIND);
        // SQLFreeStmt(self.stmt_, SQL_RESET_PARAMS);
        SQLFreeHandle(SQL_HANDLE_STMT, self.stmt_);
        self.stmt_ = SQL_NULL_HSTMT;
        if constexpr (has_params) self.params_processed_ = 0;
        if constexpr (has_results) self.rows_fetched_ = 0;
      }
      if constexpr (has_params) self.par_->reset_all_null();
      if constexpr (has_results) self.res_->set_occupied(0);
    }

    [[nodiscard]] SQLHSTMT stmt() const noexcept { return stmt_; }
  };

  template <typename params, typename results>
  inline query<params, results>::query(const database* db, std::string_view sql)
  : db_(db)
  , sql_(sql)
  {
    if (db_ == nullptr) throw std::invalid_argument("db pointer cannot be null");
    if constexpr (has_params) par_ = std::make_shared<params>();
    if constexpr (has_results) res_ = std::make_shared<results>();
  }

  template <typename params, typename results>
  inline query<params, results>::~query()
  {
    if (stmt_ != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
  }

  template <typename params, typename results>
  inline bool query<params, results>::is_prepared() const noexcept
  { return stmt_ != SQL_NULL_HSTMT; }

  /// rows the last execute inserted, updated or deleted; -1 when unavailable
  template <typename params, typename results>
  inline int64_t query<params, results>::affected_rows() const noexcept
  { return static_cast<int64_t>(affected_rows_); }

  template <typename params, typename results>
  inline e_void query<params, results>::prepare() noexcept
  {
    auto*   logger = db_->get_logger();
    SQLHDBC conn   = db_->get_conn();

    if (is_prepared()) SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
    SQLHSTMT  new_stmt = SQL_NULL_HSTMT;
    SQLRETURN ret      = SQLAllocHandle(SQL_HANDLE_STMT, conn, &new_stmt);
    if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, conn, handle_type_enum::conn));
    stmt_ = new_stmt;

    logger->debug("Preparing SQL: {}", sql_);
    ret = SQLPrepare(stmt_, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql_.c_str())), SQL_NTS); // NOLINT
    if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt));

    if constexpr (has_params)
    {
      constexpr auto param_const = params::buffer_description_const();
      auto           param_init  = par_->buffer_description_init();
      for (size_t i = 0; i < param_const.size(); ++i)
      {
        const auto& c = param_const[i];
        const auto& r = param_init[i];
        ret           = SQLBindParameter(stmt_,
                                         static_cast<SQLUSMALLINT>(i) + 1,
                                         SQL_PARAM_INPUT,
                                         db2::to_odbc_c(c.type),
                                         db2::to_odbc(c.type),
                                         c.column_size,
                                         c.decimal_digits,
                                         r.value_ptr,
                                         /// the driver needs the room it may write into. Ignored for the
                                         /// fixed size C types, but a character or binary column bound with
                               /// 0 here comes back empty - stride is exactly the array's byte size
                               static_cast<SQLLEN>(r.stride),
                               reinterpret_cast<SQLLEN*>(r.indicator_ptr)); // NOLINT - width checked in db2_types.hpp
        if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt, static_cast<SQLSMALLINT>(i + 1)));
      }
      SQLSetStmtAttr(stmt_, SQL_ATTR_PARAMSET_SIZE, reinterpret_cast<SQLPOINTER>(par_->buffer_size()), 0);
      SQLSetStmtAttr(stmt_, SQL_ATTR_PARAMS_PROCESSED_PTR, &params_processed_, 0);
      if (par_->is_batch())
      {
        auto status_span = par_->row_status();
        SQLSetStmtAttr(stmt_, SQL_ATTR_PARAM_STATUS_PTR, status_span.data(), 0);
        /// Without this the per row status array is useless. DB2 defaults to
        /// SQL_ATOMIC_YES, which treats the whole array as one unit: a single
        /// bad row fails the entire execute and the driver fills the status
        /// array with SQL_PARAM_DIAG_UNAVAILABLE rather than saying which row
        /// it was. SQL_ATOMIC_NO makes it process the rows one by one, so the
        /// good ones land and the status array names the ones that did not.
        SQLSetStmtAttr(stmt_,
                       SQL_ATTR_PARAMOPT_ATOMIC,
                       reinterpret_cast<SQLPOINTER>(SQL_ATOMIC_NO), // NOLINT
                       0);                                          // NOLINT
      }
    }

    if constexpr (has_results)
    {
      constexpr auto result_const = results::buffer_description_const();
      auto           result_init  = res_->buffer_description_init();
      for (size_t i = 0; i < result_const.size(); ++i)
      {
        const auto& c = result_const[i];
        const auto& r = result_init[i];
        ret           = SQLBindCol(stmt_,
                                   static_cast<SQLUSMALLINT>(i) + 1,
                                   db2::to_odbc_c(c.type),
                                   r.value_ptr,
                                   static_cast<SQLLEN>(r.stride), // see the note on SQLBindParameter above
                                   reinterpret_cast<SQLLEN*>(r.indicator_ptr));
        if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt, static_cast<SQLSMALLINT>(i + 1)));
      }
      SQLSetStmtAttr(stmt_, SQL_ATTR_ROW_ARRAY_SIZE, reinterpret_cast<SQLPOINTER>(res_->buffer_size()), 0);
      SQLSetStmtAttr(stmt_, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched_, 0);
    }

    /// The driver now holds raw pointers into the buffers. Remember how they
    /// looked, so that a resize afterwards is caught rather than followed.
    if constexpr (has_params) bound_par_layout_ = par_->layout_generation();
    if constexpr (has_results) bound_res_layout_ = res_->layout_generation();

    logger->info("Query prepared: params={}, results={}", has_params ? "yes" : "no", has_results ? "yes" : "no");
    return {};
  }

  /**
   * @brief refuse to run against buffers that have moved since prepare()
   *
   * set_buffer_size() reallocates, so every pointer the driver was given in
   * prepare() dangles. Letting the driver read them is a use after free that
   * ASan catches and a release build does not; this turns it into an error
   * the caller can act on.
   */
  template <typename params, typename results>
  inline e_void query<params, results>::check_layout() const noexcept
  {
    if constexpr (has_params)
      if (par_->layout_generation() != bound_par_layout_)
        return std::unexpected(odbc_error::client("parameter buffer was resized after prepare(); "
                                                  "call set_buffer_size() before prepare(), then prepare() again"));
    if constexpr (has_results)
      if (res_->layout_generation() != bound_res_layout_)
        return std::unexpected(odbc_error::client("result buffer was resized after prepare(); "
                                                  "call set_buffer_size() before prepare(), then prepare() again"));
    return {};
  }

  template <typename params, typename results>
  inline e_void query<params, results>::execute() noexcept
  {
    if (! is_prepared()) return std::unexpected(odbc_error(SQL_ERROR, SQL_NULL_HANDLE, handle_type_enum::stmt));
    if (auto layout = check_layout(); ! layout) return std::unexpected(layout.error());

    auto* logger = db_->get_logger();
    if constexpr (has_params) par_->clear_row_status();

    SQLRETURN ret = SQLExecute(stmt_);
    /// SQL_NO_DATA is not a failure: an update or delete that matched no row
    /// did exactly what it was asked to. Callers that care ask affected_rows().
    if (! SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt));

    affected_rows_ = 0;
    if (SQLRowCount(stmt_, &affected_rows_) != SQL_SUCCESS) affected_rows_ = -1;

    std::string info;
    if constexpr (has_params)
    {
      info = std::format(" {} params processed.", params_processed_);
      if (par_->is_batch())
      {
        auto   status = par_->row_status();
        size_t errors = 0;
        for (auto s : status)
          if (s == SQL_PARAM_ERROR) ++errors;
        if (errors > 0)
        {
          info += std::format(" {} errors.", errors);
          logger->warn("Batch execute completed with {} errors.", errors);
        }
      }
    }
    logger->info("SQLExecute succeeded.{}", info);
    return {};
  }

  template <typename params, typename results>
  inline std::expected<bool, odbc_error> query<params, results>::fetch() noexcept
  {
    if (! is_prepared()) return std::unexpected(odbc_error(SQL_ERROR, SQL_NULL_HANDLE, handle_type_enum::stmt));
    if (auto layout = check_layout(); ! layout) return std::unexpected(layout.error());

    /// the whole body has to live in the else branch - without it the code
    /// below is still compiled for a result-less query, where rows_fetched_
    /// and res_ are std::monostate
    if constexpr (! has_results) return false; // NOLINT(readability-inconsistent-ifelse-braces)
    else
    {
      rows_fetched_ = 0;
      SQLRETURN ret = SQLFetchScroll(stmt_, SQL_FETCH_NEXT, 0);
      if (ret == SQL_NO_DATA)
      {
        res_->set_occupied(0);
        return false;
      }
      if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt));

      const auto   reported    = static_cast<size_t>(rows_fetched_);
      const size_t max_allowed = res_->buffer_size();

      if (reported > max_allowed)
      {
        const std::string error_msg = std::format("FATAL ODBC DRIVER ERROR: SQLFetchScroll reported {} rows, "
                                                  "but the buffer holds only {}. Buffer overflow detected. "
                                                  "Data integrity compromised. Terminating application.",
                                                  reported,
                                                  max_allowed);
        db_->get_logger()->critical("{}", error_msg);
        return std::unexpected(odbc_error(SQL_ERROR, SQL_NULL_HANDLE, handle_type_enum::stmt));
      }

      res_->set_occupied(reported);
      return reported > 0;
    }
  }

} // namespace rtl
