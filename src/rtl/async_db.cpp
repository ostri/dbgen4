// async_db.cpp
#include "async_db.hpp"
#include <mutex>
#include <utility>

namespace rtl
{
  async_db::async_db(db& database)
  : db_(database)
  {
    worker_ = std::thread([this] { worker_loop(); });
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
      job j;
      {
        std::unique_lock lock{mtx_};
        to_worker_.wait(lock, [this] { return pending_.has_value() || stop_; });
        /// stop only once the queue is empty - the destructor has already
        /// drained, so this is the shutdown-after-drain path
        if (! pending_) return;
        j     = std::move(*pending_);
        pending_.reset();
        busy_ = true;
      }
      /// run outside the lock: this is where the database round trip happens,
      /// and holding the mutex across it would serialise the very thing this
      /// class exists to overlap
      from_worker_.notify_all(); ///< the queue has a slot again

      j();

      {
        const std::scoped_lock lock{mtx_};
        busy_ = false;
      }
      from_worker_.notify_all();
    }
  }

  void async_db::post_to_worker(job j, bool even_after_error)
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
      pending_ = std::move(j);
    }
    to_worker_.notify_one();
  }

  void async_db::run_on_worker(const job& j, bool even_after_error)
  {
    /// Deliberately not "post, then wait for a done flag": post_to_worker()
    /// drops the job when an error is already pending, and nothing would ever
    /// set that flag. Waiting for the queue to be empty and the worker idle
    /// covers the job-ran and the job-was-dropped case alike.
    post_to_worker(j, even_after_error);
    drain();
  }

  void async_db::drain()
  {
    std::unique_lock lock{mtx_};
    from_worker_.wait(lock, [this] { return ! pending_.has_value() && ! busy_; });
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
      return std::unexpected(db_error{.sts           = sts,
                                      .message       = "commit failed",
                                      .sql_state     = "",
                                      .driver_status = 0,
                                      .native_error  = 0});
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
      return std::unexpected(db_error{.sts           = sts,
                                      .message       = "rollback failed",
                                      .sql_state     = "",
                                      .driver_status = 0,
                                      .native_error  = 0});
    return {};
  }

} // namespace rtl
