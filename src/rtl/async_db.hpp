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
 * ## Polling instead of blocking: exec()/is_finished()
 *
 * submit()/execute_sync() are the original pair: submit() tells the caller
 * nothing at all (not even "accepted"), execute_sync() tells the caller
 * everything but only once the whole pipeline has drained. exec()/is_finished()
 * sit between the two, modelled on the ODBC "SQL_STILL_EXECUTING" pattern
 * (SQLExecute on a statement handle set to SQL_ATTR_ASYNC_ENABLE returns
 * immediately with SQL_STILL_EXECUTING while the driver is still working, and
 * the caller re-issues the same call to find out whether it is done yet) -
 * adapted here to the one-job-deep queue this facade already has, rather than
 * to a single statement handle:
 *
 * - **exec()** is asynchronous by definition - it hands a job to the worker
 *   and returns - but it *blocks* if the one-deep queue is not free yet
 *   (a job is still pending, or the worker is still busy with the previous
 *   one). There is nothing to poll for at the point of a full queue: the
 *   caller has no other job it could be doing that would not itself need this
 *   same queue slot, so exec() simply waits for room the way submit() always
 *   silently did - the difference is that exec() also carries a return value.
 * - **is_finished()** never blocks. It reports `still_pending` while the
 *   worker has not yet finished the last job handed to it (via exec()),
 *   `finished` once the queue is empty and the worker is idle - the same
 *   condition drain() waits for, just observed instead of waited on - or the
 *   sticky error if that last job (or an earlier one still remembered) failed.
 *   Because the queue is exactly one job deep, "the queue accepted a new job"
 *   and "the previous job is done" are the same event - there is no separate
 *   per-job result to track, only ever one job in flight and, briefly, one
 *   just-finished result sitting in `pending_`'s empty slot.
 *
 * What `finished` does NOT say is "rows are waiting" versus "the statement
 * had no rows to begin with" - that distinction was never is_finished()'s to
 * make. The caller already knows which kind of statement it ran: a
 * `query_handle<P, rtl::no_results>` (insert/update/delete) has no fetch()/
 * fetch_more() to call in the first place (see query_handle's own has_results
 * gate below), while a `query_handle<P, R>` with a real result type does, and
 * calling it after a `finished` result is exactly the same "drain the next
 * buffer of rows" loop execute_sync()/fetch_more() already use. Nothing here
 * introduces a second way to ask "was this a select" - the type the caller
 * already has answers that.
 *
 * Calling exec() again before a select's own results have been fully drained
 * is not a distinct error state: it is simply "the queue is not free yet",
 * so exec() blocks exactly as it does for a still-running insert - the caller
 * falls back to synchronous behaviour rather than losing the pending rows or
 * hitting a special case.
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

  /**
   * @brief what is_finished()/exec() report once a job has actually been
   * accepted into the one-deep queue - see async_db's own "Polling instead of
   * blocking" doc comment for the full picture.
   *
   * Deliberately carries no payload beyond this: which handle's results are
   * now readable is something the caller already knows (it is whichever
   * handle it last called exec() with), and whether that statement even
   * has rows to fetch follows from the handle's own type
   * (query_handle<P, R>::returns_rows), not from anything reported here.
   */
  enum class exec_status : uint8_t
  {
    still_pending, ///< the worker has not finished the last job yet
    finished,      ///< the queue is empty and the worker is idle - the last job (if any) is done
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
      /// abort this statement's own execute()/fetch(), if one is running -
      /// see query<>::cancel() (backend-specific) for what "abort" means on
      /// each side, and async_db::cancel() for the one caller
      [[nodiscard]] virtual bool cancel() const noexcept = 0;
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

      [[nodiscard]] bool cancel() const noexcept override { return q_.cancel(); }

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
     * @brief take over a connected database and start its worker thread
     *
     * The database must already be connected, and must not be touched by the
     * caller for as long as the returned object lives - that is the whole
     * contract. Ownership stays with the caller; only use of it moves here.
     *
     * A plain constructor cannot report failure the way the rest of this
     * class does (std::expected throughout): std::thread's own constructor
     * throws std::system_error if the OS refuses to create the thread (e.g.
     * RLIMIT_NPROC or a system-wide thread cap already reached - a real
     * possibility, not a theoretical one, for code that opens several
     * async_db instances in a row, as -j/--parallel-style workloads do). A
     * constructor that can throw would be the one place in this class where
     * failure looks different from everywhere else in it, and would force a
     * caller into try/catch just to construct one - so thread creation
     * happens here instead, in a factory that reports it the same way
     * prepare()/exec()/commit() already do.
     *
     * @return an async_db ready to use, or the failure std::thread's own
     *         constructor reported, translated into a db_error
     *         (db_sts::unknown; message carries std::system_error::what())
     */
    [[nodiscard]] static std::expected<std::unique_ptr<async_db>, db_error> create(db& database);

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
     * @brief hand a statement to the worker, blocking only if the queue is not free yet
     *
     * See the class's own "Polling instead of blocking" doc comment for the
     * model this and is_finished() together implement.
     *
     * Works for both kinds of statement, unlike submit() (no_results only):
     * for a row-returning handle, the point of exec() is precisely to let the
     * caller find out later, via is_finished(), when it is safe to fetch()/
     * fetch_more() without blocking to find out.
     *
     * Blocks exactly as long as the one-deep queue is occupied - i.e. exactly
     * as long as submit() always silently blocked in that case, no longer.
     * Once room is free, the wait is over: the previous job (if there was
     * one) is now known to be finished, this job is now the one pending, and
     * exec() returns without waiting for THIS job to run.
     *
     * @return exec_status::finished immediately (this job is now queued,
     *         nothing was in the way) - never still_pending, since exec()
     *         does not return until the queue actually has room - or the
     *         first sticky error if one is already pending (the job is
     *         silently discarded, same as submit()'s own contract).
     */
    template <typename params, typename results>
    [[nodiscard]] std::expected<exec_status, db_error> exec(const query_handle<params, results>& h);

    /**
     * @brief poll whether the worker has finished the last job, without waiting
     *
     * Never blocks. See the class's own "Polling instead of blocking" doc
     * comment for what `finished` does and does not tell the caller.
     *
     * @return still_pending while a job is queued or the worker is still
     *         running one; finished once the queue is empty and the worker
     *         is idle; the sticky error if the last job (or an earlier one
     *         still remembered) failed.
     */
    [[nodiscard]] std::expected<exec_status, db_error> is_finished() const;

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

    /**
     * @brief abort whatever statement the worker is currently running, from
     * any other thread
     *
     * Distinct from the destructor's own drain-then-stop, which waits for a
     * running statement to finish on its own rather than interrupting it -
     * cancel() is the one way to reach in and abort a statement that has
     * been running longer than the caller is willing to wait for (a timeout,
     * a user-initiated abort, ...), without waiting it out first. See the
     * class's own "Polling instead of blocking" doc comment's sibling note on
     * why this could not simply be part of the destructor: the destructor
     * only ever runs once the caller is done with the object entirely, while
     * cancel() is meant to be called while the object is still very much in
     * use, from a thread that is not the one blocked inside exec()/
     * execute_sync()/drain()/commit() at the time.
     *
     * Whether the aborted statement's own exec()/execute_sync()/drain() call
     * (on whichever thread is blocked in it) comes back with the cancellation
     * as its error, or with whatever error the backend happens to report for
     * an interrupted round trip, is backend and timing dependent - either
     * way, that call returns instead of continuing to block, which is
     * cancel()'s entire purpose. A canceled statement still counts as a
     * failure for the sticky-error/rollback machinery like any other.
     *
     * @return true if a statement was actually running and the cancel
     *         request was sent for it; false if nothing is currently running
     *         (there was nothing to cancel) or the request itself could not
     *         be sent - either way says nothing about whether the statement
     *         has actually stopped by the time this returns
     */
    [[nodiscard]] bool cancel() const noexcept;
  private:
    /**
     * @brief bind to the connection without starting the worker thread yet
     *
     * Only reachable through create() - see its own doc comment for why
     * thread creation is not done here. A default-constructed worker_
     * (not-a-thread) is what create() replaces with the real one, once
     * std::thread's own constructor has actually succeeded.
     */
    explicit async_db(db& database);

    /// a unit of work for the worker: run it, and say whether it failed
    using job = std::function<void()>;

    /**
     * @brief a queued job together with which tasks_ entry (if any) it runs -
     * see running_task_id_'s own doc comment for why cancel() needs this.
     */
    struct queued_job
    {
      job                   fn;
      std::optional<size_t> task_id;
    };

    void worker_loop();
    /// hand one job over and wait for the worker to finish it
    /// @param even_after_error run it even when the sticky error is set -
    ///        only COMMIT and ROLLBACK, which are what clears that state
    void run_on_worker(job j, bool even_after_error = false, std::optional<size_t> task_id = std::nullopt);
    /// hand one job over and return; blocks only while the queue is full
    void post_to_worker(job j, bool even_after_error = false, std::optional<size_t> task_id = std::nullopt);
    /// record the first error and leave later ones alone
    void note_error(const db_error& e);
  private:
    db&                                             db_;
    std::vector<std::unique_ptr<detail::task_base>> tasks_;

    mutable std::mutex      mtx_;
    std::condition_variable to_worker_;   ///< a job is waiting, or it is time to stop
    std::condition_variable from_worker_; ///< the queue drained, or a job finished

    std::optional<queued_job> pending_;      ///< the queue, one deep by design
    bool                      busy_ = false; ///< the worker has a job in hand
    bool                      stop_ = false;
    std::optional<db_error>   error_; ///< sticky: the first failure wins

    /**
     * @brief tasks_ index of the job currently running, if it is one exec()/
     * submit()/execute_sync() actually queued for a user statement - what
     * cancel() reaches for. std::nullopt for an internal job (prepare()'s own
     * setup, commit(), rollback()) that runs directly against db_ rather than
     * through a tasks_ entry, and for whenever nothing is running at all.
     * Set under mtx_ right before the worker calls j() (see worker_loop()),
     * so cancel() - called from any other thread - reads a value that is
     * either "nothing running" or "this IS the job in flight right now",
     * never a stale one from a job that already finished.
     */
    std::optional<size_t> running_task_id_;

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
      },
      /*even_after_error=*/false,
      /*task_id=*/id); ///< lets cancel() reach a submit()'d job too, same as an exec()'d one
  }

  template <typename params, typename results>
  std::expected<exec_status, db_error> async_db::exec(const query_handle<params, results>& h)
  {
    if (! h.is_valid())
      return std::unexpected(db_error{.sts           = db_sts::invalid_handle,
                                      .message       = "the handle does not name a prepared statement",
                                      .sql_state     = "",
                                      .driver_status = 0,
                                      .native_error  = 0});

    const size_t id = h.id();
    /// Same snapshot-before-queueing contract as submit() - see its own doc
    /// comment. Taken unconditionally, even for a row-returning statement:
    /// exec() does not wait for this job to run, so the caller may already be
    /// free to reuse its own parameter buffer by the time this returns.
    const void* src = nullptr;
    if constexpr (params::has_parameters()) src = h.param();
    auto snap = std::make_shared<detail::param_snapshot>(tasks_.at(id)->snapshot_params(src));

    /// post_to_worker() blocks here for exactly as long as the one-deep queue
    /// is occupied - see exec()'s own doc comment for why that wait is not
    /// something is_finished() could have reported instead. Once it returns,
    /// this job is the one pending (or was silently discarded because a
    /// sticky error is already set - see post_to_worker()'s own doc comment),
    /// and the PREVIOUS job, if any, is known to be finished.
    post_to_worker(
      [this, id, snap]
      {
        if constexpr (params::has_parameters()) tasks_.at(id)->restore_params(*snap);
        if (auto e = tasks_.at(id)->execute(); e)
        {
          note_error(*e);
          return;
        }
        if constexpr (results::has_results())
        {
          std::optional<db_error> ferr;
          (void)tasks_.at(id)->fetch(ferr); ///< first buffer of rows, same as execute_sync()'s own first fetch()
          if (ferr) note_error(*ferr);
        }
      },
      /*even_after_error=*/false,
      /*task_id=*/id); ///< without this, running_task_id_ never gets set for an exec()'d job, and cancel() can never find anything to
                       ///< cancel

    if (auto e = error()) return std::unexpected(*e);
    return exec_status::finished;
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
      },
      /*even_after_error=*/false,
      /*task_id=*/id); ///< lets another thread's cancel() reach the statement this call is blocked on

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
      },
      /*even_after_error=*/false,
      /*task_id=*/id); ///< lets another thread's cancel() reach the fetch this call is blocked on

    if (auto e = error()) return std::unexpected(*e);
    return fetched;
  }

} // namespace rtl
