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
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp"              // IWYU pragma: keep
#include "query.hpp"                // rtl::query and rtl::odbc_error
#include "test_db.hpp"              // live_db and require_ok, shared with the other crud tests
#include <sqlext.h>                 // SQL_PARAM_ERROR - the batch row status the driver writes
#include <catch2/catch_message.hpp> // INFO
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>        // CHECK_THAT
#include <catch2/matchers/catch_matchers_string.hpp> // ContainsSubstring
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <string>
#include <string_view>
#include <vector>

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
    dbx::s_del::stmt del(&db, dbx::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del)");
    del.get_param()->set_id(test_id);
    require_ok(del.execute(), "execute(del)");
  }

  // ------------------------------------------------------------------
  // insert
  // ------------------------------------------------------------------
  {
    dbx::s_ins::stmt ins(&db, dbx::s_ins::qry::sql());
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
    dbx::s_sel::stmt sel(&db, dbx::s_sel::qry::sql());
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
    dbx::s_upd::stmt upd(&db, dbx::s_upd::qry::sql());
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
    dbx::s_sel::stmt sel(&db, dbx::s_sel::qry::sql());
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
    dbx::s_del::stmt del(&db, dbx::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del)");
    del.get_param()->set_id(test_id);
    require_ok(del.execute(), "execute(del)");
  }
  {
    dbx::s_sel::stmt sel(&db, dbx::s_sel::qry::sql());
    require_ok(sel.prepare(), "prepare(sel)");
    sel.get_param()->set_id(test_id);
    require_ok(sel.execute(), "execute(sel)");

    const auto got = sel.fetch();
    require_ok(got, "fetch(sel)");
    CHECK_FALSE(*got); // no row comes back after the delete
  }
}

// ====================================================================
// batch
// ====================================================================
namespace
{
  constexpr int32_t  batch_first  = 5000; ///< first key of the batch range
  constexpr int32_t  batch_rows   = 10;   ///< rows written in one execute
  constexpr int32_t  batch_last   = batch_first + batch_rows - 1;
  constexpr int16_t  batch_year   = 2026; ///< every batch row lands in this year
  constexpr uint16_t batch_month  = 1;
  constexpr size_t   batch_window = 3; ///< rows the reader takes at a time

  /// the value a given row of the batch carries, so that writer and reader
  /// agree without a second table of expectations
  std::string batch_name(int32_t id) { return fmt::format("row {}", id); }
  rtl::date   batch_date(int32_t id)
  { return {.year = batch_year, .month = batch_month, .day = static_cast<uint16_t>(1 + (id - batch_first))}; }

  /// remove whatever an earlier run left in the batch key range
  void clear_batch(rtl::db_db2& db)
  {
    dbx::s_del::stmt del(&db, dbx::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del range)");
    for (int32_t id = batch_first; id <= batch_last; ++id)
    {
      del.get_param()->set_id(id);
      require_ok(del.execute(), "execute(del range)");
    }
  }

  /// what one batch execute left behind, copied out before the query dies
  struct batch_outcome
  {
    std::expected<void, rtl::odbc_error> result;       ///< what execute() said
    std::vector<uint16_t>                status;       ///< one entry per row of the batch
    std::vector<int32_t>                 keys;         ///< the key each row carried, read back from the buffer
    int64_t                              affected = 0; ///< rows the database accepted
  };

