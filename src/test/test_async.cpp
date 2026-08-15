// test_async.cpp
/**
 * @file
 * @brief the async facade, against a live database on both backends
 *
 * Written against the behaviour the facade promises rather than against its
 * machinery, so the same source runs on db2 and psql: a sequence of different
 * statements lands in one transaction, a failure anywhere in that sequence is
 * remembered and reported by commit(), and nothing from a failed transaction
 * survives.
 *
 * The one place the two backends visibly differ is spelled out in its own
 * case: psql aborts the whole transaction after a failed statement (25P02),
 * db2 does not, but the facade's sticky error makes the *observable* outcome
 * the same either way - which is the point of having it.
 */
#include "async_db.hpp"
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "query.hpp"
#include "test_db.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <fmt/format.h>
#include <memory>
#include <string>

namespace
{
  constexpr int32_t  async_first   = 7300; ///< a key range of its own
  constexpr int16_t  async_year    = 2026;
  constexpr uint16_t async_month   = 4;
  constexpr int32_t  days_in_month = 28; ///< keeps async_date() inside February regardless of id

  rtl::date async_date(int32_t id)
  { return {.year = async_year, .month = async_month, .day = static_cast<uint16_t>(1 + ((id - async_first) % days_in_month))}; }

  /// create()'s own std::expected, unwrapped and REQUIRE-d in one call -
  /// every case below needs this, and folding it into the case itself is
  /// most of what pushes several of them over clang-tidy's cognitive
  /// complexity threshold for no benefit (the unwrap-or-fail shape never
  /// varies between cases).
  template <typename Db>
  std::unique_ptr<rtl::async_db> open_async_db(Db& db)
  {
    auto adb_h = rtl::async_db::create(db);
    REQUIRE(adb_h.has_value());
    return std::move(adb_h.value());
  }

