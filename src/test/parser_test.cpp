#include <catch2/catch_test_macros.hpp>
// #include <iostream>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include "parser_errors.hpp"
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "common.hpp"
#include "parser.hpp"
#include "rtl.hpp"
#include "test_logger.hpp"
#include <cstdint>
#include <cstdlib>
#include <fmt/format.h>
#include <memory>
#include <string>
#include <string_view>

TEST_CASE("empty yaml input produces no statements", "[parser]")
{
  dbgen4::parser    p(dbgen4::test::test_logger());
  const std::string empty_yaml = R"(# empty)";

  auto ans = p.parse_yaml_string(empty_yaml, dbgen4::db_type_enum::db2);
  REQUIRE(ans.error() == dbgen4::exit_status_enum::statements_attr_missing);
}

TEST_CASE("simple SELECT is parsed correctly", "[parser]")
{
  const std::string yaml = R"(
statements:
  - id: get_user
    sql: SELECT id, name FROM users WHERE id = ?
)";

  dbgen4::parser p(dbgen4::test::test_logger());
  const auto     ans = p.parse_yaml_string(yaml, dbgen4::db_type_enum::db2);
  REQUIRE(ans);
  REQUIRE(ans.value().summary().empty());
  REQUIRE(ans.value().description().empty());
  REQUIRE(ans.value().map_statements().size() == 1);
  REQUIRE(ans.value().map_statements().at("get_user").sql() == "SELECT id, name FROM users WHERE id = ?");
  REQUIRE(ans.value().map_statements().at("get_user").id() == "get_user");
  REQUIRE(ans.value().map_statements().at("get_user").dscr().empty());
  REQUIRE(ans.value().map_statements().at("get_user").par_buf_size() == 1);
}

// ============================================================================
// before/after - sql to run once around a statement's own validation (see
// data_statement::before_sql()/after_sql() and parser::load_file_meta_data())
// ============================================================================

TEST_CASE("a statement with no before/after block has empty before_sql/after_sql", "[parser]")
{
  const std::string yaml = R"(
statements:
  - id: get_user
    sql: SELECT id FROM users
)";

  dbgen4::parser p(dbgen4::test::test_logger());
  const auto     ans = p.parse_yaml_string(yaml, dbgen4::db_type_enum::psql);
  REQUIRE(ans);
  CHECK(ans.value().map_statements().at("get_user").before_sql().empty());
  CHECK(ans.value().map_statements().at("get_user").after_sql().empty());
}

TEST_CASE("before/after resolve the same generic-then-dialect-specific way sql does", "[parser]")
{
  const std::string yaml = R"(
statements:
  - id: nice
    before:
      sql: CREATE TEMPORARY TABLE tmp_x (id int)
      psql: CREATE TEMPORARY TABLE tmp_x (id int) ON COMMIT DROP
    after:
      sql: DROP TABLE tmp_x
    sql: SELECT id FROM tmp_x
)";

  dbgen4::parser p(dbgen4::test::test_logger());

  // psql picks the more specific "psql:" key over the generic "sql:" one -
  // same rule the statement's own sql already follows (see "sql is generic,
  // db_type's own key overrides it if present" in resolve_dialect_sql()).
  const auto psql_ans = p.parse_yaml_string(yaml, dbgen4::db_type_enum::psql);
  REQUIRE(psql_ans);
  CHECK(psql_ans.value().map_statements().at("nice").before_sql() == "CREATE TEMPORARY TABLE tmp_x (id int) ON COMMIT DROP");
  CHECK(psql_ans.value().map_statements().at("nice").after_sql() == "DROP TABLE tmp_x");

  // db2 has no "db2:" override on before - falls back to the generic "sql:" key,
  // same as an ordinary statement with no db2 specific sql would.
  const auto db2_ans = p.parse_yaml_string(yaml, dbgen4::db_type_enum::db2);
  REQUIRE(db2_ans);
  CHECK(db2_ans.value().map_statements().at("nice").before_sql() == "CREATE TEMPORARY TABLE tmp_x (id int)");
  CHECK(db2_ans.value().map_statements().at("nice").after_sql() == "DROP TABLE tmp_x");
}

TEST_CASE("a before block with only a db2 key resolves to empty for psql", "[parser]")
{
  // Mirrors how a statement's own sql behaves when only one dialect states
  // it and the running db_type is a different one: resolve_dialect_sql()
  // only ever falls back to the generic "sql:" key, never to another
  // dialect's own key.
  const std::string yaml = R"(
statements:
  - id: nice
    before:
      db2: DECLARE GLOBAL TEMPORARY TABLE session.tmp_x (id INTEGER)
    sql: SELECT 1
)";

  dbgen4::parser p(dbgen4::test::test_logger());
  const auto     ans = p.parse_yaml_string(yaml, dbgen4::db_type_enum::psql);
  REQUIRE(ans);
  CHECK(ans.value().map_statements().at("nice").before_sql().empty());
}

