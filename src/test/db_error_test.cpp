// db_error_test.cpp
/**
 * @file
 * @brief the backend neutral error - classification and the copy into it
 *
 * Two things are worth pinning down here, and neither needs a database:
 *
 * sqlstate_to_db_sts() is the one place where a failure gets classified, and
 * it is shared by both backends. SQLSTATE is the vocabulary they genuinely
 * have in common - the same five character codes for the same conditions -
 * which is why classification hangs off it rather than off SQLRETURN or
 * ExecStatusType, whose values do not correspond at all.
 *
 * to_db_error() then copies a backend's own error into that neutral shape.
 * The test is written against whichever backend this binary was linked with,
 * because only one of them exists in any given build - the same reason
 * make_db() works the way it does.
 */
#include "db_error.hpp"
#include "rtl.hpp"
#include "test_logger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <expected>
#include <string>

#ifdef DBGEN4_HAS_DB2
#  include "odbc_error.hpp"
#elifdef DBGEN4_HAS_PSQL
#  include "query.hpp"
#endif

TEST_CASE("sqlstate maps to the status callers act on", "[unit][db_error]")
{
  SECTION("the conditions worth telling apart keep their own status")
  {
    CHECK(rtl::sqlstate_to_db_sts("23505") == rtl::db_sts::duplicate_key);
    CHECK(rtl::sqlstate_to_db_sts("40001") == rtl::db_sts::serialization_failure);
    CHECK(rtl::sqlstate_to_db_sts("40P01") == rtl::db_sts::deadlock);
    CHECK(rtl::sqlstate_to_db_sts("22001") == rtl::db_sts::data_truncated);
    CHECK(rtl::sqlstate_to_db_sts("57014") == rtl::db_sts::timeout);
    CHECK(rtl::sqlstate_to_db_sts("28P01") == rtl::db_sts::access_denied);
  }

  SECTION("a specific code wins over the default for its class")
  {
    /// 23505 and 23503 share class 23; only the first is called out by name
    CHECK(rtl::sqlstate_to_db_sts("23505") == rtl::db_sts::duplicate_key);
    CHECK(rtl::sqlstate_to_db_sts("23503") == rtl::db_sts::constraint_violation);
  }

  SECTION("unnamed codes fall back to the class")
  {
    CHECK(rtl::sqlstate_to_db_sts("08006") == rtl::db_sts::connection_lost);
    CHECK(rtl::sqlstate_to_db_sts("42601") == rtl::db_sts::invalid_sql);
    CHECK(rtl::sqlstate_to_db_sts("24000") == rtl::db_sts::invalid_cursor);
    CHECK(rtl::sqlstate_to_db_sts("0A000") == rtl::db_sts::not_implemented);
  }

  SECTION("25P02 - the aborted transaction psql leaves behind - is a transaction error")
  {
    /// the async facade has to recognise this one: after it, every further
    /// statement in the transaction is refused until a rollback
    CHECK(rtl::sqlstate_to_db_sts("25P02") == rtl::db_sts::transaction_error);
  }

  SECTION("success and warning classes are not errors")
  {
    CHECK(rtl::sqlstate_to_db_sts("00000") == rtl::db_sts::success);
    CHECK(rtl::sqlstate_to_db_sts("01004") == rtl::db_sts::success_with_info);
    CHECK(rtl::sqlstate_to_db_sts("02000") == rtl::db_sts::no_data);
  }

  SECTION("an unknown state is still an error, just an unclassified one")
  {
    CHECK(rtl::sqlstate_to_db_sts("ZZ999") == rtl::db_sts::error);
    /// a client side fault has no state at all, and must not read as success
    CHECK(rtl::sqlstate_to_db_sts("") == rtl::db_sts::error);
  }

  SECTION("a short or malformed state does not read past its end")
  {
    CHECK(rtl::sqlstate_to_db_sts("2") == rtl::db_sts::error);
    CHECK(rtl::sqlstate_to_db_sts("23") == rtl::db_sts::constraint_violation);
  }
}

TEST_CASE("db_error reports what it carries", "[unit][db_error]")
{
  SECTION("a server error names its sqlstate")
  {
    const rtl::db_error e{
      .sts = rtl::db_sts::duplicate_key, .message = "duplicate key", .sql_state = "23505", .driver_status = 0, .native_error = 0};
    CHECK(e.is_error());
    CHECK(e.str().contains("23505"));
    CHECK(e.str().contains("duplicate key"));
  }

  SECTION("a client side error has no sqlstate to name")
  {
    const rtl::db_error e{
      .sts = rtl::db_sts::error, .message = "statement is not prepared", .sql_state = "", .driver_status = 0, .native_error = 0};
    CHECK(e.is_error());
    CHECK_FALSE(e.str().contains("sqlstate"));
    CHECK(e.str().contains("statement is not prepared"));
  }

  SECTION("is_error() follows the status, not the presence of a message")
  {
    const rtl::db_error ok{.sts = rtl::db_sts::success, .message = "", .sql_state = "", .driver_status = 0, .native_error = 0};
    CHECK_FALSE(ok.is_error());
    const rtl::db_error warned{
      .sts = rtl::db_sts::success_with_info, .message = "truncated", .sql_state = "", .driver_status = 0, .native_error = 0};
    CHECK_FALSE(warned.is_error());
  }
}

