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
#include <string>

namespace
{
  constexpr int32_t  async_first   = 7300; ///< a key range of its own
  constexpr int16_t  async_year    = 2026;
  constexpr uint16_t async_month   = 4;
  constexpr int32_t  days_in_month = 28; ///< keeps async_date() inside February regardless of id

  rtl::date async_date(int32_t id)
  { return {.year = async_year, .month = async_month, .day = static_cast<uint16_t>(1 + ((id - async_first) % days_in_month))}; }

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
    rtl::async_db adb(db);

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
    rtl::async_db adb(db);

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
    rtl::async_db adb(db);

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
    rtl::async_db adb(db);
    auto          ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
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
    rtl::async_db adb(db);
    auto          ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
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
    rtl::async_db adb(db);
    auto          ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
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
    rtl::async_db adb(db);
    auto          ins = adb.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
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