// ============================================================================
// before/after against a live database - load_file_meta_data() actually
// running rtl::db::exec() around each statement's own validation, not just
// parsing the yaml. Same DBGEN4_TEST_* environment variables as
// add_crud_test_target()'s live-db suites (see CMakeLists.txt's own
// unit_tests block for how ctest sources them from PSQL_TEST_*/DB2_TEST_*).
// Whichever backend this binary was linked against decides both the
// connection details and the sql dialect - see DBGEN4_HAS_PSQL/DBGEN4_HAS_DB2,
// the same choice db_error_test.cpp makes.
// ============================================================================

namespace
{
  std::string env_or(const char* name, std::string_view fallback)
  {
    const char* v = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    return (v != nullptr) ? std::string(v) : std::string(fallback);
  }

#ifdef DBGEN4_HAS_DB2
  // an ordinary permanent table, same as the psql branch below - not
  // DECLARE GLOBAL TEMPORARY TABLE, which needs a page-size-matched
  // tablespace this test's own authorization id is not guaranteed to have
  // (SQL0286N on a plain dev instance); after tears it down regardless of
  // which kind of table before created, so nothing here needs it temporary.
  constexpr auto        test_db_type   = dbgen4::db_type_enum::db2;
  constexpr const char* create_staging = "CREATE TABLE parser_test_ba (id INTEGER)";
  constexpr const char* select_staging = "SELECT id FROM parser_test_ba";
  constexpr const char* drop_staging   = "DROP TABLE parser_test_ba";
  constexpr const char* broken_before  = "CREATE this is not valid sql";
#elifdef DBGEN4_HAS_PSQL
  constexpr auto        test_db_type   = dbgen4::db_type_enum::psql;
  constexpr const char* create_staging = "CREATE TABLE parser_test_ba (id int)";
  constexpr const char* select_staging = "SELECT id FROM parser_test_ba";
  constexpr const char* drop_staging   = "DROP TABLE parser_test_ba";
  constexpr const char* broken_before  = "CREATE this is not valid sql";
#endif

  /// a real, connected rtl::db - opened once per test case, same
  /// DBGEN4_TEST_* variables the CMakeLists.txt unit_tests block sets
  std::unique_ptr<rtl::db> connect_test_db()
  {
    auto db   = rtl::make_db(dbgen4::test::test_logger());
    auto host = env_or("DBGEN4_TEST_HOST", "localhost");
    auto port = static_cast<uint16_t>(std::stoi(env_or("DBGEN4_TEST_PORT", "5432")));
    auto name = env_or("DBGEN4_TEST_DB", "dbgen4");
    auto user = env_or("DBGEN4_TEST_USER", "dbgen4");
    auto pass = env_or("DBGEN4_TEST_PASS", "dbgen4");
    if (! rtl::is_success(db->connect(host, port, name, user, pass)))
      FAIL(fmt::format("cannot connect to {}:{} database '{}' as '{}' - is it running?", host, port, name, user));
    return db;
  }

  /// dbgen4's own --max-field-len default (see common.hpp's
  /// default_max_field_len) - these tests have no column with a
  /// backend-unreported width to fall back on it for, so any value would do
  constexpr size_t max_field_len = 4096;
} // namespace

TEST_CASE("before creates what the statement's own sql needs, after tears it down", "[parser][live-db]")
{
  auto db = connect_test_db();
  // in case an earlier, aborted run left it behind - db.exec() is not itself
  // under test in this setup step
  db->exec(drop_staging);
  db->commit();

  dbgen4::parser p(dbgen4::test::test_logger());
  const auto     yaml_str = fmt::format(R"(
statements:
  - id: sel
    res-buf-size: 1
    result-names: [id]
    before:
      sql: {}
    after:
      sql: {}
    sql: {}
)",
                                        create_staging,
                                        drop_staging,
                                        select_staging);

  auto parsed = p.parse_yaml_string(yaml_str, test_db_type);
  REQUIRE(parsed);

  auto with_meta = p.load_file_meta_data(parsed.value(), *db, max_field_len);
  REQUIRE(with_meta); // before created the table sel's own sql reads from - validation succeeded
  CHECK(with_meta.value().map_statements().at("sel").results().size() == 1);

  // after already dropped it - a second before (a second parse+load, as a
  // fresh generator run would do) must not fail with "already exists"
  auto second = p.load_file_meta_data(parsed.value(), *db, max_field_len);
  CHECK(second.has_value());

  db->exec(drop_staging); // idempotent cleanup, in case an assertion above failed first
  db->commit();
}

TEST_CASE("a failing before sql fails the statement and is reported, before validation ever runs", "[parser][live-db]")
{
  auto db = connect_test_db();

  dbgen4::parser p(dbgen4::test::test_logger());
  const auto     yaml_str = fmt::format(R"(
statements:
  - id: sel
    before:
      sql: {}
    sql: SELECT 1
)",
                                        broken_before);

  auto parsed = p.parse_yaml_string(yaml_str, test_db_type);
  REQUIRE(parsed);

  auto with_meta = p.load_file_meta_data(parsed.value(), *db, max_field_len);
  REQUIRE_FALSE(with_meta.has_value());
  CHECK(with_meta.error() == dbgen4::exit_status_enum::sql_syntax_err);
}