  /// remove a key range synchronously, so a case starts from a known state
  template <typename Db>
  void clear_async_range(Db& db, int32_t first, int32_t last)
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del async range)");
    for (int32_t id = first; id <= last; ++id)
    {
      del.get_param()->set_id(id);
      require_ok(del.execute(), "execute(del async range)");
    }
    REQUIRE(rtl::is_success(db.commit()));
  }

  /// submits one prepared statement for every id in [first, last], setting
  /// the same three fields "name" is derived from - the only thing that
  /// differs between an insert pass and an update pass is which prepared
  /// statement and which "name" prefix gets used
  template <typename Stmt>
  void submit_range(rtl::async_db& adb, Stmt& stmt, int32_t first, int32_t last, const char* name_prefix)
  {
    for (int32_t id = first; id <= last; ++id)
    {
      stmt.param()->set_id(id);
      stmt.param()->set_name(fmt::format("{} {}", name_prefix, id));
      stmt.param()->set_created(async_date(id));
      adb.submit(stmt);
    }
  }

  /// drains fetch_more() on a select statement until it reports "no more
  /// rows", REQUIRE-ing every step along the way succeeded - factored out
  /// of its one caller so that caller reads as "run this select and count
  /// what came back", not as the loop itself
  template <typename Stmt>
  size_t drain(rtl::async_db& adb, Stmt& sel, std::expected<bool, rtl::db_error> first_result)
  {
    size_t rows_seen = 0;
    auto   got       = first_result;
    REQUIRE(got.has_value());
    while (*got)
    {
      rows_seen += sel.result()->occupied();
      got = adb.fetch_more(sel);
      REQUIRE(got.has_value());
    }
    return rows_seen;
  }

  /// @return how many rows of a key range the table holds, read synchronously
  template <typename Db>
  size_t count_async_range(Db& db, int32_t first, int32_t last)
  {
    constexpr size_t fetch_buffer_size = 8;

    dbx::crud::s_sel_range::stmt sel(&db, dbx::crud::s_sel_range::qry::sql());
    sel.get_result_buffer()->set_buffer_size(fetch_buffer_size);
    require_ok(sel.prepare(), "prepare(count async range)");
    sel.get_param()->set_id_from(first);
    sel.get_param()->set_id_to(last);
    require_ok(sel.execute(), "execute(count async range)");

    size_t rows = 0;
    for (auto got = sel.fetch(); got && *got; got = sel.fetch()) rows += sel.get_result()->occupied();
    return rows;
  }

  /// is_finished() is a poll, not a wait - loops until the worker actually
  /// catches up with the last exec()'d job, same condition drain() itself
  /// waits for internally. Every exec()/is_finished() case below needs this
  /// at least once; factoring it out is also what keeps the magic number of
  /// spins and the loop itself from repeating three times over.
  /// @return exec_status::finished once observed, the sticky error if
  ///         is_finished() reported one, or exec_status::still_pending if
  ///         max_spins ran out without either - a test failure in itself,
  ///         since the whole point is to observe one of the other two.
  std::expected<rtl::exec_status, rtl::db_error> poll_until_finished(rtl::async_db& adb)
  {
    constexpr int max_spins = 100000; ///< generous upper bound - real runs finish in a handful of spins
    for (int spins = 0; spins < max_spins; ++spins)
    {
      const auto st = adb.is_finished();
      if (! st) return std::unexpected(st.error());
      if (*st == rtl::exec_status::finished) return rtl::exec_status::finished;
    }
    return rtl::exec_status::still_pending;
  }

  /// like submit_range(), but through exec() instead of submit() -
  /// REQUIRE-ing every call was actually accepted into the queue, since
  /// exec() (unlike submit()) has something to check there
  template <typename Stmt>
  void exec_range(rtl::async_db& adb, Stmt& stmt, int32_t first, int32_t last, const char* name_prefix)
  {
    for (int32_t id = first; id <= last; ++id)
    {
      stmt.param()->set_id(id);
      stmt.param()->set_name(fmt::format("{} {}", name_prefix, id));
      stmt.param()->set_created(async_date(id));
      const auto st = adb.exec(stmt);
      REQUIRE(st.has_value());
    }
  }

  /// exec()'s the same row twice - the shape "the-first-collides-with-itself"
  /// (writing the same key twice) case below needs, factored out so the
  /// TEST_CASE itself does not carry the loop and its two REQUIREs directly
  template <typename Stmt>
  void exec_duplicate_twice(rtl::async_db& adb, Stmt& stmt, int32_t key, rtl::date created, const char* name)
  {
    for (int i = 0; i < 2; ++i)
    {
      stmt.param()->set_id(key);
      stmt.param()->set_name(name);
      stmt.param()->set_created(created);
      /// exec() itself still reports "finished" (accepted into the queue) -
      /// the failure is the JOB's outcome, discovered only once the worker
      /// has actually run it, which is exactly what is_finished() is for
      REQUIRE(adb.exec(stmt).has_value());
    }
  }

  /// drains fetch_more() the same way drain() does, but starting from
  /// occupied() rather than from a first_result already in hand - the shape
  /// the exec()/is_finished() row-returning case below needs, once
  /// is_finished() (not execute_sync()) is what said the first buffer is
  /// ready
  template <typename Stmt>
  size_t fetch_remaining(rtl::async_db& adb, Stmt& sel)
  {
    size_t rows_seen = sel.result()->occupied();
    for (auto got = adb.fetch_more(sel); got && *got; got = adb.fetch_more(sel)) rows_seen += sel.result()->occupied();
    return rows_seen;
  }

  /// prepares s_ins/s_sel_range, submits the whole insert range through
  /// submit(), and readies sel's own parameters - the common setup the
  /// "exec on a row-returning statement" case below needs before it can get
  /// to what it is actually testing (exec() on the select itself)
  auto prepare_and_submit_range(rtl::async_db& adb, int32_t first, int32_t last)
  {
    auto ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    /// a result buffer smaller than the range, so fetch_more() has to loop -
    /// same reasoning as the execute_sync() case above
    auto sel = adb.prepare<dbx::crud::s_sel_range::p, dbx::crud::s_sel_range::r>(dbx::crud::s_sel_range::qry::sql(), 1, 2);
    REQUIRE(ins.has_value());
    REQUIRE(sel.has_value());

    submit_range(adb, *ins, first, last, "exec-sel");
    sel->param()->set_id_from(first);
    sel->param()->set_id_to(last);
    return sel;
  }
} // namespace

TEST_CASE("a sequence of statements lands in one transaction", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first;
  constexpr int32_t rows  = 6;
  constexpr int32_t last  = first + rows - 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;

    auto ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    /// the point of the whole exercise: fill in the next row while the
    /// previous one is still in flight
    for (int32_t id = first; id <= last; ++id)
    {
      ins->param()->set_id(id);
      ins->param()->set_name(fmt::format("async {}", id));
      ins->param()->set_created(async_date(id));
      adb.submit(*ins);
    }

    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));
  }

  CHECK(count_async_range(db, first, last) == static_cast<size_t>(rows));
  clear_async_range(db, first, last);
}

