// test_bench.cpp
/**
 * @file
 * @brief throughput of a batched insert and of a buffered select
 *
 * Two benchmarks, both parametrised from the environment so that a run can be
 * pointed at a different working set without rebuilding:
 *
 *   DBGEN4_BUFFER_SIZE   rows per execute        (default 4000)
 *   DBGEN4_ITERATIONS    executes to perform     (default  250)
 *   DBGEN4_COMMIT_EVERY  executes per commit     (default    1)
 *
 * The insert writes DBGEN4_BUFFER_SIZE rows per execute, DBGEN4_ITERATIONS
 * times - a million rows with the defaults - committing after every block,
 * because one transaction that size does not fit in the database's log (see
 * commit_every()). The select then reads the whole table back through a buffer
 * of the same size, fetching until the result set runs out.
 *
 * Both leave perf_test empty when they finish, so that an ordinary ctest run
 * afterwards finds the table the way the other tests expect it.
 *
 * Both report timings rather than assert on them: a wall clock threshold would
 * fail on a loaded machine or a slow link and would say nothing about whether
 * the code is right. What is asserted is the row count and, for the select,
 * that every row arrives exactly once in key order.
 *
 * Tagged [.benchmark], so Catch2 skips them unless they are asked for by name
 * or by tag. With the defaults these move a million rows, which is not
 * something an ordinary `ctest` run should pay for.
 */
#include "crud.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "query.hpp"
#include "test_db.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <string>

namespace
{
  /// rows per execute, and how many executes - both overridable from the
  /// environment, see the file comment
  size_t buffer_size() { return test_db::env_size("DBGEN4_BUFFER_SIZE", 4000); }
  size_t iterations() { return test_db::env_size("DBGEN4_ITERATIONS", 250); }

  /**
   * @brief how many executes to run before committing
   *
   * One per block. A single transaction over the whole run is the simpler
   * benchmark but does not fit: a million rows of perf_test needs roughly
   * 270 MB of log, and DB2 rolls the transaction back with SQL0964 once the
   * log fills. Committing per block is also what bulk loading does, and it
   * bounds how much work a failure throws away.
   *
   * Still a variable rather than a constant so that a run can measure what
   * commit frequency costs - the timing report breaks the commit out
   * separately.
   */
  size_t commit_every() { return test_db::env_size("DBGEN4_COMMIT_EVERY", 1); }

  constexpr auto   bench_date = rtl::date{.year = 2026, .month = 7, .day = 31};
  constexpr size_t name_width = 255; ///< full declared width of perf_test.name

  /// the name column of row `id` - own number, padded out to the full column
  /// width, '!' last so that a row bleeding into its neighbour is visible
  std::string bench_name(int32_t id)
  {
    auto s = fmt::format("{:05d}", id);
    s.resize(name_width - 1, '*');
    s.push_back('!');
    return s;
  }

  /// rows per second, for a report that stays comparable across working sets
  double per_second(size_t rows, std::chrono::microseconds took)
  {
    if (took.count() <= 0) return 0.0;
    return static_cast<double>(rows) * 1e6 / static_cast<double>(took.count());
  }

  /**
   * @brief empty perf_test
   *
   * TRUNCATE rather than the generated DELETE. A delete of a million rows is
   * one transaction that logs every row, and it overruns the log the same way
   * an uncommitted million row insert does - which meant a benchmark run left
   * the table in a state where the ordinary perf tests could not clear it
   * either, and five of them failed on the next ctest. TRUNCATE is logged as a
   * single operation and does not care how many rows it drops.
   *
   * Executed straight rather than through a generated statement: it takes no
   * parameters and returns no rows, so there is nothing for the generator to
   * describe.
   *
   * The commit first is not optional. DB2 requires TRUNCATE to be the first
   * statement of a unit of work and refuses it with SQL0428N otherwise, and
   * after a benchmark there is always a transaction open - the counting select
   * alone starts one.
   */
  void clear_table(rtl::db_db2& db)
  {
    REQUIRE(rtl::is_success(db.commit())); // close whatever is open first
    rtl::query<> truncate(&db, "truncate table perf_test immediate");
    require_ok(truncate.prepare(), "prepare(truncate perf_test)");
    require_ok(truncate.execute(), "execute(truncate perf_test)");
    REQUIRE(rtl::is_success(db.commit()));
  }

