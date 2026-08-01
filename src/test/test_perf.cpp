// test_perf.cpp
/**
 * @file
 * @brief how many rows move per execute, and how long that takes
 *
 * Three things are checked here, all of them about buffer sizes rather than
 * about types:
 *
 * - a whole batch goes in on one execute, and the wall clock from the first
 *   bind to the end of the commit is reported
 * - a buffered select hands back the right rows whether the buffer is larger
 *   than the table, an exact divisor of it, or neither
 * - an update that touches a subset leaves the rest alone
 *
 * Every row is self describing: the name column carries its own row number and
 * is padded to the column's full declared width, ending in '!'. A row that
 * bleeds into its neighbour in the parameter buffer therefore shows up as a
 * wrong string rather than as a silently truncated one.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "query.hpp"
#include "test_db.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <string>

namespace
{
  /// how many rows the timed insert writes, and what the select tests then
  /// find in the table. A compile time constant so that the buffer it sizes is
  /// the same one the expectations are written against.
  constexpr size_t  perf_rows  = 100;
  constexpr int32_t first_id   = 1;
  constexpr auto    fixed_date = rtl::date{.year = 2026, .month = 7, .day = 31};

  /// the date every even numbered row is moved to
  constexpr auto updated_date = rtl::date{.year = 1970, .month = 9, .day = 1};

  /// full declared width of perf_test.name
  constexpr size_t name_width = 255;

  /**
   * @brief the name column of row `id`
   *
   * "<id on 5 places>" then '*' out to the full column width, with '!' as the
   * very last character. The trailing '!' is the overflow detector: if a row
   * ran into the next one in the parameter buffer, the character at the end of
   * this row would be something else.
   */
  std::string perf_name(int32_t id)
  {
    auto s = fmt::format("{:05d}", id);
    s.resize(name_width - 1, '*');
    s.push_back('!');
    return s;
  }

  /// insert perf_rows rows as one batch and return how long insert+commit took
  template <typename Db>
  std::chrono::microseconds write_all_rows(Db& db)
  {
    dbx::s_perf_ins::stmt ins(&db, dbx::s_perf_ins::qry::sql());

    /// sized before prepare(): prepare() hands the driver pointers into these
    /// arrays, and resizing moves them
    auto par = ins.get_param();
    par->set_buffer_size(perf_rows);
    REQUIRE(par->buffer_size() == perf_rows);
    REQUIRE(par->is_batch());

    require_ok(ins.prepare(), "prepare(perf_ins)");

    /// The clock starts here rather than after the fill: filling the buffer is
    /// part of what an application pays to insert a batch, and leaving it out
    /// would flatter the number.
    const auto started = std::chrono::steady_clock::now();

    for (size_t row = 0; row < perf_rows; ++row)
    {
      const auto id = first_id + static_cast<int32_t>(row);
      par->set_id(id, row);
      par->set_name(perf_name(id), row);
      par->set_created(fixed_date, row);
    }

    require_ok(ins.execute(), "execute(perf_ins batch)");
    REQUIRE(rtl::is_success(db.commit()));

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    return elapsed;
  }

  /// @return how many rows perf_test holds
  template <typename Db>
  int32_t count_rows(Db& db)
  {
    dbx::s_perf_count::stmt cnt(&db, dbx::s_perf_count::qry::sql());
    require_ok(cnt.prepare(), "prepare(perf_count)");
    require_ok(cnt.execute(), "execute(perf_count)");
    auto got = cnt.fetch();
    require_ok(got, "fetch(perf_count)");
    REQUIRE(*got);
    return cnt.get_result()->cnt();
  }

  /// one row of a fetched window, checked against what write_all_rows put there
  void check_row(const dbx::s_perf_sel_all::r& rows, size_t row, int32_t expected_id)
  {
    INFO("row with key " << expected_id);
    CHECK(rows.id(row) == expected_id);
    CHECK(rows.name(row) == perf_name(expected_id));
    CHECK(rows.created(row) == fixed_date);
  }

  /**
   * @brief read the whole table through a buffer of `window` rows
   *
   * @return the size of every fetch, in order - the shape of this vector is
   *         what says whether the last, partial window was handled right
   */
  template <typename Db>
  std::vector<size_t> read_in_windows(Db& db, size_t window)
  {
    dbx::s_perf_sel_all::stmt sel(&db, dbx::s_perf_sel_all::qry::sql());
    sel.get_result_buffer()->set_buffer_size(window);
    require_ok(sel.prepare(), "prepare(perf_sel_all)");
    require_ok(sel.execute(), "execute(perf_sel_all)");

    std::vector<size_t> windows;
    int32_t             expected_id = first_id;

    for (auto got = sel.fetch(); got && *got; got = sel.fetch())
    {
      auto rows = sel.get_result();
      windows.push_back(rows->occupied());
      for (size_t row = 0; row < rows->occupied(); ++row) check_row(*rows, row, expected_id++);
    }

    /// every row seen exactly once
    CHECK(expected_id - first_id == static_cast<int32_t>(perf_rows));
    return windows;
  }

  /// how many rows a run of `window` sized fetches should report, in order
  std::vector<size_t> expected_windows(size_t total, size_t window)
  {
    std::vector<size_t> want;
    for (size_t left = total; left > 0;)
    {
      const auto take = (left < window) ? left : window;
      want.push_back(take);
      left -= take;
    }
    return want;
  }

  /// what a pass over the table after the even row update found
  struct update_tally
  {
    size_t seen  = 0; ///< rows read back
    size_t moved = 0; ///< rows carrying the new date
  };

  /// one row of the post update pass. Its own function because every CHECK is
  /// a branch, and enough of them inside a nested loop pushes the test case
  /// past readability-function-cognitive-complexity.
  void check_updated_row(const dbx::s_perf_sel_all::r& rows, size_t row, update_tally& tally)
  {
    const auto id = rows.id(row);
    INFO("row with key " << id);

    /// the name is the row's own whichever date it now carries - an update
    /// that touched the wrong column shows up here
    CHECK(rows.name(row) == perf_name(id));

    const bool is_even = (id % 2 == 0);
    CHECK(rows.created(row) == (is_even ? updated_date : fixed_date));
    if (is_even) ++tally.moved;
    ++tally.seen;
  }
} // namespace

