// query.h
#pragma once
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "no_params.hpp"
#include "no_results.hpp"
#include "parameter_root.hpp"
#include "result_root.hpp"
#include <sql.h>
#include <sqlext.h>
// #include <span>
#include <string>
#include <expected>
#include <memory>
#include <spdlog/spdlog.h>
#include <type_traits>
#include <format>
// #include <algorithm>
#include <string_view>


namespace rtl
{
  struct database // NOLINT
  {
    virtual ~database()                                                               = default;
    [[nodiscard]] virtual SQLHDBC                         get_conn() const noexcept   = 0;
    [[nodiscard]] virtual std::shared_ptr<spdlog::logger> get_logger() const noexcept = 0;
  } __attribute__((aligned(128))); // NOLINT
  // NOLINTNEXTLINE(performance-enum-size)
  enum class handle_type_enum : int16_t
  {
    env  = SQL_HANDLE_ENV,
    conn = SQL_HANDLE_DBC,
    stmt = SQL_HANDLE_STMT
  };
  // ====================================================================
  // odbc_error — Unicode-safe (UTF-8), SI-ready
  // ====================================================================
  struct odbc_error
  {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    SQLRETURN   ret_;
    std::string message_;   // UTF-8
    std::string sql_state_; // ASCII
    SQLINTEGER  native_error_ = 0;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    odbc_error(SQLRETURN r, SQLHANDLE h, handle_type_enum t, SQLSMALLINT rec = 1) noexcept;
  private:
    static std::string sqlchar_to_utf8(const SQLCHAR* src, size_t len) noexcept;
  } __attribute__((aligned(128))); // NOLINT

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

    const database* db_   = nullptr;
    SQLHSTMT        stmt_ = SQL_NULL_HSTMT;
    std::u8string   sql_;

    [[no_unique_address]] std::conditional_t<has_params, std::shared_ptr<params>, std::monostate>   par_;
    [[no_unique_address]] std::conditional_t<has_results, std::shared_ptr<results>, std::monostate> res_;

    [[no_unique_address]] std::conditional_t<has_params, SQLULEN, std::monostate>  params_processed_ = {};
    [[no_unique_address]] std::conditional_t<has_results, SQLULEN, std::monostate> rows_fetched_     = {};
  public:
    explicit query(const database* db, std::u8string_view sql)
    : db_(db)
    , sql_(sql.data(), sql.size())
    {
      if (db_ == nullptr) throw std::invalid_argument("db pointer cannot be null");
      if constexpr (has_params) par_ = std::make_shared<params>();
      if constexpr (has_results) res_ = std::make_shared<results>();
    }

    ~query()
    {
      if (stmt_ != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
    }

    query(const query&)                = delete;
    query& operator=(const query&)     = delete;
    query(query&&) noexcept            = default;
    query& operator=(query&&) noexcept = default;

    [[nodiscard]] bool is_prepared() const noexcept { return stmt_ != SQL_NULL_HSTMT; }

    [[nodiscard]] std::expected<void, odbc_error> prepare() noexcept
    {
      auto    logger = db_->get_logger();
      SQLHDBC conn   = db_->get_conn();

      if (is_prepared()) SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
      SQLHSTMT  new_stmt = SQL_NULL_HSTMT;
      SQLRETURN ret      = SQLAllocHandle(SQL_HANDLE_STMT, conn, &new_stmt);
      if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, conn, handle_type_enum::conn));
      stmt_ = new_stmt;

