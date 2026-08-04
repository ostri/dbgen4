// test_t1.cpp
/**
 * @file
 * @brief the generated buffers of yaml/t1.yaml, without a database
 *
 * t1.yaml is the wide one: it covers every storage category, so this is where a
 * change to the generator or to the buffer templates shows up first. Nothing
 * here connects - that is test_crud's job. What is checked is that the
 * generated code describes itself correctly and that the buffers behave.
 */

#include "t1.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstddef>
#include <string>

namespace
{
  using p = dbx::s_sql_1::p;
  using r = dbx::s_sql_1::r;
} // namespace

TEST_CASE("a generated buffer describes its own columns", "[generated][t1]")
{
  constexpr auto columns = p::buffer_description_const();

  REQUIRE_FALSE(columns.empty());
  CHECK(p::has_parameters());

  // Every column has to carry a name and a type the backend can map. An empty
  // description compiles and then fails at bind time with a driver error that
  // says nothing about the cause - this is the cheap place to catch it.
  for (const auto& c : columns)
  {
    INFO("column '" << std::string(c.name) << "'");
    CHECK_FALSE(c.name.empty());
    CHECK_FALSE(c.base_type.empty());
  }
}

TEST_CASE("the buffer dimension defaults to what the yaml asked for", "[generated][t1]")
{
  // t1.yaml says res-buf-size: 5, par-buf-size: 2
  CHECK(r::default_buffer_size == 5);
  CHECK(p::default_buffer_size == 2);

  const r res;
  const p par;
  CHECK(res.buffer_size() == r::default_buffer_size);
  CHECK(par.buffer_size() == p::default_buffer_size);
  CHECK(res.is_batch());
  CHECK(par.is_batch());
}

TEST_CASE("the dimension can be chosen at construction and changed after", "[generated][t1]")
{
  const p explicit_size(7);
  CHECK(explicit_size.buffer_size() == 7);

  p          par;
  const auto before = par.layout_generation();
  par.set_buffer_size(64);
  CHECK(par.buffer_size() == 64);
  CHECK(par.layout_generation() != before); // the resize has to be visible to query

  // a buffer of no rows makes no sense; it is quietly one instead
  par.set_buffer_size(0);
  CHECK(par.buffer_size() == 1);
  CHECK_FALSE(par.is_batch());
}

TEST_CASE("a value written to one row is not visible in another", "[generated][t1]")
{
  p par;
  par.set_buffer_size(4);

  par.set_par_1(11, 0);
  par.set_par_1(22, 3);

  CHECK(par.par_1(0) == 11);
  CHECK(par.par_1(3) == 22);
  CHECK(par.par_1(1) != 11); // untouched rows keep their own value
}

TEST_CASE("null is set and reported per row", "[generated][t1]")
{
  p par;
  par.set_buffer_size(2);

  par.set_par_1(5, 0);
  CHECK_FALSE(par.is_par_1_null(0));

  par.set_par_1_null(0);
  CHECK(par.is_par_1_null(0));
  CHECK_FALSE(par.is_par_1_null(1)); // and only that row

  par.reset_all_null();
  CHECK(par.is_par_1_null(0));
  CHECK(par.is_par_1_null(1));
}

TEST_CASE("the per row status array follows the buffer dimension", "[generated][t1]")
{
  p par;
  par.set_buffer_size(9);
  REQUIRE(par.row_status().size() == 9);

  par.clear_row_status();
  for (const auto s : par.row_status()) CHECK(s == 0);
}

TEST_CASE("row_wise mirrors the column-wise storage by reference", "[generated][t1]")
{
  p par;
  par.set_buffer_size(4);

  par.set_par_1(11, 0);
  par.set_par_1(22, 3);

  // row() and row_wise() agree with the column-wise getters
  CHECK(par.row(0).par_1.get() == par.par_1(0));
  CHECK(par.row(3).par_1.get() == par.par_1(3));
  CHECK(par.row_wise().size() == par.buffer_size());

  // the reference is live: a value written after the row was fetched is
  // visible through the same row_t, no re-fetch needed
  const auto& row0 = par.row(0);
  par.set_par_1(99, 0);
  CHECK(row0.par_1.get() == 99);

  // same for the null indicator
  CHECK(row0.is_par_1_null.get() != rtl::null_data); // not null yet
  par.set_par_1_null(0);
  CHECK(row0.is_par_1_null.get() == rtl::null_data);

  par.reset_all_null();
  for (const auto& row : par.row_wise()) CHECK(row.is_par_1_null.get() == rtl::null_data);
}

TEST_CASE("row_wise is rebuilt on resize", "[generated][t1]")
{
  p par;
  par.set_buffer_size(2);
  par.set_par_1(7, 1);
  CHECK(par.row(1).par_1.get() == 7);

  // growing the buffer moves the underlying vectors - row_wise() has to
  // follow, not keep pointing at freed storage
  par.set_buffer_size(50);
  CHECK(par.row_wise().size() == 50);
  par.set_par_1(42, 49);
  CHECK(par.row(49).par_1.get() == 42);
}

// row_wise() for every supported result column type, exercised with a live
// database round trip, lives in test_types.cpp: class r (the result buffer)
// has no setters of its own any more (see below), so there is no way to put
// values into one from here without a driver on the other end.

TEST_CASE("dump renders every row and every column", "[generated][t1]")
{
  p par;
  par.set_buffer_size(3);
  par.set_par_1_null(1);

  const auto text = par.dump();
  CHECK_FALSE(text.empty());

  // one "rec:" line per row of the buffer
  size_t rows = 0;
  for (size_t at = text.find("rec:"); at != std::string::npos; at = text.find("rec:", at + 1)) ++rows;
  CHECK(rows == par.buffer_size());

  // the column set to null says so rather than printing a value
  CHECK_THAT(text, Catch::Matchers::ContainsSubstring("NULL"));

  // every column of the description appears in the output
  for (const auto& c : p::buffer_description_const())
  {
    INFO("column '" << std::string(c.name) << "'");
    CHECK_THAT(text, Catch::Matchers::ContainsSubstring(std::string(c.name)));
  }
}

TEST_CASE("rtl::date compares chronologically", "[rtl][date]")
{
  constexpr rtl::date early{.year = 2026, .month = 1, .day = 15};
  constexpr rtl::date late{.year = 2026, .month = 7, .day = 29};
  constexpr rtl::date same_as_early{.year = 2026, .month = 1, .day = 15};

  SECTION("equality is member wise")
  {
    CHECK(early == same_as_early);
    CHECK_FALSE(early == late);
    CHECK(early != late);
  }

  SECTION("ordering runs year, then month, then day")
  {
    CHECK(early < late);
    CHECK(late > early);
    CHECK(early <= same_as_early);
    CHECK(early >= same_as_early);

    /// the significant field wins even when a less significant one disagrees:
    /// december 2025 precedes january 2026 although 12 > 1 and 31 > 1
    constexpr rtl::date last_of_2025{.year = 2025, .month = 12, .day = 31};
    constexpr rtl::date first_of_2026{.year = 2026, .month = 1, .day = 1};
    CHECK(last_of_2025 < first_of_2026);

    constexpr rtl::date jan_31{.year = 2026, .month = 1, .day = 31};
    constexpr rtl::date feb_01{.year = 2026, .month = 2, .day = 1};
    CHECK(jan_31 < feb_01);
  }

  /// the comparisons are usable in a constant expression, so a wrong answer
  /// would fail the build rather than the run
  static_assert(early < late);
  static_assert(early == same_as_early);
}
