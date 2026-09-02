// test_on_change_psql.cpp
/**
 * @file
 * @brief rtl::db::on_change() (PostgreSQL LISTEN/NOTIFY) against a live database
 *
 * on_change() is PostgreSQL only - see rtl::db::on_change()'s own doc comment for why DB2 has no
 * equivalent at this level - so, unlike test_exec.cpp/test_refresh_statistics.cpp, this file is
 * wired only into the psql crud test target (see CMakeLists.txt's own add_crud_test_target(psql
 * ...) call), not into both backends.
 *
 * A NOTIFY is only delivered once the transaction that sent it commits - db_psql stays inside an
 * open transaction from connect() onward (see db_psql::connect()'s own "mirror the db2 backend"
 * comment), so every write below is followed by db.commit() before the handler is expected to
 * fire. Delivery itself runs on on_change()'s own worker thread (see db_psql::listen_loop()'s own
 * doc comment), never on the test's thread, so every assertion on "did the handler run" polls with
 * a bounded wait rather than checking immediately after commit().
 */
#include "query.hpp" // pulls in the backend's error type test_db.hpp's describe() needs
#include "rtl.hpp"
#include "test_db.hpp" // live_db, shared with the other crud tests
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

namespace
{
  constexpr auto default_wait_timeout = std::chrono::seconds(5); // NOLINT(*-magic-numbers)

  /// polls until pred() is true or timeout elapses - on_change()'s handler runs on its own worker
  /// thread, so a caller cannot just check state right after commit()
  template <typename Pred>
  bool wait_until(Pred pred, std::chrono::milliseconds timeout = default_wait_timeout)
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
      if (pred()) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(20)); // NOLINT(*-magic-numbers)
    }
    return pred();
  }

  /// records every change_op the handler was called with, guarded for the handler's own thread
  class recorder
  {
  public:
    void operator()(rtl::change_op op)
    {
      const std::scoped_lock lock{mtx_};
      ops_.push_back(op);
    }
    [[nodiscard]] std::vector<rtl::change_op> ops() const
    {
      const std::scoped_lock lock{mtx_};
      return ops_;
    }
    [[nodiscard]] size_t count() const
    {
      const std::scoped_lock lock{mtx_};
      return ops_.size();
    }
  private:
    mutable std::mutex          mtx_;
    std::vector<rtl::change_op> ops_;
  };
} // namespace

