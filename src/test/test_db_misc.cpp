// test_db_misc.cpp
/**
 * @file
 * @brief rtl::db::as_database(), rtl::db::chk() and rtl::db::connection()
 *
 * These three carry no caller anywhere in this tree today - not the
 * generator, not any generated code, not the rest of the test suite - so
 * nothing exercised them before this file. Written to pin down their
 * contract now rather than leave them dead code with an unverified
 * behaviour, per the class comments in rtl.hpp.
 */
#include "query.hpp" // pulls in the backend's error type test_db.hpp's describe() needs
#include "rtl.hpp"
#include "test_db.hpp" // live_db, shared with the other crud tests
#include <catch2/catch_test_macros.hpp>

TEST_CASE("as_database() is a same-object upcast, never null", "[db][rtl][live-db]")
{
  live_db live;
  auto&   db = live.db;

  rtl::database& as_db = db.as_database();

  // "same object" per its own doc comment: db_db2/db_psql inherit from db,
  // database and schema::describer all at once (multiple inheritance), so
  // &as_db and &db legitimately differ - each base subobject sits at its own
  // offset within the same most-derived object. A plain pointer comparison
  // would fail here even though the doc comment's promise holds; comparing
  // through dynamic_cast<const void*> is what the standard guarantees
  // resolves to the address of the most-derived object no matter which base
  // pointer you start from, so it is the right way to ask "is this the same
  // object" across a multiple-inheritance boundary.
  CHECK(dynamic_cast<const void*>(&as_db) == dynamic_cast<const void*>(&db));

  // usable through the narrow interface a generated query<> relies on -
  // get_conn() must return the live handle, not a null/default one, once
  // live_db's constructor has already connected. 0 rather than nullptr:
  // SQLHDBC (db2) is an integer handle, not a pointer - PGconn* (psql)
  // compares against 0 just as well, since a null pointer converts to 0.
  CHECK(as_db.get_conn() != 0);
  CHECK(as_db.get_logger() != nullptr);
}

TEST_CASE("chk() reports success without logging, failure with a log line", "[db][rtl][live-db]")
{
  const live_db live; // chk() is const - nothing here mutates the connection
  const auto&   db = live.db;

  // success: true, and nothing worth asserting on the log side - chk()'s own
  // doc comment only promises a log line on failure.
  CHECK(db.chk(rtl::db_sts::success, "test context"));
  CHECK(db.chk(rtl::db_sts::success_with_info, "test context"));

  // failure: false. The log line itself is not asserted on here (this suite
  // has no log-capturing fixture), but every other live-db test already
  // exercises chk()-adjacent error paths with visible log output (see e.g.
  // parser_test.cpp's own "before sql ... failed" cases), so the format
  // string is covered; what is missing before this test is chk()'s own
  // return value ever being checked at all.
  CHECK_FALSE(db.chk(rtl::db_sts::connection_error, "test context"));
  CHECK_FALSE(db.chk(rtl::db_sts::not_implemented, "test context"));
}

TEST_CASE("connection() is empty before connect() succeeds, filled after", "[db][rtl][live-db]")
{
  // live_db's own constructor already calls connect() - to see the "before"
  // state this needs a second, not-yet-connected object of whichever backend
  // is linked into this executable, rather than live_db's fixture.
  auto db = rtl::make_db(dbgen4::test::test_logger());
  REQUIRE(db != nullptr);

  CHECK(db->connection().empty());

  // now connect it the same way live_db does, through the same environment
  // variables the rest of the crud suite reads
  const auto host = test_db::env_or("DBGEN4_TEST_HOST", "localhost");
  const auto port = static_cast<uint16_t>(std::stoi(test_db::env_or("DBGEN4_TEST_PORT", std::to_string(rtl::default_port()))));
  const auto name = test_db::env_or("DBGEN4_TEST_DB", "dbgen4");
  const auto user = test_db::env_or("DBGEN4_TEST_USER", "dbgen4");
  const auto pass = test_db::env_or("DBGEN4_TEST_PASS", "dbgen4");

  REQUIRE(rtl::is_success(db->connect(host, port, name, user, pass)));

  // exact shape per connection()'s own doc comment, not just a substring
  // check - set_connection_info() (see rtl.hpp) never even takes a password
  // parameter, so there is no "does the password leak in" case worth writing:
  // it structurally cannot. What is worth pinning down is the format itself.
  const auto expected = fmt::format("host:{} port:{} database:{} user:{}", host, port, name, user);
  CHECK(db->connection() == expected);

  db->disconnect();
}