  /// size_t rather than the int32_t the column carries: every caller compares
  /// it against a row count, and those are all size_t
  size_t count_rows(rtl::db_db2& db)
  {
    dbx::s_perf_count::stmt cnt(&db, dbx::s_perf_count::qry::sql());
    require_ok(cnt.prepare(), "prepare(perf_count)");
    require_ok(cnt.execute(), "execute(perf_count)");
    auto got = cnt.fetch();
    require_ok(got, "fetch(perf_count)");
    REQUIRE(*got);
    return static_cast<size_t>(cnt.get_result()->cnt());
  }

  /// where the time went during a timed insert run
  struct insert_timing
  {
    std::chrono::microseconds filling{0};    ///< building the parameter buffer
    std::chrono::microseconds executing{0};  ///< the execute itself
    std::chrono::microseconds committing{0}; ///< the commits
    std::chrono::microseconds total{0};      ///< wall clock over all of it
    size_t                    commits = 0;   ///< how many there were
  };

  /**
   * @brief write blocks x rows_per_block rows, timing each phase
   *
   * Its own function rather than the body of the test case: the phase timing
   * and the periodic commit are branches, and enough of them in one Catch2
   * test case pushes it past readability-function-cognitive-complexity.
   */
  insert_timing timed_insert(rtl::db_db2& db, size_t rows_per_block, size_t blocks)
  {
    dbx::s_perf_ins::stmt ins(&db, dbx::s_perf_ins::qry::sql());

    /// sized before prepare(): prepare() hands the driver pointers into these
    /// arrays, and resizing moves them
    auto par = ins.get_param();
    par->set_buffer_size(rows_per_block);
    REQUIRE(par->buffer_size() == rows_per_block);
    require_ok(ins.prepare(), "prepare(perf_ins)");

    insert_timing t;
    const auto    started      = std::chrono::steady_clock::now();
    const size_t  commit_after = commit_every();

    int32_t next_id = 1;
    for (size_t block = 0; block < blocks; ++block)
    {
      const auto fill_from = std::chrono::steady_clock::now();
      for (size_t row = 0; row < rows_per_block; ++row)
      {
        const auto id = next_id++;
        par->set_id(id, row);
        par->set_name(bench_name(id), row);
        par->set_created(bench_date, row);
      }
      const auto exec_from = std::chrono::steady_clock::now();
      t.filling += std::chrono::duration_cast<std::chrono::microseconds>(exec_from - fill_from);

      require_ok(ins.execute(), "execute(perf_ins)");
      const auto commit_from = std::chrono::steady_clock::now();
      t.executing += std::chrono::duration_cast<std::chrono::microseconds>(commit_from - exec_from);

      /// see commit_every() - one transaction over the whole run does not fit
      /// in the database's log
      if ((block + 1) % commit_after == 0)
      {
        REQUIRE(rtl::is_success(db.commit()));
        ++t.commits;
        t.committing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - commit_from);
      }
    }

    /// Only when the loop did not just commit. With the default of one commit
    /// per block it always did, and an unconditional commit here would report
    /// an empty transaction as if it were a real one.
    if (blocks % commit_after != 0)
    {
      const auto last_from = std::chrono::steady_clock::now();
      REQUIRE(rtl::is_success(db.commit()));
      ++t.commits;
      t.committing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - last_from);
    }

    t.total = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    return t;
  }

  /**
   * @brief fill perf_test with rows_per_block x blocks rows
   *
   * The select benchmark needs a populated table but is not measuring how it
   * got that way, so it shares the insert benchmark's writing loop rather than
   * carrying a second copy of it.
   */
  void fill_table(rtl::db_db2& db, size_t rows_per_block, size_t blocks)
  {
    dbx::s_perf_ins::stmt ins(&db, dbx::s_perf_ins::qry::sql());
    auto                  par = ins.get_param();
    par->set_buffer_size(rows_per_block);
    require_ok(ins.prepare(), "prepare(perf_ins setup)");

    const size_t commit_after = commit_every();
    int32_t      next_id      = 1;
    for (size_t block = 0; block < blocks; ++block)
    {
      for (size_t row = 0; row < rows_per_block; ++row)
      {
        const auto id = next_id++;
        par->set_id(id, row);
        par->set_name(bench_name(id), row);
        par->set_created(bench_date, row);
      }
      require_ok(ins.execute(), "execute(perf_ins setup)");
      /// as in the insert benchmark: the log will not hold it all at once
      if ((block + 1) % commit_after == 0) REQUIRE(rtl::is_success(db.commit()));
    }
    REQUIRE(rtl::is_success(db.commit()));
  }
} // namespace

