// query.hpp - psql runtime
#pragma once
/**
 * @file
 * @brief the query runtime used by generated code, on top of libpq
 *
 * Deliberately not a port of the db2 runtime. ODBC lets a driver write
 * straight into the generated arrays (SQLBindCol plus SQL_ATTR_ROW_ARRAY_SIZE);
 * libpq has no such thing - PQexecPrepared hands back a PGresult and the
 * values are read out of it one at a time. So this runtime owns the copying.
 *
 * Values travel in text format in both directions. Binary format would save
 * the parsing, but PostgreSQL's binary encoding is big endian and counts time
 * in microseconds since 2000-01-01, so it needs byte swapping and epoch
 * arithmetic on every column - worth doing later, as an optimisation, once
 * there are tests to catch a mistake in it.
 */

#include "buffer_dscr.hpp"
#include "no_params.hpp"
#include "no_results.hpp"
#include "parameter_root.hpp"
#include "psql_database.hpp" // IWYU pragma: export
#include "psql_types.hpp"
#include "result_root.hpp"
#include "logger.hpp"
#include <libpq-fe.h>
#include <charconv>
#include <cstring>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include <fmt/format.h>

namespace rtl
{
  /**
   * @brief an error reported by the server or by libpq
   */
  struct psql_error
  {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    std::string message;   ///< human readable, already UTF-8
    std::string sql_state; ///< five character SQLSTATE, empty for client side errors
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    static psql_error from_result(const PGresult* res, PGconn* conn)
    {
      const char* state = (res != nullptr) ? PQresultErrorField(res, PG_DIAG_SQLSTATE) : nullptr;
      const char* msg   = (res != nullptr) ? PQresultErrorMessage(res) : PQerrorMessage(conn);
      return psql_error{.message = (msg != nullptr) ? msg : "unknown error", .sql_state = (state != nullptr) ? state : ""};
    }
  };

  namespace detail
  {
    /// RAII for PGresult - libpq results must be cleared on every path
    class result_holder
    {
    public:
      explicit result_holder(PGresult* r) noexcept
      : res_(r)
      {
      }
      ~result_holder() { PQclear(res_); }
      result_holder(const result_holder&)            = delete;
      result_holder& operator=(const result_holder&) = delete;
      result_holder(result_holder&& o) noexcept
      : res_(o.res_)
      {
        o.res_ = nullptr;
      }
      result_holder& operator=(result_holder&& o) noexcept
      {
        if (this != &o)
        {
          PQclear(res_);
          res_   = o.res_;
          o.res_ = nullptr;
        }
        return *this;
      }
      [[nodiscard]] PGresult* get() const noexcept { return res_; }
      [[nodiscard]] explicit  operator bool() const noexcept { return res_ != nullptr; }
    private:
      PGresult* res_;
    };

    /// address of row `row` of a buffer column
    [[nodiscard]] inline void* row_ptr(const buffer_dscr_init& init, size_t row) noexcept
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      return static_cast<std::byte*>(init.value_ptr) + (init.stride * row);
    }

