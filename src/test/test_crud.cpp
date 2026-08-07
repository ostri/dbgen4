// test_crud.cpp
/**
 * @file
 * @brief round trip against a live database through the generated code
 *
 * Insert a row, read it back and compare, update it, read it back and compare
 * again, delete it and confirm it is gone. This is the first test that runs
 * the generated buffers through rtl::query rather than only compiling them,
 * so it is the first thing that can catch a wrong binding.
 *
 * Covers one column per storage category: integer (atomic), varchar
 * (c_string) and date (structure).
 *
 * The six steps are one Catch2 test case rather than one per step, and
 * deliberately not SECTIONs: a round trip is ordered, and Catch2 re-runs the
 * whole case body once per section, which would restart the story from the
 * insert every time.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "test_db.hpp" // live_db and require_ok, shared with the other crud tests
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

namespace
{
  constexpr int32_t   test_id     = 4242;
  constexpr rtl::date first_date  = {.year = 2026, .month = 1, .day = 15};
  constexpr rtl::date second_date = {.year = 2026, .month = 7, .day = 29};
  constexpr auto      first_name  = "first value";
  constexpr auto      second_name = "second value";

} // namespace

TEST_CASE("crud round trip through the generated buffers", "[crud][generated][live-db]")
{
  live_db live;
  auto&   db = live.db;

  // ------------------------------------------------------------------
  // leave no trace of an earlier run
  // ------------------------------------------------------------------
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del)");
    del.get_param()->set_id(test_id);
    require_ok(del.execute(), "execute(del)");
  }

  // ------------------------------------------------------------------
  // insert
  // ------------------------------------------------------------------
  {
    dbx::crud::s_ins::stmt ins(&db, dbx::crud::s_ins::qry::sql());
    require_ok(ins.prepare(), "prepare(ins)");
    auto par = ins.get_param();
    par->set_id(test_id);
    par->set_name(first_name);
    par->set_created(first_date);
    require_ok(ins.execute(), "execute(ins)");
  }

  // ------------------------------------------------------------------
  // read back and compare against what went in
  // ------------------------------------------------------------------
  {
    dbx::crud::s_sel::stmt sel(&db, dbx::crud::s_sel::qry::sql());
    require_ok(sel.prepare(), "prepare(sel)");
    sel.get_param()->set_id(test_id);
    require_ok(sel.execute(), "execute(sel)");

    const auto got = sel.fetch();
    require_ok(got, "fetch(sel)");
    REQUIRE(*got); // one row comes back

    auto row = sel.get_result();
    CHECK(row->id() == test_id);
    CHECK(row->name() == first_name);
    CHECK(row->created() == first_date);
  }

  // ------------------------------------------------------------------
  // update
  // ------------------------------------------------------------------
  {
    dbx::crud::s_upd::stmt upd(&db, dbx::crud::s_upd::qry::sql());
    require_ok(upd.prepare(), "prepare(upd)");
    auto par = upd.get_param();
    par->set_name(second_name);
    par->set_created(second_date);
    par->set_id(test_id);
    require_ok(upd.execute(), "execute(upd)");
  }

  // ------------------------------------------------------------------
  // read back again - the new values must have reached the database
  // ------------------------------------------------------------------
  {
    dbx::crud::s_sel::stmt sel(&db, dbx::crud::s_sel::qry::sql());
    require_ok(sel.prepare(), "prepare(sel)");
    sel.get_param()->set_id(test_id);
    require_ok(sel.execute(), "execute(sel)");

    const auto got = sel.fetch();
    require_ok(got, "fetch(sel)");
    REQUIRE(*got); // the row is still there

    auto row = sel.get_result();
    CHECK(row->id() == test_id);
    CHECK(row->name() == second_name);
    CHECK(row->created() == second_date);
  }

  // ------------------------------------------------------------------
  // delete, then prove it is gone
  // ------------------------------------------------------------------
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del)");
    del.get_param()->set_id(test_id);
    require_ok(del.execute(), "execute(del)");
  }
  {
    dbx::crud::s_sel::stmt sel(&db, dbx::crud::s_sel::qry::sql());
    require_ok(sel.prepare(), "prepare(sel)");
    sel.get_param()->set_id(test_id);
    require_ok(sel.execute(), "execute(sel)");

    const auto got = sel.fetch();
    require_ok(got, "fetch(sel)");
    CHECK_FALSE(*got); // no row comes back after the delete
  }
}
