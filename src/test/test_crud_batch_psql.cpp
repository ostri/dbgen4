// test_crud_batch_psql.cpp
/**
 * @file
 * @brief psql-only batch behavior: all-or-nothing and the psql error vocabulary
 *
 * Counterpart of test_crud_batch_db2.cpp. psql's execute_batch()
 * (src/rtl/psql/query.hpp) sends every row through a libpq pipeline, and
 * PostgreSQL aborts the rest of a pipelined transaction after the first
 * error - there is no per-row savepoint, so a batch either lands completely
 * or not at all. That is a real behavioral difference from db2, not a
 * missing feature: a case that would need per-row partial success does not
 * have a psql equivalent, and is replaced here by one that asserts the
 * all-or-nothing outcome instead.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "test_db.hpp" // live_db and require_ok, shared with the other crud tests
#include <catch2/catch_message.hpp> // INFO
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>        // CHECK_THAT
#include <catch2/matchers/catch_matchers_string.hpp> // ContainsSubstring
#include <cstdint>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <string>
#include <vector>

namespace
{
  constexpr int32_t  batch_first  = 5000; ///< first key of the batch range - matches test_crud_batch.cpp
  constexpr int32_t  batch_rows   = 10;
  constexpr int32_t  batch_last   = batch_first + batch_rows - 1;
  constexpr int16_t  batch_year   = 2026;
  constexpr uint16_t batch_month  = 1;
  constexpr size_t   batch_window = 3;

  std::string batch_name(int32_t id) { return fmt::format("row {}", id); }
  rtl::date   batch_date(int32_t id)
  { return {.year = batch_year, .month = batch_month, .day = static_cast<uint16_t>(1 + (id - batch_first))}; }

  void clear_batch(rtl::db_psql& db)
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del range)");
    for (int32_t id = batch_first; id <= batch_last; ++id)
    {
      del.get_param()->set_id(id);
      require_ok(del.execute(), "execute(del range)");
    }
  }

  /// @return how many rows of the batch range the table actually holds
  size_t count_batch_rows(rtl::db_psql& db)
  {
    dbx::crud::s_sel_range::stmt sel(&db, dbx::crud::s_sel_range::qry::sql());
    sel.get_result_buffer()->set_buffer_size(batch_window);
    require_ok(sel.prepare(), "prepare(count)");
    sel.get_param()->set_id_from(batch_first);
    sel.get_param()->set_id_to(batch_last);
    require_ok(sel.execute(), "execute(count)");

    size_t rows = 0;
    for (auto got = sel.fetch(); got && *got; got = sel.fetch()) rows += sel.get_result()->occupied();
    return rows;
  }
} // namespace

TEST_CASE("a duplicate key anywhere in a batch fails the whole batch", "[crud][generated][live-db][batch][psql]")
{
  live_db live;
  auto&   db = live.db;

  clear_batch(db);

  /// the row that will collide, and the key it collides with
  constexpr size_t  duplicate_row = 6;
  constexpr int32_t duplicate_of  = batch_first + 2;

  dbx::crud::s_ins::stmt ins(&db, dbx::crud::s_ins::qry::sql());
  auto             par = ins.get_param();
  par->set_buffer_size(batch_rows);
  require_ok(ins.prepare(), "prepare(ins batch)");

  for (size_t row = 0; row < batch_rows; ++row)
  {
    const auto id = (row == duplicate_row) ? duplicate_of : batch_first + static_cast<int32_t>(row);
    par->set_id(id, row);
    par->set_name(batch_name(id), row);
    par->set_created(batch_date(id), row);
  }

  const auto result = ins.execute();
  REQUIRE_FALSE(result); // the batch as a whole failed

  const auto status = par->row_status();
  REQUIRE(status.size() == static_cast<size_t>(batch_rows));

  // rows before the duplicate ran and succeeded, the duplicate itself and
  // everything queued after it never landed - but PostgreSQL rolled the
  // whole pipeline back, so none of it is in the table (checked below)
  INFO("execute reported: " << test_db::describe(result.error()));
  INFO("row status: " << fmt::format("{}", fmt::join(status, " ")));

  // a failed statement leaves the PostgreSQL transaction aborted - every
  // further statement is refused with sqlstate 25P02 until a rollback, same
  // as running the same query by hand in psql after an error. The caller
  // has to know this and roll back before reusing the connection; db_psql's
  // rollback() then opens a fresh transaction the same way commit() does.
  REQUIRE(rtl::is_success(db.rollback()));

  // all-or-nothing: unlike db2, nothing from this batch is in the table,
  // not even the rows that came before the duplicate key
  CHECK(count_batch_rows(db) == 0);

  clear_batch(db);
}

// prepare() hands the driver raw pointers into the buffer's arrays.
// set_buffer_size() reallocates them, so those pointers dangle - and until the
// generation counter existed, execute() followed them into freed memory. ASan
// caught that; a release build would not have.
TEST_CASE("a parameter buffer resized after prepare stops execute", "[crud][generated][live-db][batch][psql]")
{
  live_db live;
  auto&   db = live.db;
  {
    dbx::crud::s_ins::stmt ins(&db, dbx::crud::s_ins::qry::sql());
    require_ok(ins.prepare(), "prepare(ins)");

    ins.get_param()->set_buffer_size(batch_rows); // the mistake

    const auto res = ins.execute();
    REQUIRE_FALSE(res);
    INFO("reported: " << test_db::describe(res.error()));
    CHECK_THAT(res.error().message, Catch::Matchers::ContainsSubstring("resized after prepare()"));
  }
}

TEST_CASE("a result buffer resized after prepare stops fetch", "[crud][generated][live-db][batch][psql]")
{
  live_db live;
  auto&   db = live.db;
  {
    dbx::crud::s_sel_range::stmt sel(&db, dbx::crud::s_sel_range::qry::sql());
    require_ok(sel.prepare(), "prepare(sel_range)");
    sel.get_param()->set_id_from(batch_first);
    sel.get_param()->set_id_to(batch_last);
    require_ok(sel.execute(), "execute(sel_range)");

    sel.get_result_buffer()->set_buffer_size(batch_window); // the mistake

    const auto got = sel.fetch();
    REQUIRE_FALSE(got);
    CHECK_THAT(got.error().message, Catch::Matchers::ContainsSubstring("resized after prepare()"));
  }
}