  /**
   * @brief write the whole batch on one execute
   *
   * @param duplicate_row row that repeats an earlier key instead of carrying
   *        its own, or batch_rows for a batch with no duplicate at all
   */
  batch_outcome write_batch(rtl::db_db2& db, size_t duplicate_row, int32_t duplicate_of)
  {
    dbx::s_ins::stmt ins(&db, dbx::s_ins::qry::sql());

    /// Sized before prepare(), never after: prepare() hands the driver pointers
    /// into these arrays, and resizing moves them. Getting this order wrong is
    /// a use after free, not an error return.
    auto par = ins.get_param();
    par->set_buffer_size(batch_rows);
    REQUIRE(par->buffer_size() == static_cast<size_t>(batch_rows));
    REQUIRE(par->is_batch());

    require_ok(ins.prepare(), "prepare(ins batch)");

    for (size_t row = 0; row < batch_rows; ++row)
    {
      /// every row carries its own key except the planted one, which repeats a
      /// key from earlier in the same batch and so violates the primary key
      const auto id = (row == duplicate_row) ? duplicate_of : batch_first + static_cast<int32_t>(row);
      par->set_id(id, row);
      par->set_name(batch_name(id), row);
      par->set_created(batch_date(id), row);
    }

    batch_outcome out;
    out.result        = ins.execute();
    const auto status = par->row_status();
    out.status.assign(status.begin(), status.end());
    out.affected = ins.affected_rows();
    /// read back through the getter, so what is compared later is what the
    /// driver was actually handed rather than what the test meant to write
    for (size_t row = 0; row < batch_rows; ++row) out.keys.push_back(par->id(row));
    return out;
  }

  /// compare one fetched row against what the writer put there. Its own
  /// function because every CHECK is a branch, and four of them inside two
  /// nested loops is past what readability-function-cognitive-complexity allows.
  void check_batch_row(const dbx::s_sel_range::r& rows, size_t row, int32_t expected_id)
  {
    INFO("row with key " << expected_id);
    CHECK(rows.id(row) == expected_id);
    CHECK(rows.name(row) == batch_name(expected_id));
    CHECK(rows.created(row) == batch_date(expected_id));
  }

  /// @return how many rows of the batch range the table actually holds
  size_t count_batch_rows(rtl::db_db2& db)
  {
    dbx::s_sel_range::stmt sel(&db, dbx::s_sel_range::qry::sql());
    sel.get_result_buffer()->set_buffer_size(batch_window);
    require_ok(sel.prepare(), "prepare(count)");
    sel.get_param()->set_id_from(batch_first);
    sel.get_param()->set_id_to(batch_last);
    require_ok(sel.execute(), "execute(count)");

    size_t rows = 0;
    for (auto got = sel.fetch(); got && *got; got = sel.fetch()) rows += sel.get_result()->occupied();
    return rows;
  }

  /// @return how many rows each fetch handed over, checking the values as it goes
  std::vector<size_t> read_batch_windows(rtl::db_db2& db)
  {
    dbx::s_sel_range::stmt sel(&db, dbx::s_sel_range::qry::sql());
    sel.get_result_buffer()->set_buffer_size(batch_window);
    require_ok(sel.prepare(), "prepare(sel_range)");
    sel.get_param()->set_id_from(batch_first);
    sel.get_param()->set_id_to(batch_last);
    require_ok(sel.execute(), "execute(sel_range)");

    std::vector<size_t> window_sizes;
    int32_t             expected_id = batch_first;

    for (auto got = sel.fetch(); got && *got; got = sel.fetch())
    {
      auto rows = sel.get_result();
      window_sizes.push_back(rows->occupied());

      for (size_t row = 0; row < rows->occupied(); ++row) check_batch_row(*rows, row, expected_id++);
    }

    CHECK(expected_id - batch_first == batch_rows); // all ten, none twice
    return window_sizes;
  }
} // namespace

TEST_CASE("a batch of ten rows goes in on one execute and comes back three at a time", "[crud][generated][live-db][batch]")
{
  live_db live;
  auto&   db = live.db;

  clear_batch(db);

  // one execute writes all ten rows: the parameter buffer is p<10>, and the
  // generated code is the same one the single row cases use
  const auto written = write_batch(db, batch_rows, 0);
  require_ok(written.result, "execute(ins batch)");
  CHECK(written.affected == batch_rows);

  // ten rows through a three row window means four fetches - 3, 3, 3, 1 - and
  // that is the point of the exercise, not an incidental detail
  CHECK(read_batch_windows(db) == std::vector<size_t>{3, 3, 3, 1});

  clear_batch(db);
}