TEST_CASE("different statements share one transaction and one worker", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 20;
  constexpr int32_t last  = first + 3;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;

    /// two different statement types - different parameter buffers, different
    /// SQL - registered on the same facade, which is what it exists for
    auto ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    auto upd = adb.prepare<dbx::crud::s_upd::p, rtl::no_results>(dbx::crud::s_upd::qry::sql());
    REQUIRE(ins.has_value());
    REQUIRE(upd.has_value());

    /// submits one prepared statement for every id in [first, last], setting
    /// the same three fields "name" is derived from - the only thing that
    /// differs between the insert pass below and the update pass is which
    /// prepared statement and which "name" prefix gets used
    submit_range(adb, *ins, first, last, "before");
    submit_range(adb, *upd, first, last, "after");

    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));
  }

  /// the updates ran after the inserts - ordering is total through one worker
  dbx::crud::s_sel::stmt sel(&db, dbx::crud::s_sel::qry::sql());
  require_ok(sel.prepare(), "prepare(verify update order)");
  sel.get_param()->set_id(first);
  require_ok(sel.execute(), "execute(verify update order)");
  const auto got = sel.fetch();
  REQUIRE(got.has_value());
  REQUIRE(*got);
  CHECK(std::string(sel.get_result()->name()) == fmt::format("after {}", first));

  clear_async_range(db, first, last);
}

TEST_CASE("a select runs through the facade and sees the whole transaction", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 40;
  constexpr int32_t rows  = 5;
  constexpr int32_t last  = first + rows - 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;

    auto ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    /// a result buffer smaller than the range, so fetch_more() has to loop.
    /// The size is an argument because it has to be applied before prepare().
    auto sel = adb.prepare<dbx::crud::s_sel_range::p, dbx::crud::s_sel_range::r>(dbx::crud::s_sel_range::qry::sql(), 1, 2);
    REQUIRE(ins.has_value());
    REQUIRE(sel.has_value());

    submit_range(adb, *ins, first, last, "sel");

    /// the select drains the submitted inserts first, so it sees all of them
    /// even though they were never waited for individually
    sel->param()->set_id_from(first);
    sel->param()->set_id_to(last);

    const size_t seen = drain(adb, *sel, adb.execute_sync(*sel));
    CHECK(seen == static_cast<size_t>(rows));

    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));
  }

  clear_async_range(db, first, last);
}

TEST_CASE("the first error is remembered and reported by commit", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 60;
  constexpr int32_t rows  = 6;
  constexpr int32_t last  = first + rows - 1;
  /// the row that will collide, and the one it collides with
  constexpr int32_t duplicate_of = first + 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;
    auto  ins     = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    for (int32_t id = first; id <= last; ++id)
    {
      /// halfway through, insert a key that is already there
      const int32_t key = (id == first + 3) ? duplicate_of : id;
      ins->param()->set_id(key);
      ins->param()->set_name(fmt::format("dup {}", id));
      ins->param()->set_created(async_date(id));
      adb.submit(*ins);
    }

    /// nothing was waited for, so the failure surfaces here
    const auto committed = adb.commit();
    REQUIRE_FALSE(committed.has_value());
    CHECK(committed.error().sts == rtl::db_sts::duplicate_key);
    CHECK(committed.error().sql_state == "23505");
  }

  /// commit() rolled back rather than committing, so nothing from the
  /// sequence survived - not even the rows that went in before the collision
  CHECK(count_async_range(db, first, last) == 0);

  clear_async_range(db, first, last);
}

TEST_CASE("statements after a failure are discarded, not run", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 80;
  constexpr int32_t last  = first + 3;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;
    auto  ins     = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    /// the same key twice, so the second submit is the one that fails
    for (int i = 0; i < 2; ++i)
    {
      ins->param()->set_id(first);
      ins->param()->set_name("collide");
      ins->param()->set_created(async_date(first));
      adb.submit(*ins);
    }
    adb.drain();
    CHECK(adb.has_error());

    /// everything from here on is dropped without reaching the database,
    /// which on psql could not have run anyway (25P02)
    for (int32_t id = first + 1; id <= last; ++id)
    {
      ins->param()->set_id(id);
      ins->param()->set_name(fmt::format("never {}", id));
      ins->param()->set_created(async_date(id));
      adb.submit(*ins);
    }

    const auto committed = adb.commit();
    REQUIRE_FALSE(committed.has_value());
    /// the error reported is the first one, not one from the discarded tail
    CHECK(committed.error().sts == rtl::db_sts::duplicate_key);
  }

  CHECK(count_async_range(db, first, last) == 0);
  clear_async_range(db, first, last);
}

TEST_CASE("rollback clears the error and leaves the facade usable", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 100;
  constexpr int32_t last  = first + 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;
    auto  ins     = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    for (int i = 0; i < 2; ++i)
    {
      ins->param()->set_id(first);
      ins->param()->set_name("collide");
      ins->param()->set_created(async_date(first));
      adb.submit(*ins);
    }
    adb.drain();
    REQUIRE(adb.has_error());

    REQUIRE(adb.rollback().has_value());
    CHECK_FALSE(adb.has_error()); ///< the transaction is gone, and so is its error

    /// the same facade now works again, on a fresh transaction
    ins->param()->set_id(last);
    ins->param()->set_name("after rollback");
    ins->param()->set_created(async_date(last));
    adb.submit(*ins);

    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));
  }

  CHECK(count_async_range(db, first, last) == 1);
  clear_async_range(db, first, last);
}

