// test_refresh_statistics.cpp
/**
 * @file
 * @brief rtl::db::refresh_statistics() against a live database
 *
 * refresh_statistics() gives every backend the same one-call contract for
 * "refresh the planner's own statistics for this table" (VACUUM ANALYZE on
 * psql, RUNSTATS on db2 - see rtl::db::refresh_statistics()'s own doc
 * comment), each backend picking its own correct SQL/API sequence and
 * transaction handling. This test drives it through the story it exists
 * for: create a table, put rows in it, call refresh_statistics(), then
 * confirm the connection is still usable for ordinary statements afterwards
 * - the one behavior a caller cannot verify from db_sts::success alone is
 * whether refresh_statistics() left the connection's own transaction state
 * intact for whatever runs next.
 */
#include "query.hpp" // pulls in the backend's error type test_db.hpp's describe() needs
#include "rtl.hpp"
#include "test_db.hpp" // live_db, shared with the other crud tests
#include <catch2/catch_test_macros.hpp>

TEST_CASE("refresh_statistics leaves the connection usable for the next statement", "[refresh_statistics][rtl][live-db]")
{
  live_db live;
  auto&   db = live.db;

  // leave no trace of an earlier run that failed before reaching drop - db.exec() is not itself
  // under test in this cleanup step, so its result is ignored the same way test_exec.cpp's own
  // setup does it
  db.exec("drop table refresh_stats_test");
  db.commit();

  // ------------------------------------------------------------------
  // create a table with a few rows for the planner to have something to say about
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.exec("create table refresh_stats_test (id integer)")));
  REQUIRE(rtl::is_success(db.exec("insert into refresh_stats_test (id) values (1)")));
  REQUIRE(rtl::is_success(db.exec("insert into refresh_stats_test (id) values (2)")));
  REQUIRE(rtl::is_success(db.commit()));

  // ------------------------------------------------------------------
  // the call under test
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.refresh_statistics("refresh_stats_test")));

  // ------------------------------------------------------------------
  // the connection must still be usable afterwards - not left mid-transaction, not left
  // disconnected. A plain exec() right after is the same shape of check test_exec.cpp's own
  // "the table must be gone" assertion is: the thing under test does not report its own success
  // by db_sts alone, only by what still works right after it.
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.exec("insert into refresh_stats_test (id) values (3)")));
  REQUIRE(rtl::is_success(db.commit()));

  // ------------------------------------------------------------------
  // clean up
  // ------------------------------------------------------------------
  REQUIRE(rtl::is_success(db.exec("drop table refresh_stats_test")));
  REQUIRE(rtl::is_success(db.commit()));
}
