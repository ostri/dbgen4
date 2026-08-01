// test_crud_batch.cpp
/**
 * @file
 * @brief batch execute against a live database through the generated code
 *
 * Shared between db2 and psql: both runtimes support sending every row of a
 * parameter buffer in one execute() now (db2 via SQL_ATTR_PARAMSET_SIZE,
 * psql via libpq pipeline mode - see rtl::query::execute_batch() in
 * src/rtl/psql/query.hpp). What is NOT shared is what happens when one row
 * in the batch is bad: db2 can report nine landed and one refused, psql
 * cannot (PostgreSQL aborts the rest of a pipelined transaction after the
 * first error) - so that divergent behavior has its own test file per
 * backend: test_crud_batch_db2.cpp and test_crud_batch_psql.cpp.
 *
 * Templated on the db type (deduced from live_db::db, itself resolved per
 * backend by which test_db.hpp include path a target adds) rather than
 * named as rtl::db_db2/rtl::db_psql, and on the error type of execute()
 * (deduced via auto), which differs between rtl::odbc_error and
 * rtl::psql_error.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "query.hpp"   // rtl::query
#include "test_db.hpp" // live_db and require_ok, shared with the other crud tests
#include <catch2/catch_message.hpp> // INFO
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <fmt/format.h>
#include <string>
#include <vector>

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
  template <typename Db>
  void clear_batch(Db& db)
  {
    dbx::s_del::stmt del(&db, dbx::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del range)");
    for (int32_t id = batch_first; id <= batch_last; ++id)
    {
      del.get_param()->set_id(id);
      require_ok(del.execute(), "execute(del range)");
    }
  }

  /**
   * @brief write the whole batch on one execute, no duplicate key planted
   */
  template <typename Db>
  int64_t write_batch(Db& db)
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
      const auto id = batch_first + static_cast<int32_t>(row);
      par->set_id(id, row);
      par->set_name(batch_name(id), row);
      par->set_created(batch_date(id), row);
    }

    require_ok(ins.execute(), "execute(ins batch)");
    return ins.affected_rows();
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

  /// @return how many rows each fetch handed over, checking the values as it goes
  template <typename Db>
  std::vector<size_t> read_batch_windows(Db& db)
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
  CHECK(write_batch(db) == batch_rows);

  // ten rows through a three row window means four fetches - 3, 3, 3, 1 - and
  // that is the point of the exercise, not an incidental detail
  CHECK(read_batch_windows(db) == std::vector<size_t>{3, 3, 3, 1});

  clear_batch(db);
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
