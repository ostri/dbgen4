// test_schema_namespace.cpp
/**
 * @file
 * @brief proves dbx::<schema>::s_<id> isolates two yaml files that reuse the same statement id
 *
 * crud.yaml and crud2.yaml both declare a statement called "ins" (see
 * crud2.yaml's own comment on why - "ins"/"nice" is the common naming a real
 * consumer project, e.g. ach's dic_*.yaml files, tends to reuse across every
 * table). Before the wrapping dbx::<schema> namespace (see main_hpp.jinja),
 * #including both crud.hpp and crud2.hpp in the same translation unit failed
 * with a redefinition of dbx::s_ins - this file including both is itself
 * most of the test; the static_assert below just makes the intent explicit
 * and gives Catch2 something to report.
 */
#include "crud.hpp"
#include "crud2.hpp"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

TEST_CASE("crud.yaml's s_ins and crud2.yaml's s_ins are distinct types under their own schema namespace", "[generator][namespace]")
{
  // both compiled and linked into the same binary already proves the two
  // dbx::s_ins definitions did not collide - this only adds a check that
  // they are actually the two different types they are supposed to be,
  // not, say, one silently shadowing the other via an unqualified using.
  static_assert(! std::is_same_v<dbx::crud::s_ins::qry, dbx::crud2::s_ins::qry>,
                "dbx::crud::s_ins::qry and dbx::crud2::s_ins::qry must be distinct types");
  CHECK(dbx::crud::s_ins::qry::sql() != dbx::crud2::s_ins::qry::sql());
}