      logger->debug("Preparing SQL: {}", sql_);
      ret = SQLPrepare(stmt_, reinterpret_cast<SQLCHAR*>(const_cast<char8_t*>(sql_.c_str())), SQL_NTS); // NOLINT
      if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt));

      if constexpr (has_params)
      {
        constexpr auto param_const = params::buffer_description_const();
        auto           param_init  = par_->buffer_description_init();
        for (int16_t i = 0; i < param_const.size(); ++i)
        {
          const auto& c = param_const[i];
          const auto& r = param_init[i];
          ret           = SQLBindParameter(stmt_,
                                 static_cast<SQLUSMALLINT>(i) + 1,
                                 SQL_PARAM_INPUT,
                                 c.value_type,
                                 c.param_type,
                                 c.column_size,
                                 c.decimal_digits,
                                 r.value_ptr,
                                 0,
                                 r.indicator_ptr);
          if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt, i + 1)); // NOLINT
        }
        SQLSetStmtAttr(stmt_,
                       SQL_ATTR_PARAMSET_SIZE,
                       reinterpret_cast<SQLPOINTER>(params::batch_size), // NOLINT
                       0);                                               // NOLINT
        SQLSetStmtAttr(stmt_, SQL_ATTR_PARAMS_PROCESSED_PTR, &params_processed_, 0);
        if constexpr (params::is_batch())
        {
          auto status_span = par_->get_row_status();
          SQLSetStmtAttr(stmt_, SQL_ATTR_PARAM_STATUS_PTR, status_span.data(), 0);
        }
      }

      if constexpr (has_results)
      {
        constexpr auto result_const = results::buffer_description_const();
        auto           result_init  = res_->buffer_description_init();
        for (int16_t i = 0; i < result_const.size(); ++i)
        {
          const auto& c = result_const[i];
          const auto& r = result_init[i];
          ret           = SQLBindCol(stmt_, static_cast<SQLUSMALLINT>(i) + 1, c.value_type, r.value_ptr, 0, r.indicator_ptr);
          if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt, i + 1)); // NOLINT
        }
        SQLSetStmtAttr(stmt_,
                       SQL_ATTR_ROW_ARRAY_SIZE,
                       reinterpret_cast<SQLPOINTER>(results::batch_size), // NOLINT
                       0);                                                // NOLINT
        SQLSetStmtAttr(stmt_, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched_, 0);
      }

      logger->info("Query prepared: params={}, results={}", has_params ? "yes" : "no", has_results ? "yes" : "no");
      return {};
    }

    [[nodiscard]] std::expected<void, odbc_error> execute() noexcept
    {
      if (! is_prepared()) return std::unexpected(odbc_error(SQL_ERROR, SQL_NULL_HANDLE, handle_type_enum::stmt));

      auto logger = db_->get_logger();
      if constexpr (has_params) par_->clear_row_status();

      SQLRETURN ret = SQLExecute(stmt_);
      if (! SQL_SUCCEEDED(ret) && ret != SQL_SUCCESS_WITH_INFO) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt));

      std::string info;
      if constexpr (has_params)
      {
        info = std::format(" {} params processed.", params_processed_);
        if constexpr (params::is_batch())
        {
          auto   status = par_->get_row_status();
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

    [[nodiscard]] std::expected<bool, odbc_error> fetch() noexcept
    {
      if (! is_prepared()) return std::unexpected(odbc_error(SQL_ERROR, SQL_NULL_HANDLE, handle_type_enum::stmt));
      if constexpr (! has_results) return false;

      rows_fetched_ = 0;
      SQLRETURN ret = SQLFetchScroll(stmt_, SQL_FETCH_NEXT, 0);
      if (ret == SQL_NO_DATA)
      {
        if constexpr (has_results) res_->set_occupied(0);
        return false;
      }
      if (! SQL_SUCCEEDED(ret)) return std::unexpected(odbc_error(ret, stmt_, handle_type_enum::stmt));

      const auto   reported    = static_cast<size_t>(rows_fetched_);
      const size_t max_allowed = results::batch_size;

      if (reported > max_allowed)
      {
        const std::string error_msg = std::format("FATAL ODBC DRIVER ERROR: SQLFetchScroll reported {} rows, "
                                                  "but batch_size is only {}. Buffer overflow detected. "
                                                  "Data integrity compromised. Terminating application.",
                                                  reported,
                                                  max_allowed);
        db_->get_logger()->critical("{}", error_msg);
        return std::unexpected(odbc_error(SQL_ERROR, SQL_NULL_HANDLE, handle_type_enum::stmt));
      }

      if constexpr (has_results) res_->set_occupied(reported);
      return reported > 0;
    }

    [[nodiscard]] std::conditional_t<has_params, std::shared_ptr<params>, std::monostate> get_param() noexcept
    {
      return has_params ? par_ : std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_results, std::shared_ptr<const results>, std::monostate> get_result() const noexcept

    {
      return has_results ? res_ : std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_params, SQLULEN, std::monostate> params_processed() const noexcept

    {
      return has_params ? params_processed_ : std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_results, SQLULEN, std::monostate> rows_fetched() const noexcept
    {
      return has_results ? rows_fetched_ : std::monostate{};
    }

    [[nodiscard]] std::conditional_t<has_results, size_t, std::monostate> occupied_count() const noexcept
    {
      return has_results ? res_->occupied() : std::monostate{};
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

} // namespace rtl
