#include <catch2/catch_test_macros.hpp>
// #include <iostream>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "common.hpp"
#include "parser.hpp"

TEST_CASE("empty yaml input produces no statements", "[parser]")
{
  dbgen4::parser    p;
  const std::string empty_yaml = R"(# empty)";

  auto ans = p.parse_yaml_string(empty_yaml, dbgen4::db_type_enum::db2);
  REQUIRE(! ans); // FIXME  TEST ON MISSING STATEMENTS: CLAUSE
}

TEST_CASE("simple SELECT is parsed correctly", "[parser]")
{
  const std::string yaml = R"(
- name: get_user
  sql: SELECT id, name FROM users WHERE id = ?
)";

  dbgen4::parser p;
  const auto     ans = p.parse_yaml_string(yaml, dbgen4::db_type_enum::db2);
  const auto&    s   = ans.value();

  REQUIRE(s.map_statements().size() == 1);
  CHECK(s.map_statements().at("get_user").sql() == "SELECT id, name FROM users WHERE id = ?");
}