TEST_CASE("a batch of a hundred rows goes in on one execute", "[perf][crud][generated][live-db]")
{
  live_db live;
  auto&   db = live.db;

  {
    dbx::s_perf_del::stmt del(&db, dbx::s_perf_del::qry::sql());
    require_ok(del.prepare(), "prepare(perf_del)");
    require_ok(del.execute(), "execute(perf_del)");
    REQUIRE(rtl::is_success(db.commit()));
  }

  const auto elapsed = write_all_rows(db);

  /// Reported rather than asserted on: a wall clock threshold would fail on a
  /// loaded machine or a slow link and say nothing about correctness. The
  /// number is here to be read, and to be compared against itself over time.
  WARN(fmt::format("insert of {} rows through one execute plus commit: {} us ({:.1f} us/row)",
                   perf_rows,
                   elapsed.count(),
                   static_cast<double>(elapsed.count()) / static_cast<double>(perf_rows)));

  /// the actual assertion: all hundred rows are in the table
  CHECK(count_rows(db) == static_cast<int32_t>(perf_rows));
}

/// The three buffer-to-table relations are three test cases rather than three
/// SECTIONs of one: a SECTION re-runs the whole case body, so each would repay
/// the hundred row insert anyway, and separate cases can be run one at a time
/// when one of them is the one that broke.
namespace
{
  /// refill the table so that a case is runnable on its own, in any order
  template <typename Db>
  void reset_table(Db& db)
  {
    dbx::s_perf_del::stmt del(&db, dbx::s_perf_del::qry::sql());
    require_ok(del.prepare(), "prepare(perf_del)");
    require_ok(del.execute(), "execute(perf_del)");
    write_all_rows(db);
    REQUIRE(count_rows(db) == static_cast<int32_t>(perf_rows));
  }
} // namespace