TEST_CASE("insert throughput: n rows per execute, m executes", "[.benchmark][perf][crud][live-db]")
{
  const size_t rows_per_execute = buffer_size();
  const size_t execute_count    = iterations();
  const size_t total_rows       = rows_per_execute * execute_count;

  live_db live;
  auto&   db = live.db;
  clear_table(db);

  const auto t = timed_insert(db, rows_per_execute, execute_count);

  WARN(fmt::format("insert: {} rows = {} per execute x {} executes\n"
                   "  total (fill + execute + commit): {} ms, {:.0f} rows/s\n"
                   "  filling the buffer:              {} ms\n"
                   "  execute:                         {} ms\n"
                   "  commit ({} of them):             {} ms",
                   total_rows,
                   rows_per_execute,
                   execute_count,
                   t.total.count() / 1000,
                   per_second(total_rows, t.total),
                   t.filling.count() / 1000,
                   t.executing.count() / 1000,
                   t.commits,
                   t.committing.count() / 1000));

  /// the assertion: every row the benchmark claims to have written is there
  CHECK(count_rows(db) == total_rows);

  /// Leave the table as it was found. A million rows left behind is not just
  /// untidy: the ordinary perf tests clear perf_test with a DELETE, and a
  /// DELETE that size does not fit in the log, so the next plain ctest run
  /// would fail on a table this benchmark filled.
  clear_table(db);
}

TEST_CASE("select throughput: read the whole table through one buffer", "[.benchmark][perf][crud][live-db]")
{
  const size_t rows_per_fetch = buffer_size();

  live_db live;
  auto&   db = live.db;

  /// The table is left populated by the insert benchmark, but a benchmark that
  /// only works when another one ran first is not one you can run on its own.
  /// Refilling costs a moment and makes the row count known.
  const size_t total_rows = rows_per_fetch * iterations();
  if (count_rows(db) != total_rows)
  {
    clear_table(db);
    fill_table(db, rows_per_fetch, iterations());
  }
  REQUIRE(count_rows(db) == total_rows);

  dbx::s_perf_sel_all::stmt sel(&db, dbx::s_perf_sel_all::qry::sql());
  sel.get_result_buffer()->set_buffer_size(rows_per_fetch);
  require_ok(sel.prepare(), "prepare(perf_sel_all)");

  const auto started = std::chrono::steady_clock::now();
  require_ok(sel.execute(), "execute(perf_sel_all)");

  size_t  seen        = 0;
  size_t  fetches     = 0;
  int32_t expected_id = 1;
  bool    in_order    = true;

  /// reads until the result set runs out, however many fetches that takes
  for (auto got = sel.fetch(); got && *got; got = sel.fetch())
  {
    auto rows = sel.get_result();
    ++fetches;
    /// The rows are checked with a running flag rather than a CHECK per row:
    /// a million assertions would cost more than the fetch being measured, and
    /// one failure is as informative as a million.
    for (size_t row = 0; row < rows->occupied(); ++row)
      if (rows->id(row) != expected_id++) in_order = false;
    seen += rows->occupied();
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);

  WARN(fmt::format("select: {} rows through a {} row buffer in {} fetches\n"
                   "  execute + fetch to exhaustion: {} ms, {:.0f} rows/s",
                   seen,
                   rows_per_fetch,
                   fetches,
                   elapsed.count() / 1000,
                   per_second(seen, elapsed)));

  CHECK(seen == total_rows);
  CHECK(in_order); // every row exactly once, in key order

  /// see the insert benchmark - the table goes back to empty
  clear_table(db);
}