TEST_CASE("the destructor drains what was submitted without committing it", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 120;
  constexpr int32_t last  = first + 2;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;
    auto  ins     = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    for (int32_t id = first; id <= last; ++id)
    {
      ins->param()->set_id(id);
      ins->param()->set_name(fmt::format("no commit {}", id));
      ins->param()->set_created(async_date(id));
      adb.submit(*ins);
    }
    /// no commit() - the destructor drains the queue and stops the worker,
    /// but deliberately does not commit
  }

  /// so the rows are still uncommitted; rolling back must lose them, which is
  /// what a forgotten commit() is supposed to look like
  REQUIRE(rtl::is_success(db.rollback()));
  CHECK(count_async_range(db, first, last) == 0);

  clear_async_range(db, first, last);
}

// ====================================================================
// exec()/is_finished() - the polling pair, see async_db's own "Polling
// instead of blocking" doc comment for the model these implement.
// ====================================================================

TEST_CASE("is_finished reports finished before anything was ever exec'd", "[crud][generated][live-db][async]")
{
  live_db live;
  auto&   db    = live.db;
  auto    adb_h = rtl::async_db::create(db);
  REQUIRE(adb_h.has_value());
  auto& adb = *adb_h.value();

  /// nothing has been prepared or exec'd yet - the queue was never
  /// anything but empty, so this is "finished" by the same definition
  /// exec()'s own doc comment gives: nothing pending, worker not busy
  const auto st = adb.is_finished();
  REQUIRE(st.has_value());
  CHECK(*st == rtl::exec_status::finished);
}

TEST_CASE("exec accepts a no_results statement and is_finished catches up", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 140;
  constexpr int32_t rows  = 6;
  constexpr int32_t last  = first + rows - 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;

    auto ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    /// exec() only blocks if the one-deep queue is still occupied - with six
    /// small inserts and no other work, each call should virtually always
    /// return immediately with "finished" (accepted into the queue)
    exec_range(adb, *ins, first, last, "exec");

    const auto last_status = poll_until_finished(adb);
    REQUIRE(last_status.has_value());
    CHECK(*last_status == rtl::exec_status::finished);

    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));
  }

  CHECK(count_async_range(db, first, last) == static_cast<size_t>(rows));
  clear_async_range(db, first, last);
}

TEST_CASE("exec on a row-returning statement is fetched once is_finished says finished", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 160;
  constexpr int32_t rows  = 5;
  constexpr int32_t last  = first + rows - 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;
    auto  sel     = prepare_and_submit_range(adb, first, last);

    /// exec() on a row-returning handle: it does not wait for the select to
    /// actually run - only for the queue to have room, which the six queued
    /// inserts above may still occupy
    const auto submitted = adb.exec(*sel);
    REQUIRE(submitted.has_value());
    CHECK(*submitted == rtl::exec_status::finished);

    /// wait out the actual execute()+first fetch() the same way the
    /// no_results test above does
    const auto last_status = poll_until_finished(adb);
    REQUIRE(last_status.has_value());
    REQUIRE(*last_status == rtl::exec_status::finished);

    /// is_finished() said finished - sel is the handle we last exec()'d, and
    /// its own type (a real result buffer, not rtl::no_results) is what told
    /// us fetch()/fetch_more() apply here, not anything is_finished() itself
    /// reported (see exec()'s own doc comment)
    CHECK(fetch_remaining(adb, *sel) == static_cast<size_t>(rows));

    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("commit failed: {}", committed.error().str()));
  }

  clear_async_range(db, first, last);
}

TEST_CASE("is_finished reports the sticky error once the worker catches up", "[crud][generated][live-db][async]")
{
  live_db           live;
  auto&             db    = live.db;
  constexpr int32_t first = async_first + 180;
  constexpr int32_t last  = first + 1;

  clear_async_range(db, first, last);

  {
    auto  adb_ptr = open_async_db(db);
    auto& adb     = *adb_ptr;
    auto  ins     = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
    REQUIRE(ins.has_value());

    /// the same key twice, so the second exec() is the one that fails
    exec_duplicate_twice(adb, *ins, first, async_date(first), "exec collide");

    const auto last_status = poll_until_finished(adb);
    REQUIRE_FALSE(last_status.has_value()); // the duplicate key is expected to surface as an error here
    CHECK(last_status.error().sts == rtl::db_sts::duplicate_key);
    CHECK(last_status.error().sql_state == "23505");

    /// commit() reports the exact same sticky error - is_finished() did not
    /// consume or clear it, only observed it
    const auto committed = adb.commit();
    REQUIRE_FALSE(committed.has_value());
    CHECK(committed.error().sts == rtl::db_sts::duplicate_key);
  }

  CHECK(count_async_range(db, first, last) == 0);
  clear_async_range(db, first, last);
}