TEST_CASE("a buffer larger than the table takes it all in one fetch", "[perf][crud][generated][live-db]")
{
  live_db live;
  reset_table(live.db);

  constexpr size_t window = perf_rows + 50;
  const auto       got    = read_in_windows(live.db, window);

  CHECK(got == expected_windows(perf_rows, window));
  CHECK(got.size() == 1);          // everything arrived at once
  CHECK(got.front() == perf_rows); // and it was the whole table
}

TEST_CASE("a table that is an exact multiple of the buffer fills every fetch", "[perf][crud][generated][live-db]")
{
  live_db live;
  reset_table(live.db);

  constexpr size_t window = 10; // 100 = 10 * 10, no remainder
  static_assert(perf_rows % window == 0, "this case is about the exact fit");
  const auto got = read_in_windows(live.db, window);

  CHECK(got == expected_windows(perf_rows, window));
  CHECK(got.size() == perf_rows / window);
  /// no partial window anywhere - one assertion rather than a loop, so that
  /// checking it does not add a branch per row
  CHECK(std::ranges::all_of(got, [](size_t n) { return n == window; }));
}

TEST_CASE("a table that is not a multiple of the buffer ends on a short fetch", "[perf][crud][generated][live-db]")
{
  live_db live;
  reset_table(live.db);

  constexpr size_t window = 30; // 100 = 3 * 30 + 10
  static_assert(perf_rows % window != 0, "this case is about the remainder");
  const auto got = read_in_windows(live.db, window);

  CHECK(got == expected_windows(perf_rows, window));
  CHECK(got.size() == (perf_rows / window) + 1);
  CHECK(got.back() == perf_rows % window); // the short one
}

TEST_CASE("an update moves only the rows it names", "[perf][crud][generated][live-db]")
{
  live_db live;
  auto&   db = live.db;

  {
    dbx::s_perf_del::stmt del(&db, dbx::s_perf_del::qry::sql());
    require_ok(del.prepare(), "prepare(perf_del)");
    require_ok(del.execute(), "execute(perf_del)");
  }
  write_all_rows(db);

  // ------------------------------------------------------------------
  // move every even numbered row to a different date
  // ------------------------------------------------------------------
  {
    dbx::s_perf_upd_even::stmt upd(&db, dbx::s_perf_upd_even::qry::sql());
    require_ok(upd.prepare(), "prepare(perf_upd_even)");
    upd.get_param()->set_created(updated_date);
    require_ok(upd.execute(), "execute(perf_upd_even)");

    /// ids run 1..100, so exactly half of them are even
    CHECK(upd.affected_rows() == static_cast<int64_t>(perf_rows / 2));
    REQUIRE(rtl::is_success(db.commit()));
  }

  // ------------------------------------------------------------------
  // even rows moved, odd rows untouched, and nothing else changed
  // ------------------------------------------------------------------
  {
    dbx::s_perf_sel_all::stmt sel(&db, dbx::s_perf_sel_all::qry::sql());
    sel.get_result_buffer()->set_buffer_size(perf_rows);
    require_ok(sel.prepare(), "prepare(perf_sel_all)");
    require_ok(sel.execute(), "execute(perf_sel_all)");

    update_tally tally;

    for (auto got = sel.fetch(); got && *got; got = sel.fetch())
    {
      auto rows = sel.get_result();
      for (size_t row = 0; row < rows->occupied(); ++row) check_updated_row(*rows, row, tally);
    }

    CHECK(tally.seen == perf_rows);
    CHECK(tally.moved == perf_rows / 2);
  }
}
