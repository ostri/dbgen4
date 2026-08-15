// test_cancel.cpp
/**
 * @file
 * @brief async_db::cancel() against a live database on both backends
 *
 * Shared between db2 and psql, like test_async.cpp: cancel()'s own contract
 * ("abort whatever the worker is currently running, from another thread") is
 * backend neutral even though its implementation is not (PQcancelBlocking on
 * a connection vs. SQLCancel on a statement handle - see query<>::cancel()
 * on each side). crud.yaml's own s_slow_count statement is what gives these
 * cases something genuinely still running to cancel: a recursive CTE tuned
 * per backend (see the yaml file's own comment on s_slow_count) to take a
 * few seconds either way, long enough for is_finished() to reliably observe
 * still_pending before cancel() is called.
 */
#include "async_db.hpp"
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "test_db.hpp" // live_db, shared with the other crud tests
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fmt/format.h>
#include <thread>

namespace
{
  constexpr auto      worker_head_start = std::chrono::milliseconds(100); ///< time given the worker to actually start s_slow_count
  constexpr auto      poll_interval     = std::chrono::milliseconds(20);  ///< sleep between is_finished() polls
  constexpr auto      poll_timeout      = std::chrono::seconds(10);       ///< how long a poll loop below waits before giving up
  constexpr int32_t   cancel_test_key   = 9990; ///< key used by the "completes before cancel" case, own range from every other test file
  constexpr rtl::date cancel_test_date{.year = 2026, .month = 1, .day = 1};

  /// Polls is_finished() until it stops reporting still_pending (finished or
  /// an error), sleeping briefly between polls so the loop does not spin the
  /// CPU - factored out since every case below needs this at least once.
  rtl::exec_status poll_until_done(rtl::async_db& adb, std::chrono::milliseconds timeout)
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
      const auto st = adb.is_finished();
      if (! st) return rtl::exec_status::finished; // caller distinguishes via error()/has_error()
      if (*st == rtl::exec_status::finished) return rtl::exec_status::finished;
      std::this_thread::sleep_for(poll_interval);
    }
    return rtl::exec_status::still_pending; // timed out - the test itself failed to observe completion
  }

  /// deletes cancel_test_key synchronously, so a case starts from (and ends
  /// in) a known state - own function since the third case below needs it
  /// both before and after. Templated the same way test_async.cpp's own
  /// clear_async_range() is: Db is whichever backend's live_db::db concrete
  /// type this translation unit was compiled against.
  template <typename Db>
  void clear_cancel_test_key(Db& db)
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del cancel_test_key)");
    del.get_param()->set_id(cancel_test_key);
    require_ok(del.execute(), "execute(del cancel_test_key)");
    REQUIRE(rtl::is_success(db.commit()));
  }
} // namespace

TEST_CASE("cancel aborts a statement that is genuinely still running", "[crud][generated][live-db][async][cancel]")
{
  live_db live;
  auto&   db = live.db;

  auto adb_h = rtl::async_db::create(db);
  REQUIRE(adb_h.has_value());
  auto& adb = *adb_h.value();

  auto slow = adb.prepare<rtl::no_params, dbx::crud::s_slow_count::r>(dbx::crud::s_slow_count::qry::sql());
  REQUIRE(slow.has_value());

  const auto submitted = adb.exec(*slow);
  REQUIRE(submitted.has_value());
  CHECK(*submitted == rtl::exec_status::finished); // "finished" here means "accepted", not "the select is done"

  // Give the worker a moment to actually start executing before cancelling -
  // cancel() on a statement that has not even reached the server yet is not
  // what this case is testing (that race is covered by the "cancel with
  // nothing running" case below).
  std::this_thread::sleep_for(worker_head_start);

  const auto first_poll = adb.is_finished();
  REQUIRE(first_poll.has_value());
  // If this fires, s_slow_count finished faster than worker_head_start
  // budgeted for - see the yaml file's own row counts if this needs
  // retuning for a faster server.
  INFO("expected still_pending after worker_head_start - s_slow_count may need a larger row count for this server");
  CHECK(*first_poll == rtl::exec_status::still_pending);

  const bool canceled = adb.cancel();
  CHECK(canceled); // a statement was genuinely running, so a cancel request should have gone out

  // Whichever thread is waiting on the cancelled statement gets an error
  // back instead of "finished" - poll_until_done() itself distinguishes an
  // error is_finished() report from a clean finish via has_error() below.
  poll_until_done(adb, poll_timeout);
  CHECK(adb.has_error());

  const auto committed = adb.commit();
  REQUIRE_FALSE(committed.has_value()); // the cancelled statement counts as a failure like any other
}

TEST_CASE("cancel with nothing running is a harmless no-op", "[crud][generated][live-db][async][cancel]")
{
  live_db live;
  auto&   db = live.db;

  auto adb_h = rtl::async_db::create(db);
  REQUIRE(adb_h.has_value());
  auto& adb = *adb_h.value();

  // Nothing has ever been exec()'d - there is no running_task_id_ for
  // cancel() to reach for.
  CHECK_FALSE(adb.cancel());

  const auto st = adb.is_finished();
  REQUIRE(st.has_value());
  CHECK(*st == rtl::exec_status::finished);
}

TEST_CASE("a statement that completes before cancel is called commits normally", "[crud][generated][live-db][async][cancel]")
{
  live_db live;
  auto&   db = live.db;

  clear_cancel_test_key(db);

  auto adb_h = rtl::async_db::create(db);
  REQUIRE(adb_h.has_value());
  auto& adb = *adb_h.value();

  auto ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
  REQUIRE(ins.has_value());

  ins->param()->set_id(cancel_test_key);
  ins->param()->set_name("cancel race");
  ins->param()->set_created(cancel_test_date);
  const auto submitted = adb.exec(*ins);
  REQUIRE(submitted.has_value());

  poll_until_done(adb, poll_timeout);
  REQUIRE_FALSE(adb.has_error());

  // The job already finished by the time cancel() runs - nothing was
  // running, so this is the same harmless no-op as the case above, just
  // reached after real work instead of none.
  CHECK_FALSE(adb.cancel());

  const auto committed = adb.commit();
  if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));

  clear_cancel_test_key(db);
}
