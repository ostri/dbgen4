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
#include "db_error.hpp"
#include "no_params.hpp"
#include "no_results.hpp"
#include "parameter_root.hpp"
#include "psql_database.hpp" // IWYU pragma: export
#include "psql_types.hpp"
#include "result_root.hpp"
#include "logger.hpp"
#include <libpq-fe.h>
#include <atomic>
#include <charconv>
#include <cstring>
#include <expected>
#include <memory>
#include <optional>
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
    /// what PQresultStatus() said - PGRES_FATAL_ERROR and friends. Kept
    /// because it is the one thing that distinguishes a row which failed from
    /// one that never ran (PGRES_PIPELINE_ABORTED), which SQLSTATE alone does
    /// not say. Defaults to PGRES_FATAL_ERROR for the client side errors
    /// built by hand below, since those never had a PGresult to ask.
    ExecStatusType status = PGRES_FATAL_ERROR;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    static psql_error from_result(const PGresult* res, PGconn* conn)
    {
      const char* state = (res != nullptr) ? PQresultErrorField(res, PG_DIAG_SQLSTATE) : nullptr;
      const char* msg   = (res != nullptr) ? PQresultErrorMessage(res) : PQerrorMessage(conn);
      return psql_error{.message   = (msg != nullptr) ? msg : "unknown error",
                        .sql_state = (state != nullptr) ? state : "",
                        .status    = (res != nullptr) ? PQresultStatus(res) : PGRES_FATAL_ERROR};
    }
  };

  /**
   * @brief the psql half of the backend neutral error - see db_error.hpp
   *
   * `driver_status` keeps the ExecStatusType untranslated on purpose; the
   * classification callers act on comes from the SQLSTATE instead.
   * PostgreSQL has no numeric error code beyond that state, so `native_error`
   * stays 0.
   */
  [[nodiscard]] inline db_error to_db_error(const psql_error& e)
  {
    return db_error{.sts           = sqlstate_to_db_sts(e.sql_state),
                    .message       = e.message,
                    .sql_state     = e.sql_state,
                    .driver_status = static_cast<int16_t>(e.status),
                    .native_error  = 0};
  }

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
      { o.res_ = nullptr; }
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

    /**
     * @brief a fresh number for every prepared statement name
     *
     * Deliberately not a static member of query<> - that would give every
     * template instantiation a counter of its own, and two queries with
     * different parameter types would then both start at zero and collide on
     * the server with 42P05. One function, one counter, all instantiations.
     *
     * Atomic because the async facade prepares queries on a worker thread
     * while the main thread may still be constructing others.
     */
    [[nodiscard]] inline uint64_t next_stmt_id() noexcept
    {
      static std::atomic<uint64_t> counter{0};
      return counter.fetch_add(1, std::memory_order_relaxed);
    }

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

    static constexpr bool has_p = params::has_parameters();
    static constexpr bool has_r = results::has_results();

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

    [[no_unique_address]] std::conditional_t<has_p, std::shared_ptr<params>, std::monostate>  par_;
    [[no_unique_address]] std::conditional_t<has_r, std::shared_ptr<results>, std::monostate> res_;
  public:
    explicit query(const database* db, std::string_view sql)
    : db_(db)
    , sql_(sql)
    {
      if (db_ == nullptr) throw std::invalid_argument("db pointer cannot be null");
      /// A name unique to this object - two queries may live at once. Drawn
      /// from a counter rather than from `this`: an address is only unique
      /// among objects alive at the same time, so a query destroyed and
      /// another built in its place would share a name, and moving an object
      /// would leave the name pointing at where it used to be.
      stmt_name_ = fmt::format("dbgen4_{}", detail::next_stmt_id());
      if constexpr (has_p) par_ = std::make_shared<params>();
      if constexpr (has_r) res_ = std::make_shared<results>();
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
    /**
     * @brief move, leaving the source owning nothing
     *
     * Not `= default`: the prepared statement on the server is named by
     * stmt_name_ and freed by whoever still has prepared_ set. A defaulted
     * move copies both into the target and leaves prepared_ true in the
     * source, so both destructors DEALLOCATE the same name - the second one
     * fails with 26000, which on PostgreSQL puts the whole transaction into
     * 25P02 and takes every later statement down with it.
     */
    query(query&& o) noexcept
    : db_(o.db_)
    , sql_(std::move(o.sql_))
    , stmt_name_(std::move(o.stmt_name_))
    , prepared_(o.prepared_)
    , bound_par_layout_(o.bound_par_layout_)
    , bound_res_layout_(o.bound_res_layout_)
    , rows_(std::move(o.rows_))
    , next_row_(o.next_row_)
    , total_rows_(o.total_rows_)
    , rows_fetched_(o.rows_fetched_)
    , affected_rows_(o.affected_rows_)
    , par_(std::move(o.par_))
    , res_(std::move(o.res_))
    {
      o.prepared_ = false; ///< the statement is ours now - the source must not free it
      o.db_       = nullptr;
    }
    query& operator=(query&&) = delete;

    [[nodiscard]] bool    is_prepared() const noexcept { return prepared_; }
    [[nodiscard]] int64_t affected_rows() const noexcept { return affected_rows_; }
  private:
    /// marshal one row of the parameter buffer into text, ready for
    /// PQexecPrepared/PQsendQueryPrepared. holders owns the storage;
    /// values points into it (nullptr for a null column) and is what the
    /// caller hands to libpq - repointed after holders stops growing, since
    /// push_back may have reallocated while filling it in.
    template <typename param_dscr, typename param_init>
    static void marshal_row(const param_dscr&         pd,
                            const param_init&         pi,
                            size_t                    row,
                            std::vector<std::string>& holders,
                            std::vector<const char*>& values)
    {
      holders.reserve(pd.size());
      values.reserve(pd.size());
      for (size_t i = 0; i < pd.size(); ++i)
      {
        const bool is_null = pi[i].indicator_ptr[row] == null_data;
        holders.push_back(is_null ? std::string{} : detail::load_value(pd[i], pi[i], row));
        values.push_back(is_null ? nullptr : holders.back().c_str());
      }
      for (size_t i = 0; i < pd.size(); ++i)
        if (values[i] != nullptr) values[i] = holders[i].c_str(); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }
  public:
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
      if constexpr (has_p)
      {
        constexpr auto pd = params::buffer_description_const();
        oids.reserve(pd.size());
        for (const auto& c : pd) oids.push_back(static_cast<Oid>(psql::to_oid(c.type)));
      }

      const detail::result_holder res{
        PQprepare(conn, stmt_name_.c_str(), sql_.c_str(), static_cast<int>(oids.size()), oids.empty() ? nullptr : oids.data())};
      if (PQresultStatus(res.get()) != PGRES_COMMAND_OK) return std::unexpected(psql_error::from_result(res.get(), conn));

      prepared_ = true;
      /// Remember how the buffers looked, so that a resize afterwards is caught
      /// rather than followed - see check_layout().
      if constexpr (has_p) bound_par_layout_ = par_->layout_generation();
      if constexpr (has_r) bound_res_layout_ = res_->layout_generation();
      logger->info("Query prepared: params={}, results={}", has_p ? "yes" : "no", has_r ? "yes" : "no");
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
      if constexpr (has_p)
        if (par_->layout_generation() != bound_par_layout_)
          return std::unexpected(psql_error{.message   = "parameter buffer was resized after prepare(); "
                                                         "call set_buffer_size() before prepare(), then prepare() again",
                                            .sql_state = ""});
      if constexpr (has_r)
        if (res_->layout_generation() != bound_res_layout_)
          return std::unexpected(psql_error{.message   = "result buffer was resized after prepare(); "
                                                         "call set_buffer_size() before prepare(), then prepare() again",
                                            .sql_state = ""});
      return {};
    }

    [[nodiscard]] std::expected<void, psql_error> execute() noexcept
    try
    {
      if (! prepared_) return std::unexpected(psql_error{.message = "statement is not prepared", .sql_state = ""});
      if (auto layout = check_layout(); ! layout) return std::unexpected(layout.error());

      /// a batch of parameter rows has no protocol level equivalent to send in
      /// one message - see execute_batch() for how it is still done in one
      /// round trip's worth of network latency
      if constexpr (has_p)
        if (par_->buffer_size() > 1) return execute_batch();

      PGconn* conn = db_->get_conn();

      std::vector<std::string> holders;
      std::vector<const char*> values;
      if constexpr (has_p)
      {
        constexpr auto pd = params::buffer_description_const();
        auto           pi = par_->buffer_description_init();
        marshal_row(pd, pi, 0, holders, values);
      }

      detail::result_holder res{PQexecPrepared(conn,
                                               stmt_name_.c_str(),
                                               static_cast<int>(values.size()),
                                               values.empty() ? nullptr : values.data(),
                                               nullptr, // all text, so no lengths needed
                                               nullptr, // all text, so no formats needed
                                               0)};     // ask for text results

      const auto status = PQresultStatus(res.get());
      if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) return std::unexpected(psql_error::from_result(res.get(), conn));

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
  private:
    /**
     * @brief send every row of the parameter buffer in one round trip
     *
     * PostgreSQL's wire protocol has no "one Bind+Execute, N parameter sets"
     * message the way ODBC does - each Bind+Execute is for one row. Pipeline
     * mode gets the same round trip saving a different way: every row's
     * Bind+Execute is sent without waiting for a reply, then all the replies
     * are read back together.
     *
     * All or nothing: PostgreSQL aborts the rest of a pipelined transaction
     * after the first error, with no implicit per-row savepoint. Adding one
     * would cost a round trip per row and defeat the point of pipelining, so
     * this does not attempt db2's "nine landed, one was refused" semantics -
     * one bad row fails the whole batch, and nothing from it lands. Callers
     * that need to know which row was bad still get it: row_status() marks
     * every row queued after the first failure as never having run, which is
     * different from db2's SQL_PARAM_ERROR-on-the-bad-row-only meaning of the
     * same array. See test_crud_batch_psql.cpp for the case this is written
     * against.
     */
    [[nodiscard]] std::expected<void, psql_error> execute_batch() noexcept
    try
    {
      PGconn*        conn = db_->get_conn();
      const size_t   rows = par_->buffer_size();
      constexpr auto pd   = params::buffer_description_const();
      auto           pi   = par_->buffer_description_init();

      if (PQenterPipelineMode(conn) != 1)
        return std::unexpected(psql_error{.message = "could not enter libpq pipeline mode", .sql_state = ""});

      /// how many rows were actually queued before a send failed - a send
      /// failure (e.g. the socket buffer is full and PQflush would be needed)
      /// still has to be synced and drained for exactly that many rows before
      /// the pipeline can be exited cleanly; rows after it were never sent, so
      /// row_status() is left untouched rather than guessed at for them
      size_t                    sent = 0;
      std::optional<psql_error> send_error;
      for (; sent < rows; ++sent)
      {
        std::vector<std::string> holders;
        std::vector<const char*> values;
        marshal_row(pd, pi, sent, holders, values);
        if (PQsendQueryPrepared(
              conn, stmt_name_.c_str(), static_cast<int>(values.size()), values.empty() ? nullptr : values.data(), nullptr, nullptr, 0) !=
            1)
        {
          send_error = psql_error{.message = PQerrorMessage(conn), .sql_state = ""};
          break;
        }
      }
      PQpipelineSync(conn);

      par_->clear_row_status();
      auto                      status_span = par_->row_status();
      std::optional<psql_error> first_error;
      for (size_t r = 0; r < sent; ++r)
      {
        const detail::result_holder res{PQgetResult(conn)};
        const auto            est = PQresultStatus(res.get());
        if (! first_error && est != PGRES_COMMAND_OK && est != PGRES_TUPLES_OK) first_error = psql_error::from_result(res.get(), conn);
        status_span[r] = (! first_error) ? psql::param_status_ok : psql::param_status_error;
        {
          const detail::result_holder end_of_command{PQgetResult(conn)};
        } // nullptr - marks the end of this row's result
      }
      {
        const detail::result_holder sync{PQgetResult(conn)};
      } // PGRES_PIPELINE_SYNC
      {
        const detail::result_holder end_of_sync{PQgetResult(conn)};
      } // nullptr again

      PQexitPipelineMode(conn);

      /// a send failure is reported even when every row that did get sent
      /// came back clean - the batch as a whole did not go through
      if (send_error) return std::unexpected(*send_error);
      if (first_error) return std::unexpected(*first_error);

      total_rows_    = 0;
      next_row_      = 0;
      affected_rows_ = static_cast<int64_t>(rows);
      rows_          = detail::result_holder{nullptr};
      db_->get_logger()->info("Batch of {} rows executed.", rows);
      return {};
    }
    catch (const std::exception& e)
    {
      return std::unexpected(psql_error{.message = e.what(), .sql_state = ""});
    }
  public:
    /**
     * @brief copy the next batch of rows into the result buffer
     *
     * @return true when at least one row was copied, false at end of data
     */
    [[nodiscard]] std::expected<bool, psql_error> fetch() noexcept
    try
    {
      if constexpr (! has_r) return false; // NOLINT(readability-inconsistent-ifelse-braces)
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

    [[nodiscard]] std::conditional_t<has_p, std::shared_ptr<params>, std::monostate> get_param() noexcept
    {
      if constexpr (has_p) return par_;
      else return std::monostate{};
    }


    /**
     * @brief the result buffer, writable
     *
     * Separate from get_result() because that one hands out a const pointer for
     * reading fetched rows, while sizing the buffer is a write and has to
     * happen before prepare().
     */
    [[nodiscard]] std::conditional_t<has_r, std::shared_ptr<results>, std::monostate> get_result_buffer() noexcept
    {
      if constexpr (has_r) return res_;
      else return std::monostate{};
    }
    [[nodiscard]] std::conditional_t<has_r, std::shared_ptr<const results>, std::monostate> get_result() const noexcept
    {
      if constexpr (has_r) return res_;
      else return std::monostate{};
    }

    [[nodiscard]] size_t rows_fetched() const noexcept { return rows_fetched_; }
    [[nodiscard]] size_t total_rows() const noexcept { return total_rows_; }

    [[nodiscard]] std::conditional_t<has_r, size_t, std::monostate> occupied_count() const noexcept
    {
      if constexpr (has_r) return res_->occupied();
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
      if constexpr (has_p) par_->reset_all_null();
      if constexpr (has_r) res_->set_occupied(0);
    }
  };

} // namespace rtl
