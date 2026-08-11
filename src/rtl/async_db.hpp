// async_db.hpp
#pragma once
/**
 * @file
 * @brief run a sequence of statements on a worker thread, one connection deep
 *
 * The workload this exists for is `connect, q1, q2, ..., qn, commit` where
 * q1..qn are *different* statements - different parameter types, different
 * tables - all on one connection inside one transaction. Synchronously the
 * main thread sits idle for the length of every round trip. Here it hands
 * each statement over and goes back to generating the data for the next one,
 * so application work overlaps database latency.
 *
 * What it does not do is run SQL in parallel: one connection carries one
 * statement at a time, and both libpq and ODBC forbid using a connection from
 * two threads at once. Horizontal throughput needs several connections, which
 * src/programs/appl.cpp already arranges at the generator level.
 *
 * Backend neutral by construction rather than by abstraction: `rtl::db`,
 * `rtl::query<P,R>` and `rtl::to_db_error()` all name the same things in
 * either backend, and exactly one backend library is linked into any given
 * executable. So this compiles against whichever one that is, with no #ifdef
 * and no virtual layer of its own - the same arrangement make_db() uses.
 *
 * ## What runs asynchronously, and what does not
 *
 * A statement that returns no rows (`R = rtl::no_results`) is submitted and
 * forgotten - submit() returns as soon as the worker has taken it. A
 * statement that returns rows has to be run through execute_sync(), which
 * waits: fetch() is a loop, the caller decides each time round whether to ask
 * for more, and that decision cannot be made ahead of the answer.
 *
 * The split is made at compile time from `R`, never by looking at the SQL.
 * The generator already emits `rtl::no_results` for statements that return
 * nothing, having asked the database itself through get_sql_metadata(), so
 * the classification is inherited from where the SQL is actually understood.
 * submit() on a row-returning statement fails to compile rather than
 * misbehaving at run time.
 *
 * Ordering is total either way - one worker, one queue - so a select still
 * sees everything submitted before it. The cost is that execute_sync() has to
 * drain the pipeline first, which is why a run goes fastest when the
 * row-returning statements are grouped at the end rather than interleaved.
 *
 * ## Errors are sticky
 *
 * The outcome of q(i) is not needed before q(i+1) is submitted, so nothing
 * blocks to collect one. Instead the worker keeps the *first* error, refuses
 * every task after it, and commit() reports it and rolls back. Same idea as
 * std::ostream's failbit: keep going, fail at the end. This also matches what
 * the databases do - after a failed statement PostgreSQL puts the transaction
 * into 25P02 and refuses everything until a rollback anyway.
 *
 * ## Lifetime
 *
 * The destructor drains what was already submitted, stops the worker and
 * joins it before releasing anything. It does NOT commit: a transaction still
 * open at that point is rolled back by the backend's disconnect(). Committing
 * on the way out would make a forgotten commit() look like it had worked.
 */