    /// parse a decimal integer, leaving the target untouched on failure
    template <typename T>
    bool parse_int(std::string_view sv, T& out) noexcept
    {
      const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), out); // NOLINT
      return res.ec == std::errc{};
    }

    /// parse a floating point value
    template <typename T>
    bool parse_float(std::string_view sv, T& out) noexcept
    {
      const auto res = std::from_chars(sv.data(), sv.data() + sv.size(), out); // NOLINT
      return res.ec == std::errc{};
    }

    bool parse_date(std::string_view sv, rtl::date& out) noexcept;
    bool parse_time(std::string_view sv, rtl::time& out) noexcept;
    bool parse_timestamp(std::string_view sv, rtl::timestamp& out) noexcept;
    bool parse_guid(std::string_view sv, rtl::guid& out) noexcept;

    std::string format_date(const rtl::date& v);
    std::string format_time(const rtl::time& v);
    std::string format_timestamp(const rtl::timestamp& v);
    std::string format_guid(const rtl::guid& v);

    /// PostgreSQL renders bytea as "\x<hex>" in text format
    size_t      decode_bytea(std::string_view sv, std::byte* dst, size_t capacity) noexcept;
    std::string encode_bytea(const std::byte* src, size_t len);

    /// copy one text value into the storage slot of a buffer column
    bool store_value(const buffer_dscr_const& dscr, const buffer_dscr_init& init, size_t row, std::string_view text) noexcept;
    /// render the storage slot of a buffer column as text for the server
    std::string load_value(const buffer_dscr_const& dscr, const buffer_dscr_init& init, size_t row);
  } // namespace detail

  /**
   * @brief a prepared statement together with its parameter and result buffers
   *
   * Mirrors the interface of the db2 runtime - prepare, execute, fetch,
   * get_param, get_result - so that generated code reads the same either way.
   */
  template <typename params = no_params, typename results = no_results>
  class query
  {
    static_assert(std::is_base_of_v<parameter_root, params>, "Template parameter 'params' must inherit from parameter_root.");
    static_assert(std::is_base_of_v<result_root, results>, "Template parameter 'results' must inherit from result_root.");

    static constexpr bool has_params  = params::has_parameters();
    static constexpr bool has_results = results::has_results();

    const database* db_ = nullptr;
    std::string     sql_;
    std::string     stmt_name_;
    bool            prepared_ = false;

    /// the rows of the last execute, still owned by libpq until the next one
    /// what layout_generation() said when prepare() bound the buffers; see check_layout()
    uint64_t bound_par_layout_ = 0;
    uint64_t bound_res_layout_ = 0;

    detail::result_holder rows_{nullptr};
    size_t                next_row_      = 0; ///< how far fetch() has walked through rows_
    size_t                total_rows_    = 0;
    size_t                rows_fetched_  = 0;
    int64_t               affected_rows_ = -1; ///< rows the last execute inserted, updated or deleted; -1 when unavailable

    [[no_unique_address]] std::conditional_t<has_params, std::shared_ptr<params>, std::monostate>   par_;
    [[no_unique_address]] std::conditional_t<has_results, std::shared_ptr<results>, std::monostate> res_;
  public:
    explicit query(const database* db, std::string_view sql)
    : db_(db)
    , sql_(sql)
    {
      if (db_ == nullptr) throw std::invalid_argument("db pointer cannot be null");
      /// a name unique to this object - two queries may live at once
      stmt_name_ = fmt::format("dbgen4_{}", static_cast<const void*>(this));
      if constexpr (has_params) par_ = std::make_shared<params>();
      if constexpr (has_results) res_ = std::make_shared<results>();
    }

    ~query()
    {
      if (prepared_ && db_ != nullptr)
      {
        const detail::result_holder r{PQexec(db_->get_conn(), fmt::format("DEALLOCATE {}", stmt_name_).c_str())};
      }
    }

    query(const query&)            = delete;
    query& operator=(const query&) = delete;
    query(query&&) noexcept        = default;
    query& operator=(query&&)      = delete;

    [[nodiscard]] bool    is_prepared() const noexcept { return prepared_; }
    [[nodiscard]] int64_t affected_rows() const noexcept { return affected_rows_; }

    /**
     * @brief parse and plan the statement on the server
     */
    [[nodiscard]] std::expected<void, psql_error> prepare() noexcept
    try
    {
      auto*   logger = db_->get_logger();
      PGconn* conn   = db_->get_conn();

      /// declare the parameter types we intend to send, so that the server
      /// does not have to infer them from the values
      std::vector<Oid> oids;
      if constexpr (has_params)
      {
        constexpr auto pd = params::buffer_description_const();
        oids.reserve(pd.size());
        for (const auto& c : pd) oids.push_back(static_cast<Oid>(psql::to_oid(c.type)));
      }

      const detail::result_holder res{
        PQprepare(conn, stmt_name_.c_str(), sql_.c_str(), static_cast<int>(oids.size()), oids.empty() ? nullptr : oids.data())};
      if (PQresultStatus(res.get()) != PGRES_COMMAND_OK)
        return std::unexpected(psql_error::from_result(res.get(), conn));

      prepared_ = true;
      /// Remember how the buffers looked, so that a resize afterwards is caught
      /// rather than followed - see check_layout().
      if constexpr (has_params) bound_par_layout_ = par_->layout_generation();
      if constexpr (has_results) bound_res_layout_ = res_->layout_generation();
      logger->info("Query prepared: params={}, results={}", has_params ? "yes" : "no", has_results ? "yes" : "no");
      return {};
    }
    catch (const std::exception& e)
    {
      return std::unexpected(psql_error{.message = e.what(), .sql_state = ""});
    }

    /**
     * @brief send the current parameter buffer and run the statement
     */
    /**
     * @brief refuse to run against buffers that have moved since prepare()
     *
     * set_buffer_size() reallocates every column array. This backend walks
     * those arrays itself rather than handing them to a driver, but the
     * pointers in buffer_description_init() go stale all the same.
     */
    [[nodiscard]] std::expected<void, psql_error> check_layout() const noexcept
    {
      if constexpr (has_params)
        if (par_->layout_generation() != bound_par_layout_)
          return std::unexpected(psql_error{.message = "parameter buffer was resized after prepare(); "
                                                       "call set_buffer_size() before prepare(), then prepare() again",
                                            .sql_state = ""});
      if constexpr (has_results)
        if (res_->layout_generation() != bound_res_layout_)
          return std::unexpected(psql_error{.message = "result buffer was resized after prepare(); "
                                                       "call set_buffer_size() before prepare(), then prepare() again",
                                            .sql_state = ""});
      return {};
    }

    [[nodiscard]] std::expected<void, psql_error> execute() noexcept
    try
    {
      if (! prepared_) return std::unexpected(psql_error{.message = "statement is not prepared", .sql_state = ""});
      if (auto layout = check_layout(); ! layout) return std::unexpected(layout.error());

      PGconn* conn = db_->get_conn();

      /// marshal row 0 of the parameter buffer into text
      /// (batches send one row at a time - see execute_row below)
      std::vector<std::string> holders;
      std::vector<const char*> values;
      if constexpr (has_params)
      {
        constexpr auto pd = params::buffer_description_const();
        auto           pi = par_->buffer_description_init();
        holders.reserve(pd.size());
        values.reserve(pd.size());
        for (size_t i = 0; i < pd.size(); ++i)
        {
          const bool is_null = pi[i].indicator_ptr[0] == null_data;
          holders.push_back(is_null ? std::string{} : detail::load_value(pd[i], pi[i], 0));
          values.push_back(is_null ? nullptr : holders.back().c_str());
        }
        /// holders may have reallocated while being filled - repoint
        for (size_t i = 0; i < pd.size(); ++i)
          if (values[i] != nullptr) values[i] = holders[i].c_str(); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      }

      detail::result_holder res{PQexecPrepared(conn,
                                               stmt_name_.c_str(),
                                               static_cast<int>(values.size()),
                                               values.empty() ? nullptr : values.data(),
                                               nullptr, // all text, so no lengths needed
                                               nullptr, // all text, so no formats needed
                                               0)};     // ask for text results

      const auto status = PQresultStatus(res.get());
      if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
        return std::unexpected(psql_error::from_result(res.get(), conn));

      total_rows_ = (status == PGRES_TUPLES_OK) ? static_cast<size_t>(PQntuples(res.get())) : 0;
      next_row_   = 0;

      /// PQcmdTuples() reports rows for INSERT/UPDATE/DELETE as a decimal string,
      /// empty for statements it does not apply to (e.g. SELECT, where total_rows_
      /// already says how many rows came back)
      const char* tuples = PQcmdTuples(res.get());
      affected_rows_     = -1;
      if (tuples != nullptr && *tuples != '\0') detail::parse_int(tuples, affected_rows_);

      rows_ = std::move(res);

      db_->get_logger()->info("Statement executed, {} rows available.", total_rows_);
      return {};
    }
    catch (const std::exception& e)
    {
      return std::unexpected(psql_error{.message = e.what(), .sql_state = ""});
    }

    /**
     * @brief copy the next batch of rows into the result buffer
     *
     * @return true when at least one row was copied, false at end of data
     */
    [[nodiscard]] std::expected<bool, psql_error> fetch() noexcept
    try
    {
      if constexpr (! has_results) return false; // NOLINT(readability-inconsistent-ifelse-braces)
      else
      {
        if (auto layout = check_layout(); ! layout) return std::unexpected(layout.error());
        if (! rows_) return std::unexpected(psql_error{.message = "no result set - execute first", .sql_state = ""});

        constexpr auto rd = results::buffer_description_const();
        auto           ri = res_->buffer_description_init();

        const size_t remaining = total_rows_ - next_row_;
        const size_t count     = std::min(remaining, res_->buffer_size());

        for (size_t r = 0; r < count; ++r)
        {
          const int src_row = static_cast<int>(next_row_ + r);
          for (size_t c = 0; c < rd.size(); ++c)
          {
            if (PQgetisnull(rows_.get(), src_row, static_cast<int>(c)) != 0)
            {
              ri[c].indicator_ptr[r] = null_data; // NOLINT
              continue;
            }
            const char* raw = PQgetvalue(rows_.get(), src_row, static_cast<int>(c));
            const auto  len = static_cast<size_t>(PQgetlength(rows_.get(), src_row, static_cast<int>(c)));
            if (! detail::store_value(rd[c], ri[c], r, std::string_view(raw, len)))
            {
              db_->get_logger()->warn("Column '{}' value could not be converted - stored as null.", rd[c].name);
              ri[c].indicator_ptr[r] = null_data; // NOLINT
            }
          }
        }

        next_row_ += count;
        rows_fetched_ = count;
        res_->set_occupied(count);
        return count > 0;
      }
    }
    catch (const std::exception& e)
    {
      return std::unexpected(psql_error{.message = e.what(), .sql_state = ""});
    }

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

    [[nodiscard]] size_t rows_fetched() const noexcept { return rows_fetched_; }
    [[nodiscard]] size_t total_rows() const noexcept { return total_rows_; }

    [[nodiscard]] std::conditional_t<has_results, size_t, std::monostate> occupied_count() const noexcept
    {
      if constexpr (has_results) return res_->occupied();
      else return std::monostate{};
    }

    /// forget the current result set and rewind, keeping the statement prepared
    void reset() noexcept
    {
      rows_          = detail::result_holder{nullptr};
      next_row_      = 0;
      total_rows_    = 0;
      rows_fetched_  = 0;
      affected_rows_ = -1;
      if constexpr (has_params) par_->reset_all_null();
      if constexpr (has_results) res_->set_occupied(0);
    }
  };

} // namespace rtl
