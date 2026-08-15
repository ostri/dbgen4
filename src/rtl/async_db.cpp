// async_db.cpp
#include "async_db.hpp"
#include <mutex>
#include <system_error>
#include <utility>

namespace rtl
{
  async_db::async_db(db& database)
  : db_(database)
  {
    /// worker_ stays a default-constructed (not-a-thread) std::thread until
    /// create() starts the real one - see the ctor's own doc comment.
  }

  std::expected<std::unique_ptr<async_db>, db_error> async_db::create(db& database)
  {
    /// make_unique rather than returning by value: async_db is deliberately
    /// non-movable (worker_loop() captures `this`, so a moved-from/moved-to
    /// pair sharing the thread's captured pointer would be a use-after-free
    /// the moment the source is destroyed) - see the class's own deleted
    /// move members.
    auto adb = std::unique_ptr<async_db>(new async_db(database));
    try
    {
      adb->worker_ = std::thread([raw = adb.get()] { raw->worker_loop(); });
    }
    catch (const std::system_error& e)
    {
      return std::unexpected(db_error{.sts           = db_sts::unknown,
                                      .message       = std::string("could not start async_db's worker thread: ") + e.what(),
                                      .sql_state     = "",
                                      .driver_status = 0,
                                      .native_error  = 0});
    }
    return adb;
  }

  async_db::~async_db()
  {
    /**
     * Drain-and-stop, deliberately: whatever was submitted before this point
     * was submitted on purpose, and dropping it would lose data silently.
     * What this does NOT do is commit - an open transaction is left to the
     * backend's disconnect() to roll back, so that a missing commit() fails
     * loudly rather than appearing to have worked.
     */
    drain();
    {
      const std::scoped_lock lock{mtx_};
      stop_ = true;
    }
    to_worker_.notify_all();
    if (worker_.joinable()) worker_.join();
  }

  void async_db::worker_loop()
  {
    while (true)
    {
      queued_job qj;
      {
        std::unique_lock lock{mtx_};
        to_worker_.wait(lock, [this] { return pending_.has_value() || stop_; });
        /// stop only once the queue is empty - the destructor has already
        /// drained, so this is the shutdown-after-drain path
        if (! pending_) return;
        qj = std::move(*pending_);
        pending_.reset();
        busy_            = true;
        running_task_id_ = qj.task_id; ///< set before j() runs - see its own doc comment for why
      }
      /// run outside the lock: this is where the database round trip happens,
      /// and holding the mutex across it would serialise the very thing this
      /// class exists to overlap
      from_worker_.notify_all(); ///< the queue has a slot again

      qj.fn();

      {
        const std::scoped_lock lock{mtx_};
        busy_ = false;
        running_task_id_.reset();
      }
      from_worker_.notify_all();
    }
  }

  void async_db::post_to_worker(job j, bool even_after_error, std::optional<size_t> task_id)
  {
    {
      std::unique_lock lock{mtx_};
      /// Nothing further is worth running once a statement has failed: the
      /// transaction cannot be salvaged, and commit() will report the error.
      ///
      /// COMMIT and ROLLBACK are the exception, and have to be - a rollback
      /// is the only thing that clears PostgreSQL's aborted transaction
      /// state, so refusing to send it because an error is pending would
      /// leave the connection permanently unusable, which is the opposite of
      /// what the sticky error is for.
      if (error_ && ! even_after_error) return;
      /// one deep - blocking here is what keeps memory bounded when the main
      /// thread generates faster than the database consumes
      from_worker_.wait(lock, [this] { return ! pending_.has_value(); });
      pending_ = queued_job{.fn = std::move(j), .task_id = task_id};
    }
    to_worker_.notify_one();
  }

  void async_db::run_on_worker(job j, bool even_after_error, std::optional<size_t> task_id)
  {
    /// Deliberately not "post, then wait for a done flag": post_to_worker()
    /// drops the job when an error is already pending, and nothing would ever
    /// set that flag. Waiting for the queue to be empty and the worker idle
    /// covers the job-ran and the job-was-dropped case alike.
    post_to_worker(std::move(j), even_after_error, task_id);
    drain();
  }

  bool async_db::cancel() const noexcept
  {
    std::optional<size_t> id;
    {
      const std::scoped_lock lock{mtx_};
      id = running_task_id_;
    }
    /// Deliberately read outside the lock: tasks_ itself is only ever
    /// appended to (by prepare(), on the worker) and never touched by
    /// cancel()'s own caller thread otherwise, and cancel() on the task
    /// object is safe to call concurrently with that same task's own
    /// execute()/fetch() running on the worker thread - that is the entire
    /// point of query<>::cancel() existing (see its own doc comment).
    if (! id) return false;
    return tasks_.at(*id)->cancel();
  }

  void async_db::drain()
  {
    std::unique_lock lock{mtx_};
    from_worker_.wait(lock, [this] { return ! pending_.has_value() && ! busy_; });
  }

  std::expected<exec_status, db_error> async_db::is_finished() const
  {
    const std::scoped_lock lock{mtx_};
    /// Same condition drain() waits for, just read once instead of waited on -
    /// see the class's own "Polling instead of blocking" doc comment for why
    /// this single read is enough: the queue is exactly one job deep, so
    /// "nothing pending and the worker is idle" can only mean the last job
    /// exec() handed over has actually finished.
    if (pending_.has_value() || busy_) return exec_status::still_pending;
    if (error_) return std::unexpected(*error_);
    return exec_status::finished;
  }

  void async_db::note_error(const db_error& e)
  {
    const std::scoped_lock lock{mtx_};
    if (! error_) error_ = e; ///< sticky: the first failure is the one that explains the rest
  }

  std::optional<db_error> async_db::error() const
  {
    const std::scoped_lock lock{mtx_};
    return error_;
  }

  bool async_db::has_error() const
  {
    const std::scoped_lock lock{mtx_};
    return error_.has_value();
  }

  std::expected<void, db_error> async_db::commit()
  {
    drain();

    if (auto e = error())
    {
      /// A failed statement means the transaction cannot be committed - on
      /// PostgreSQL the server refuses anyway (25P02). Roll back so the
      /// connection is usable again, and report what actually went wrong
      /// rather than the rollback's own status.
      run_on_worker([this] { (void)db_.rollback(); }, true);
      {
        const std::scoped_lock lock{mtx_};
        error_.reset();
      }
      return std::unexpected(*e);
    }

    db_sts sts = db_sts::success;
    run_on_worker([this, &sts] { sts = db_.commit(); }, true);
    if (! is_success(sts))
      return std::unexpected(db_error{.sts = sts, .message = "commit failed", .sql_state = "", .driver_status = 0, .native_error = 0});
    return {};
  }

  std::expected<void, db_error> async_db::rollback()
  {
    drain();

    db_sts sts = db_sts::success;
    run_on_worker([this, &sts] { sts = db_.rollback(); }, true);
    {
      const std::scoped_lock lock{mtx_};
      error_.reset(); ///< the transaction is gone, so the sticky error goes with it
    }
    if (! is_success(sts))
      return std::unexpected(db_error{.sts = sts, .message = "rollback failed", .sql_state = "", .driver_status = 0, .native_error = 0});
    return {};
  }

} // namespace rtl