TEST_CASE("on_change fires with change_op::insert on a new row", "[on_change][rtl][live-db][psql]")
{
  live_db live;
  auto&   db = live.db;

  // leave no trace of an earlier run - drop function too, not just the table: create_change_trigger()
  // creates the trigger function independently of the table it is later attached to, so a table
  // drop alone (even CASCADE) does not remove it. Ignored on purpose, same as db.exec()'s other
  // cleanup calls elsewhere in this file - it is not itself under test here.
  db.exec("drop table on_change_test");
  db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()");
  db.commit();
  REQUIRE(rtl::is_success(db.exec("create table on_change_test (id integer)")));
  REQUIRE(rtl::is_success(db.commit()));

  recorder rec;
  REQUIRE(rtl::is_success(db.on_change("on_change_test", std::ref(rec))));

  REQUIRE(rtl::is_success(db.exec("insert into on_change_test (id) values (1)")));
  REQUIRE(rtl::is_success(db.commit()));

  REQUIRE(wait_until([&] { return rec.count() >= 1; }));
  const auto ops = rec.ops();
  REQUIRE(ops.size() == 1);
  CHECK(ops.front() == rtl::change_op::insert);

  REQUIRE(rtl::is_success(db.exec("drop table on_change_test")));
  REQUIRE(rtl::is_success(db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()")));
  REQUIRE(rtl::is_success(db.commit()));
}

TEST_CASE("on_change fires with change_op::update on an updated row", "[on_change][rtl][live-db][psql]")
{
  live_db live;
  auto&   db = live.db;

  // leave no trace of an earlier run - drop function too, not just the table: create_change_trigger()
  // creates the trigger function independently of the table it is later attached to, so a table
  // drop alone (even CASCADE) does not remove it. Ignored on purpose, same as db.exec()'s other
  // cleanup calls elsewhere in this file - it is not itself under test here.
  db.exec("drop table on_change_test");
  db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()");
  db.commit();
  REQUIRE(rtl::is_success(db.exec("create table on_change_test (id integer)")));
  REQUIRE(rtl::is_success(db.exec("insert into on_change_test (id) values (1)")));
  REQUIRE(rtl::is_success(db.commit()));

  recorder rec;
  REQUIRE(rtl::is_success(db.on_change("on_change_test", std::ref(rec))));

  REQUIRE(rtl::is_success(db.exec("update on_change_test set id = 2 where id = 1")));
  REQUIRE(rtl::is_success(db.commit()));

  REQUIRE(wait_until([&] { return rec.count() >= 1; }));
  const auto ops = rec.ops();
  REQUIRE(ops.size() == 1);
  CHECK(ops.front() == rtl::change_op::update);

  REQUIRE(rtl::is_success(db.exec("drop table on_change_test")));
  REQUIRE(rtl::is_success(db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()")));
  REQUIRE(rtl::is_success(db.commit()));
}

TEST_CASE("on_change fires with change_op::remove on a deleted row", "[on_change][rtl][live-db][psql]")
{
  live_db live;
  auto&   db = live.db;

  // leave no trace of an earlier run - drop function too, not just the table: create_change_trigger()
  // creates the trigger function independently of the table it is later attached to, so a table
  // drop alone (even CASCADE) does not remove it. Ignored on purpose, same as db.exec()'s other
  // cleanup calls elsewhere in this file - it is not itself under test here.
  db.exec("drop table on_change_test");
  db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()");
  db.commit();
  REQUIRE(rtl::is_success(db.exec("create table on_change_test (id integer)")));
  REQUIRE(rtl::is_success(db.exec("insert into on_change_test (id) values (1)")));
  REQUIRE(rtl::is_success(db.commit()));

  recorder rec;
  REQUIRE(rtl::is_success(db.on_change("on_change_test", std::ref(rec))));

  REQUIRE(rtl::is_success(db.exec("delete from on_change_test where id = 1")));
  REQUIRE(rtl::is_success(db.commit()));

  REQUIRE(wait_until([&] { return rec.count() >= 1; }));
  const auto ops = rec.ops();
  REQUIRE(ops.size() == 1);
  CHECK(ops.front() == rtl::change_op::remove);

  REQUIRE(rtl::is_success(db.exec("drop table on_change_test")));
  REQUIRE(rtl::is_success(db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()")));
  REQUIRE(rtl::is_success(db.commit()));
}

TEST_CASE("on_change on a table that does not exist fails, not silently ignored", "[on_change][rtl][live-db][psql]")
{
  live_db live;
  auto&   db = live.db;

  db.exec("drop table on_change_no_such_table");
  db.exec("drop function if exists dbgen4_on_change_on_change_no_such_table_fn()");
  db.commit();

  recorder   rec;
  const auto ret = db.on_change("on_change_no_such_table", std::ref(rec));
  CHECK_FALSE(rtl::is_success(ret));
  db.rollback(); // the failed CREATE TRIGGER above left the transaction aborted (psql)

  // create_change_trigger() creates the function before attempting the trigger, so that first half
  // did succeed even though on_change() as a whole reported failure - clean it up rather than
  // leaving it behind for the next run to silently reuse.
  REQUIRE(rtl::is_success(db.exec("drop function if exists dbgen4_on_change_on_change_no_such_table_fn()")));
  REQUIRE(rtl::is_success(db.commit()));
}

TEST_CASE("on_change registered twice for the same table replaces the handler, not adds a second one", "[on_change][rtl][live-db][psql]")
{
  live_db live;
  auto&   db = live.db;

  // leave no trace of an earlier run - drop function too, not just the table: create_change_trigger()
  // creates the trigger function independently of the table it is later attached to, so a table
  // drop alone (even CASCADE) does not remove it. Ignored on purpose, same as db.exec()'s other
  // cleanup calls elsewhere in this file - it is not itself under test here.
  db.exec("drop table on_change_test");
  db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()");
  db.commit();
  REQUIRE(rtl::is_success(db.exec("create table on_change_test (id integer)")));
  REQUIRE(rtl::is_success(db.commit()));

  recorder first;
  recorder second;
  REQUIRE(rtl::is_success(db.on_change("on_change_test", std::ref(first))));
  REQUIRE(rtl::is_success(db.on_change("on_change_test", std::ref(second)))); // replaces `first`'s registration

  REQUIRE(rtl::is_success(db.exec("insert into on_change_test (id) values (1)")));
  REQUIRE(rtl::is_success(db.commit()));

  REQUIRE(wait_until([&] { return second.count() >= 1; }));
  // give `first` every chance to have (wrongly) fired too, before asserting it never did
  std::this_thread::sleep_for(std::chrono::milliseconds(200)); // NOLINT(*-magic-numbers)
  CHECK(first.count() == 0);
  CHECK(second.count() == 1);

  REQUIRE(rtl::is_success(db.exec("drop table on_change_test")));
  REQUIRE(rtl::is_success(db.exec("drop function if exists dbgen4_on_change_on_change_test_fn()")));
  REQUIRE(rtl::is_success(db.commit()));
}

TEST_CASE("on_change on a disconnected database reports an error", "[on_change][rtl][live-db][psql]")
{
  live_db live;
  live.db.disconnect();

  recorder   rec;
  const auto ret = live.db.on_change("on_change_test", std::ref(rec));
  CHECK_FALSE(rtl::is_success(ret));
}