#ifdef DBGEN4_HAS_DB2
TEST_CASE("an odbc_error copies into a db_error without losing a field", "[unit][db_error][db2]")
{
  constexpr int32_t db2_duplicate_key_native_error = -803; ///< db2's own number for a duplicate key

  auto e          = rtl::odbc_error::client("something the runtime caught itself");
  e.sql_state     = "23505";
  e.ret_          = SQL_ERROR;
  e.native_error_ = db2_duplicate_key_native_error;

  const auto d = rtl::to_db_error(e);

  CHECK(d.sts == rtl::db_sts::duplicate_key); ///< classified from the sqlstate
  CHECK(d.message == e.message);
  CHECK(d.sql_state == "23505");
  CHECK(d.driver_status == static_cast<int16_t>(SQL_ERROR)); ///< SQLRETURN, untranslated
  CHECK(d.native_error == db2_duplicate_key_native_error);   ///< db2 has one, and it survives
  CHECK(d.is_error());
}

TEST_CASE("chk() collapses a prepare()/execute() result to bool, logging only on failure", "[unit][db_error][db2]")
{
  auto& log = dbgen4::test::test_logger();

  SECTION("success passes straight through, nothing logged")
  {
    const std::expected<void, rtl::odbc_error> ok;
    CHECK(rtl::chk(ok, log, "test context"));
  }
  SECTION("failure logs and returns false")
  {
    auto                                       e      = rtl::odbc_error::client("something the runtime caught itself");
    const std::expected<void, rtl::odbc_error> failed = std::unexpected(e);
    CHECK_FALSE(rtl::chk(failed, log, "test context"));
  }
}
#elifdef DBGEN4_HAS_PSQL
TEST_CASE("a psql_error copies into a db_error without losing a field", "[unit][db_error][psql]")
{
  const rtl::psql_error e{.message = "duplicate key value violates unique constraint", .sql_state = "23505", .status = PGRES_FATAL_ERROR};

  const auto d = rtl::to_db_error(e);

  CHECK(d.sts == rtl::db_sts::duplicate_key); ///< classified from the sqlstate
  CHECK(d.message == e.message);
  CHECK(d.sql_state == "23505");
  CHECK(d.driver_status == static_cast<int16_t>(PGRES_FATAL_ERROR)); ///< ExecStatusType, untranslated
  CHECK(d.native_error == 0);                                        ///< psql has no numeric code beyond the state
  CHECK(d.is_error());
}

TEST_CASE("a pipelined row that never ran keeps that apart from one that failed", "[unit][db_error][psql]")
{
  /// The distinction execute_batch() depends on, and the reason driver_status
  /// is carried rather than folded into sts: both are errors with the same
  /// classification, and only the raw status tells them apart.
  const rtl::psql_error failed{.message = "duplicate key", .sql_state = "23505", .status = PGRES_FATAL_ERROR};
  const rtl::psql_error never_ran{.message = "", .sql_state = "", .status = PGRES_PIPELINE_ABORTED};

  CHECK(rtl::to_db_error(failed).driver_status != rtl::to_db_error(never_ran).driver_status);
}

TEST_CASE("chk() collapses a prepare()/execute() result to bool, logging only on failure", "[unit][db_error][psql]")
{
  auto& log = dbgen4::test::test_logger();

  SECTION("success passes straight through, nothing logged")
  {
    const std::expected<void, rtl::psql_error> ok;
    CHECK(rtl::chk(ok, log, "test context"));
  }
  SECTION("failure logs and returns false")
  {
    const std::expected<void, rtl::psql_error> failed =
      std::unexpected(rtl::psql_error{.message = "duplicate key", .sql_state = "23505", .status = PGRES_FATAL_ERROR});
    CHECK_FALSE(rtl::chk(failed, log, "test context"));
  }
}
#endif

TEST_CASE("chk() collapses a commit()/rollback() db_sts to bool", "[unit][db_error]")
{
  auto& log = dbgen4::test::test_logger();
  CHECK(rtl::chk(rtl::db_sts::success, log, "test context"));
  CHECK_FALSE(rtl::chk(rtl::db_sts::error, log, "test context"));
}
