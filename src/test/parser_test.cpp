#include <catch2/catch_test_macros.hpp>
// #include <iostream>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include "parser_errors.hpp"
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "common.hpp"
#include "parser.hpp"
#include "test_logger.hpp"

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