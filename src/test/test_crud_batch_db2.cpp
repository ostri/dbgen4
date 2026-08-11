// test_crud_batch_db2.cpp
/**
 * @file
 * @brief db2-only batch behavior: partial success and the ODBC error vocabulary
 *
 * Split out of test_crud_batch.cpp: these cases are db2-specific because they
 * either assert on db2's partial-success batch semantics (ODBC drivers can
 * land nine rows and refuse a tenth, reporting which one via row_status())
 * or on rtl::odbc_error's fields directly (.sql_state, "HY010"). psql has
 * neither - see test_crud_batch_psql.cpp for its counterparts.
 */
#include "crud.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp"              // IWYU pragma: keep
#include "test_db.hpp"              // live_db and require_ok, shared with the other crud tests
#include <sqlext.h>                 // SQL_PARAM_ERROR - the batch row status the driver writes
#include <catch2/catch_message.hpp> // INFO
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>        // CHECK_THAT
#include <catch2/matchers/catch_matchers_string.hpp> // ContainsSubstring
#include <cstdint>
#include <expected>
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

  void clear_batch(rtl::db_db2& db)
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
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
    dbx::crud::s_ins::stmt ins(&db, dbx::crud::s_ins::qry::sql());

    auto par = ins.get_param();
    par->set_buffer_size(batch_rows);
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

  /// @return how many rows of the batch range the table actually holds
  size_t count_batch_rows(rtl::db_db2& db)
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

TEST_CASE("one duplicate key in a batch is reported against that row and no other", "[crud][generated][live-db][batch][db2]")
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
// caught that; a release build would not have.
TEST_CASE("a parameter buffer resized after prepare stops execute", "[crud][generated][live-db][batch][db2]")
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
    CHECK(res.error().sql_state == "HY010"); // ODBC: function sequence error
    CHECK_THAT(res.error().message, Catch::Matchers::ContainsSubstring("resized after prepare()"));
  }
}

TEST_CASE("a result buffer resized after prepare stops fetch", "[crud][generated][live-db][batch][db2]")
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
    CHECK(got.error().sql_state == "HY010");
  }
}