#include "db_error.hpp"
#include "no_results.hpp"
#include "rtl.hpp"
/// The backend's own query.hpp - db2's or psql's, whichever include path this
/// is compiled with. It brings rtl::query<>, rtl::database and that backend's
/// to_db_error(), all under the same names, which is what lets everything
/// below be written once. IWYU pragma: keep
#include "query.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace rtl
{
  /**
   * @brief what the caller holds instead of the query object itself
   *
   * The query<P,R> lives on the worker side, because only the worker may
   * touch the connection. The caller still has to say which of the n
   * statements to run, and still has to fill in its parameters - so the
   * handle carries the id for the first and a shared_ptr to the parameter
   * buffer for the second. param() is a plain pointer return, not a message
   * to the worker.
   *
   * Cheap to copy and safe to keep: the buffers outlive the handle either
   * way, since the worker holds the same shared_ptr.
   */
  template <typename params, typename results>
  class query_handle
  {
  public:
    using param_type  = params;
    using result_type = results;

    /// whether this statement returns rows - decides submit() vs execute_sync()
    static constexpr bool returns_rows = results::has_results();

    query_handle() = default;

    /**
     * @brief the parameter buffer to fill in before submitting
     *
     * Only exists when the statement takes parameters; asking otherwise is a
     * compile error rather than a null return.
     */
    [[nodiscard]] params* param() const noexcept
      requires(params::has_parameters())
    { return par_.get(); }

    /**
     * @brief the result buffer, readable after execute_sync()
     */
    [[nodiscard]] results* result() const noexcept
      requires(results::has_results())
    { return res_.get(); }

    [[nodiscard]] bool   is_valid() const noexcept { return id_ != npos; }
    [[nodiscard]] size_t id() const noexcept { return id_; }
  private:
    template <typename>
    friend class async_db_impl;
    friend class async_db;

    static constexpr size_t npos = static_cast<size_t>(-1);

    query_handle(size_t id, std::shared_ptr<params> par, std::shared_ptr<results> res)
    : id_(id)
    , par_(std::move(par))
    , res_(std::move(res))
    {
    }

    size_t                   id_ = npos;
    std::shared_ptr<params>  par_;
    std::shared_ptr<results> res_;
  };

  namespace detail
  {
    /**
     * @brief the values of one parameter buffer, lifted out of it
     *
     * One entry per column, each holding that column's whole value array and
     * its indicator array as plain bytes. Nothing here points back into the
     * buffer it came from, which is the entire point: the caller may refill
     * the buffer the moment submit() returns.
     */
    struct param_snapshot
    {
      struct column
      {
        std::vector<std::byte> values;     ///< the value array, bytes as they lie
        std::vector<int32_t>   indicators; ///< length/null indicator per row
      };
      std::vector<column> columns;
    };

    /**
     * @brief one registered statement, with its concrete type erased
     *
     * The queries are of different types - that is the whole point of the
     * workload - so they cannot share a container without this. execute() and
     * fetch_all() are the only things the worker ever needs to do to one.
     */
    struct task_base
    {
      task_base()                            = default;
      virtual ~task_base()                   = default;
      task_base(const task_base&)            = delete;
      task_base& operator=(const task_base&) = delete;
      task_base(task_base&&)                 = delete;
      task_base& operator=(task_base&&)      = delete;

      /// run the statement with whatever is in the parameter buffer now
      [[nodiscard]] virtual std::optional<db_error> execute() = 0;
      /**
       * @brief take a copy of the parameter *values* as they stand
       *
       * Called by submit() on the caller's thread, so that the caller can go
       * on filling in the next row while this snapshot waits its turn. The
       * copy is held by the queued job and put back by restore_params() just
       * before the statement runs.
       *
       * Deliberately a copy of the bytes rather than of the buffer object.
       * The generated buffers are copyable, but their copy assignment also
       * copies dscr_init_, which holds raw pointers into the source's own
       * vectors - so an assigned-to buffer would describe storage belonging
       * to the snapshot, and the runtime, which reads through those pointers,
       * would be reading a temporary that dies on the next submit.
       */
      [[nodiscard]] virtual param_snapshot snapshot_params(const void* src) = 0;
      /// put a snapshot back, on the worker thread, just before execute()
      virtual void restore_params(const param_snapshot& snap) = 0;
      /// copy one buffer's worth of rows into the result buffer
      /// @return whether any rows were copied; nullopt-free errors go in `err`
      [[nodiscard]] virtual bool fetch(std::optional<db_error>& err) = 0;
    };

    /// a task holding the real query<P,R>
    template <typename params, typename results>
    class task final : public task_base
    {
    public:
      using query_type = query<params, results>;
      using param_type = params;

      explicit task(query_type q)
      : q_(std::move(q))
      {
      }

      [[nodiscard]] std::optional<db_error> execute() override
      {
        if (auto r = q_.execute(); ! r) return to_db_error(r.error());
        return std::nullopt;
      }

      [[nodiscard]] param_snapshot snapshot_params(const void* src) override
      {
        param_snapshot snap;
        if constexpr (param_type::has_parameters())
        {
          /// const_cast: buffer_description_init() is non-const because it
          /// republishes the pointers it hands out, but nothing here writes
          /// through them - this only reads the caller's values out.
          auto*        buf  = const_cast<param_type*>(static_cast<const param_type*>(src)); // NOLINT
          const auto   pi   = buf->buffer_description_init();
          const size_t rows = buf->buffer_size();
          snap.columns.reserve(pi.size());
          for (const auto& c : pi)
          {
            const size_t bytes = c.stride * rows;
            auto&        col   = snap.columns.emplace_back();
            col.values.resize(bytes);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            std::memcpy(col.values.data(), c.value_ptr, bytes);
            col.indicators.assign(c.indicator_ptr, c.indicator_ptr + rows); // NOLINT
          }
        }
        return snap;
      }

      void restore_params(const param_snapshot& snap) override
      {
        if constexpr (param_type::has_parameters())
        {
          const auto   pi   = q_.get_param()->buffer_description_init();
          const size_t rows = q_.get_param()->buffer_size();
          for (size_t i = 0; i < pi.size() && i < snap.columns.size(); ++i)
          {
            const auto& col = snap.columns[i]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            std::memcpy(pi[i].value_ptr, col.values.data(), col.values.size());
            std::copy_n(col.indicators.begin(), std::min(rows, col.indicators.size()), pi[i].indicator_ptr); // NOLINT
          }
        }
      }

      [[nodiscard]] bool fetch(std::optional<db_error>& err) override
      {
        auto r = q_.fetch();
        if (! r)
        {
          err = to_db_error(r.error());
          return false;
        }
        return *r;
      }

      [[nodiscard]] query_type& qry() noexcept { return q_; }
    private:
      query_type q_;
    };
  } // namespace detail

  /**
   * @brief the facade - one per connection
   *
   * Not one per query: the transaction is one, and the connection may only be
   * used from one thread, so every statement serialises through the same
   * worker regardless.
   */
  class async_db
  {
  public:
    /**
     * @brief take over a connected database for the worker thread
     *
     * The database must already be connected, and must not be touched by the
     * caller for as long as this object lives - that is the whole contract.
     * Ownership stays with the caller; only use of it moves here.
     */
    explicit async_db(db& database);
    ~async_db();

    async_db(const async_db&)            = delete;
    async_db& operator=(const async_db&) = delete;
    async_db(async_db&&)                 = delete;
    async_db& operator=(async_db&&)      = delete;

    /**
     * @brief register a statement and prepare it
     *
     * Prepared synchronously: a statement that will not prepare is a fault in
     * the program rather than in the data, and the caller should hear about
     * it at the point it registers, not three submits later.
     *
     * The buffer sizes are arguments rather than something to set on the
     * returned handle, because set_buffer_size() reallocates the column
     * arrays and both runtimes refuse to run against buffers that moved after
     * prepare() (check_layout()). Sizing has to happen first, and the only
     * moment before prepare() that the caller can reach is this call.
     *
     * @tparam params  the generated parameter buffer type, or rtl::no_params
     * @tparam results the generated result buffer type, or rtl::no_results
     * @param sql the statement text, as the generated qry::sql() gives it
     * @param param_rows how many parameter rows per execute; >1 is a batch
     * @param result_rows how many rows one fetch may return
     * @return a handle, or the error that prepare() reported
     */
    template <typename params, typename results>
    [[nodiscard]] std::expected<query_handle<params, results>, db_error> prepare(std::string_view sql,
                                                                                 size_t           param_rows  = 1,
                                                                                 size_t           result_rows = 1);

    /**
     * @brief hand a statement to the worker and return without waiting
     *
     * There is one parameter buffer per statement, and the worker reads it
     * when it runs the job - so submit() takes a snapshot of it before
     * queueing, and the caller may start filling in the next row the moment
     * this returns. Without the copy the caller would be overwriting the
     * buffer the worker is about to read, which shows up as the same row
     * being inserted twice.
     *
     * The copy is one row of parameters, which is small next to the round
     * trip it buys. Double buffering would avoid it, but only by making
     * param() point somewhere different after every submit - and a pointer
     * that moves under the caller is a worse bargain than a memcpy.
     *
     * Only for statements that return no rows; see the file comment.
     *
     * Does nothing once an error is pending: the transaction is already lost
     * at that point, and the error is reported by commit().
     */
    template <typename params, typename results>
    void submit(const query_handle<params, results>& h)
      requires(! query_handle<params, results>::returns_rows);

    /**
     * @brief run a statement and wait for it, then fetch one buffer of rows
     *
     * Drains everything submitted before it first, so the statement sees the
     * full transaction. Repeat calls to fetch_more() walk the rest of the
     * result set.
     *
     * @return whether any rows were fetched, or the first error so far
     */
    template <typename params, typename results>
    [[nodiscard]] std::expected<bool, db_error> execute_sync(const query_handle<params, results>& h);

    /**
     * @brief fetch the next buffer of rows from the last execute_sync()
     *
     * @return whether any rows were fetched; false at end of data
     */
    template <typename params, typename results>
    [[nodiscard]] std::expected<bool, db_error> fetch_more(const query_handle<params, results>& h);

    /**
     * @brief wait for everything submitted so far to finish
     *
     * Rarely needed directly - commit() and execute_sync() drain on their own.
     */
    void drain();

    /**
     * @brief finish the transaction
     *
     * Drains first. If any statement failed, the transaction is rolled back
     * instead and that first error is returned - a commit that reported
     * success after a failed insert would be the worst possible outcome here.
     */
    [[nodiscard]] std::expected<void, db_error> commit();

    /// drain, then roll back. Clears the sticky error, so the object is
    /// usable again afterwards.
    [[nodiscard]] std::expected<void, db_error> rollback();

    /// the first error since the last commit/rollback, if any
    [[nodiscard]] std::optional<db_error> error() const;
    /// whether a statement has failed and further submits are being discarded
    [[nodiscard]] bool has_error() const;
  private:
    /// a unit of work for the worker: run it, and say whether it failed
    using job = std::function<void()>;

    void worker_loop();
    /// hand one job over and wait for the worker to finish it
    /// @param even_after_error run it even when the sticky error is set -
    ///        only COMMIT and ROLLBACK, which are what clears that state
    void run_on_worker(const job& j, bool even_after_error = false);
    /// hand one job over and return; blocks only while the queue is full
    void post_to_worker(job j, bool even_after_error = false);
    /// record the first error and leave later ones alone
    void note_error(const db_error& e);

    db&                                             db_;
    std::vector<std::unique_ptr<detail::task_base>> tasks_;

    mutable std::mutex      mtx_;
    std::condition_variable to_worker_;   ///< a job is waiting, or it is time to stop
    std::condition_variable from_worker_; ///< the queue drained, or a job finished

    std::optional<job>      pending_;      ///< the queue, one deep by design
    bool                    busy_ = false; ///< the worker has a job in hand
    bool                    stop_ = false;
    std::optional<db_error> error_; ///< sticky: the first failure wins

    std::thread worker_;
  };

  // --------------------------------------------------------------------------
  // template members
  // --------------------------------------------------------------------------

  template <typename params, typename results>
  std::expected<query_handle<params, results>, db_error> async_db::prepare(std::string_view sql, size_t param_rows, size_t result_rows)
  {
    using query_type = query<params, results>;

    /// The query is built and prepared on the worker, because both touch the
    /// connection. unique_ptr keeps it at a fixed address for its whole life,
    /// which the drivers require - see the note on query's move constructor.
    std::unique_ptr<detail::task<params, results>> t;
    std::optional<db_error>                        err;

    const std::string sql_copy{sql}; ///< the view may not outlive the call

    run_on_worker(
      [&]
      {
        /// db and database are separate bases - db_psql derives from both, and
        /// query<> wants the latter. The cast is what connects the connection
        /// object the caller handed over to the interface a query needs.
        auto* d = dynamic_cast<database*>(&db_);
        if (d == nullptr)
        {
          err = db_error{.sts           = db_sts::invalid_handle,
                         .message       = "the database object does not implement rtl::database",
                         .sql_state     = "",
                         .driver_status = 0,
                         .native_error  = 0};
          return;
        }
        auto q = query_type(d, sql_copy);
        /// Sizing has to come before prepare(): it reallocates the column
        /// arrays, and prepare() is what records where they ended up.
        if constexpr (params::has_parameters())
          if (param_rows > 1) q.get_param()->set_buffer_size(param_rows);
        if constexpr (results::has_results()) q.get_result_buffer()->set_buffer_size(result_rows);
        if (auto r = q.prepare(); ! r)
        {
          err = to_db_error(r.error());
          return;
        }
        t = std::make_unique<detail::task<params, results>>(std::move(q));
      });

    if (err) return std::unexpected(*err);

    /**
     * The caller writes into a buffer of its own, NOT the one the query holds.
     *
     * They cannot be the same object: the worker is reading the query's buffer
     * for the statement in flight at the very moment the caller is filling in
     * the next row, and one buffer cannot hold both. submit() copies the
     * caller's values across (snapshot_params/restore_params) precisely so
     * that these two can move independently.
     *
     * The result buffer is shared, because reading it is only meaningful
     * after execute_sync() has finished - there is no overlap to protect
     * against.
     */
    std::shared_ptr<params>  par;
    std::shared_ptr<results> res;
    if constexpr (params::has_parameters())
    {
      par = std::make_shared<params>();
      if (param_rows > 1) par->set_buffer_size(param_rows);
    }
    if constexpr (results::has_results()) res = t->qry().get_result_buffer();

    const size_t id = tasks_.size();
    tasks_.push_back(std::move(t));
    return query_handle<params, results>(id, std::move(par), std::move(res));
  }

  template <typename params, typename results>
  void async_db::submit(const query_handle<params, results>& h)
    requires(! query_handle<params, results>::returns_rows)
  {
    if (! h.is_valid()) return;
    const size_t id = h.id();
    /// Taken here, on the caller's thread, before the job is queued: from the
    /// moment this returns the caller owns its buffer again and may write the
    /// next row into it, while the worker runs from this copy.
    const void* src = nullptr;
    if constexpr (params::has_parameters()) src = h.param();
    auto snap = std::make_shared<detail::param_snapshot>(tasks_.at(id)->snapshot_params(src));
    post_to_worker(
      [this, id, snap]
      {
        tasks_.at(id)->restore_params(*snap);
        if (auto e = tasks_.at(id)->execute(); e) note_error(*e);
      });
  }

  template <typename params, typename results>
  std::expected<bool, db_error> async_db::execute_sync(const query_handle<params, results>& h)
  {
    if (! h.is_valid())
      return std::unexpected(db_error{.sts           = db_sts::invalid_handle,
                                      .message       = "the handle does not name a prepared statement",
                                      .sql_state     = "",
                                      .driver_status = 0,
                                      .native_error  = 0});

    const size_t id      = h.id();
    bool         fetched = false;
    /// The caller waits for this one, so its buffer is not moving under us -
    /// but the query still has a parameter buffer of its own, and the values
    /// have to be carried across to it just the same.
    detail::param_snapshot snap;
    if constexpr (params::has_parameters()) snap = tasks_.at(id)->snapshot_params(h.param());

    run_on_worker(
      [&]
      {
        if (has_error()) return; ///< the transaction is already lost
        if constexpr (params::has_parameters()) tasks_.at(id)->restore_params(snap);
        if (auto e = tasks_.at(id)->execute(); e)
        {
          note_error(*e);
          return;
        }
        if constexpr (results::has_results())
        {
          std::optional<db_error> ferr;
          fetched = tasks_.at(id)->fetch(ferr);
          if (ferr) note_error(*ferr);
        }
      });

    if (auto e = error()) return std::unexpected(*e);
    return fetched;
  }

  template <typename params, typename results>
  std::expected<bool, db_error> async_db::fetch_more(const query_handle<params, results>& h)
  {
    static_assert(results::has_results(), "fetch_more() is only for statements that return rows");
    if (! h.is_valid())
      return std::unexpected(db_error{.sts           = db_sts::invalid_handle,
                                      .message       = "the handle does not name a prepared statement",
                                      .sql_state     = "",
                                      .driver_status = 0,
                                      .native_error  = 0});

    const size_t id      = h.id();
    bool         fetched = false;
    run_on_worker(
      [&]
      {
        if (has_error()) return;
        std::optional<db_error> ferr;
        fetched = tasks_.at(id)->fetch(ferr);
        if (ferr) note_error(*ferr);
      });

    if (auto e = error()) return std::unexpected(*e);
    return fetched;
  }

} // namespace rtl