TEST_CASE("one duplicate key in a batch is reported against that row and no other", "[crud][generated][live-db][batch]")
{
  live_db live;
  auto&   db = live.db;

  clear_batch(db);

  /// the row that will collide, and the key it collides with
  constexpr size_t  duplicate_row = 6;
  constexpr int32_t duplicate_of  = batch_first + 2;

  /// The driver may report the batch as failed outright or as succeeded with
  /// info - either is allowed. What has to be right either way is the status
  /// array, so the result itself is deliberately not asserted on.
  const auto out = write_batch(db, duplicate_row, duplicate_of);
  REQUIRE(out.status.size() == static_cast<size_t>(batch_rows));

  std::vector<size_t> refused;
  size_t              row = 0;
  for (const auto s : out.status)
  {
    if (s == SQL_PARAM_ERROR) refused.push_back(row);
    ++row;
  }

  INFO("execute reported: " << (out.result ? std::string("success") : test_db::describe(out.result.error())));
  INFO("row status: " << fmt::format("{}", fmt::join(out.status, " ")));

  // the row that carried the duplicate is the one marked, and it is the only one
  CHECK(refused == std::vector<size_t>{duplicate_row});

  // the other nine were not thrown out with it - asked of the table rather than
  // of affected_rows(), which this runtime does not update on a failed execute
  CHECK(count_batch_rows(db) == static_cast<size_t>(batch_rows - 1));

  // and the row the driver refused really is the one carrying the duplicate:
  // read back through the getter, from the same buffer the driver was handed
  REQUIRE(refused.size() == 1);
  CHECK(out.keys.at(refused.front()) == duplicate_of);

  clear_batch(db);
}

// prepare() hands the driver raw pointers into the buffer's arrays.
// set_buffer_size() reallocates them, so those pointers dangle - and until the
// generation counter existed, execute() followed them into freed memory. ASan
// caught that; a release build would not have. Three test cases rather than
// three SECTIONs: they share no setup, and one function holding all of them is
// past what readability-function-cognitive-complexity allows.
TEST_CASE("a parameter buffer resized after prepare stops execute", "[crud][generated][live-db][batch]")
{
  live_db live;
  auto&   db = live.db;
  {
    dbx::s_ins::stmt ins(&db, dbx::s_ins::qry::sql());
    require_ok(ins.prepare(), "prepare(ins)");

    ins.get_param()->set_buffer_size(batch_rows); // the mistake

    const auto res = ins.execute();
    REQUIRE_FALSE(res);
    INFO("reported: " << test_db::describe(res.error()));
    CHECK(res.error().sql_state_ == "HY010"); // ODBC: function sequence error
    CHECK_THAT(res.error().message_, Catch::Matchers::ContainsSubstring("resized after prepare()"));
  }
}

TEST_CASE("a result buffer resized after prepare stops fetch", "[crud][generated][live-db][batch]")
{
  live_db live;
  auto&   db = live.db;
  {
    dbx::s_sel_range::stmt sel(&db, dbx::s_sel_range::qry::sql());
    require_ok(sel.prepare(), "prepare(sel_range)");
    sel.get_param()->set_id_from(batch_first);
    sel.get_param()->set_id_to(batch_last);
    require_ok(sel.execute(), "execute(sel_range)");

    sel.get_result_buffer()->set_buffer_size(batch_window); // the mistake

    const auto got = sel.fetch();
    REQUIRE_FALSE(got);
    CHECK(got.error().sql_state_ == "HY010");
  }
}

TEST_CASE("sizing before prepare is the supported order and still works", "[crud][generated][live-db][batch]")
{
  live_db live;
  auto&   db = live.db;
  {
    /// a batch delete over the test range: harmless whether or not the rows are
    /// there, and it proves the counter does not fire on the correct order
    dbx::s_del::stmt del(&db, dbx::s_del::qry::sql());
    auto                      par = del.get_param();
    par->set_buffer_size(batch_rows);
    require_ok(del.prepare(), "prepare(del batch)");
    for (size_t row = 0; row < batch_rows; ++row) par->set_id(batch_first + static_cast<int32_t>(row), row);
    require_ok(del.execute(), "execute(del batch)");
  }
}
