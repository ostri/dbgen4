// test_exec.cpp
/**
 * @file
 * @brief rtl::db::exec() against a live database
 *
 * exec() runs a bare SQL statement that has no parameters and produces no
 * result set - it is not prepared, so it is the right tool for DDL and
 * transaction control rather than for the query<> path. This test drives it
 * through the story it is meant for: create a table, put a row in it,
 * commit, drop the table, then confirm the drop actually took - a SELECT
 * against a table that no longer exists must fail.
 */
#include "query.hpp" // pulls in the backend's error type test_db.hpp's describe() needs
#include "rtl.hpp"
#include "test_db.hpp" // live_db, shared with the other crud tests
#include <catch2/catch_test_macros.hpp>

TEST_CASE("exec runs a bare statement with no parameters and no result", "[exec][rtl][live-db]")
{
  live_db live;
  auto&   db = live.db;

  // leave no trace of an earlier run that failed before reaching drop -
  // db.exec() is not itself under test in this cleanup step, so its result is
  // ignored the same way parser_test.cpp's own setup does it
  db.exec("drop table exec_no_par_test");
  db.commit();

  // ------------------------------------------------------------------
  // create a table nobody prepared a statement for
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.exec("create table exec_no_par_test (id integer)")));

  // ------------------------------------------------------------------
  // put a row in it, then make the change durable
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.exec("insert into exec_no_par_test (id) values (42)")));
  REQUIRE(rtl::is_success(db.commit()));

  // ------------------------------------------------------------------
  // drop the table - again through exec(), no prepare involved
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.exec("drop table exec_no_par_test")));
  REQUIRE(rtl::is_success(db.commit()));

  // ------------------------------------------------------------------
  // the table must be gone - selecting from it has to fail
  // ------------------------------------------------------------------
  const auto sel = db.exec("select id from exec_no_par_test");
  CHECK_FALSE(rtl::is_success(sel));
  db.rollback(); // the failed select above left the transaction aborted (psql)
}
