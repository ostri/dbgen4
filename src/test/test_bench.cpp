// test_bench.cpp
/**
 * @file
 * @brief throughput of a batched insert and of a buffered select
 *
 * Two benchmarks, both parametrised from the environment so that a run can be
 * pointed at a different working set without rebuilding:
 *
 *   DBGEN4_BUFFER_SIZE   rows per execute        (default 4000)
 *   DBGEN4_ITERATIONS    executes to perform     (default    3)
 *   DBGEN4_COMMIT_EVERY  executes per commit     (default    1)
 *
 * The insert writes DBGEN4_BUFFER_SIZE rows per execute, DBGEN4_ITERATIONS
 * times - 12000 rows (4000 x 3) with the defaults, each carrying a full 5120
 * character tran (roughly 60 MB in all) - committing after every block,
 * because a run large enough does not fit in the database's log in one
 * transaction (see commit_every()). The select then reads the whole table
 * back through a buffer of the same size, fetching until the result set runs
 * out.
 *
 * Both leave perf_test1 empty when they finish, so that an ordinary ctest run
 * afterwards finds the table the way the other tests expect it.
 *
 * Both report timings rather than assert on them: a wall clock threshold would
 * fail on a loaded machine or a slow link and would say nothing about whether
 * the code is right. What is asserted is the row count and, for the select,
 * that every row arrives exactly once in key order.
 *
 * Tagged [.benchmark], so Catch2 skips them unless they are asked for by name
 * or by tag. Raise DBGEN4_ITERATIONS for a heavier run - at 250 (the previous
 * default) this moves a million rows and around 5 GB, which is not something
 * an ordinary `ctest` run should pay for.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "test_db.hpp"
#include "test_logger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <random>
#include <string>

namespace
{
  constexpr const std::size_t c_buffer_size = 4000;
  constexpr const std::size_t c_iterations  = 3;
  constexpr const std::size_t c_commit_size = 100;
  constexpr const std::size_t us_per_ms     = 1000; ///< microseconds per millisecond, for the timing reports
  /// rows per execute, and how many executes - both overridable from the
  /// environment, see the file comment
  size_t buffer_size() { return test_db::env_size("DBGEN4_BUFFER_SIZE", c_buffer_size); }
  size_t iterations() { return test_db::env_size("DBGEN4_ITERATIONS", c_iterations); }

  /**
   * @brief how many executes to run before committing
   *
   * One per block. A single transaction over the whole run is the simpler
   * benchmark but does not fit: a million rows of perf_test1, each carrying a
   * full 5120 character tran, needs on the order of 5 GB of log, and DB2
   * rolls the transaction back with SQL0964 once the log fills. Committing
   * per block is also what bulk loading does, and it bounds how much work a
   * failure throws away.
   *
   * Still a variable rather than a constant so that a run can measure what
   * commit frequency costs - the timing report breaks the commit out
   * separately.
   */
  size_t commit_every() { return test_db::env_size("DBGEN4_COMMIT_EVERY", c_commit_size); }

  constexpr auto   bench_date = rtl::date{.year = 2026, .month = 7, .day = 31};
  constexpr size_t name_width = 255;  ///< full declared width of perf_test1.name
  constexpr size_t tran_width = 5120; ///< full declared width of perf_test1.tran

  /// the name column of row `id` - own number, padded out to the full column
  /// width, '!' last so that a row bleeding into its neighbour is visible
  std::string bench_name(int32_t id)
  {
    auto s = fmt::format("{:05d}", id);
    s.resize(name_width - 1, '*');
    s.push_back('!');
    return s;
  }

  /**
   * @brief tran_width random printable characters
   *
   * Random content, not seeded or checked against id here: this benchmark
   * does not read tran back to verify it, only writes and counts rows, so
   * there is nothing to reproduce for. Random rather than repeated is still
   * the point - see db/db2/create_table_perf.sql - a benchmark that wrote the
   * same bytes into every row would let the database compress them away and
   * measure something other than moving 5120 bytes of real row width.
   */
  std::string bench_tran(std::mt19937& gen)
  {
    constexpr int                      first_printable = 0x20;
    constexpr int                      last_printable  = 0x7E;
    std::uniform_int_distribution<int> dist(first_printable, last_printable);

    std::string s(tran_width, '\0');
    for (auto& c : s) c = static_cast<char>(dist(gen));
    return s;
  }

  /// rows per second, for a report that stays comparable across working sets
  double per_second(size_t rows, std::chrono::microseconds took)
  {
    constexpr const std::size_t mega = 1e6;
    if (took.count() <= 0) return 0.0;
    return static_cast<double>(rows) * mega / static_cast<double>(took.count());
  }

  /**
   * @brief empty perf_test1
   *
   * TRUNCATE rather than the generated DELETE. A delete of a million rows is
   * one transaction that logs every row, and it overruns the log the same way
   * an uncommitted million row insert does - which meant a benchmark run left
   * the table in a state where the ordinary perf tests could not clear it
   * either, and five of them failed on the next ctest. TRUNCATE is logged as a
   * single operation and does not care how many rows it drops.
   *
   * Through dbx::crud::s_perf_truncate, same as every other statement here - see
   * yaml/crud.yaml for why its db2/psql text differs (the IMMEDIATE keyword).
   *
   * The commit first is not optional. DB2 requires TRUNCATE to be the first
   * statement of a unit of work and refuses it with SQL0428N otherwise, and
   * after a benchmark there is always a transaction open - the counting select
   * alone starts one.
   */
  template <typename Db>
  void clear_table(Db& db)
  {
    REQUIRE(rtl::is_success(db.commit())); // close whatever is open first
    dbx::crud::s_perf_truncate::stmt truncate(&db, dbx::crud::s_perf_truncate::qry::sql());
    require_ok(truncate.prepare(), "prepare(perf_truncate)");
    require_ok(truncate.execute(), "execute(perf_truncate)");
    REQUIRE(rtl::is_success(db.commit()));
  }

  /// size_t rather than the int32_t the column carries: every caller compares
  /// it against a row count, and those are all size_t
  template <typename Db>
  size_t count_rows(Db& db)
  {
    dbx::crud::s_perf_count::stmt cnt(&db, dbx::crud::s_perf_count::qry::sql());
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
   *
   * Every successful commit is logged at debug level with the row count it
   * just wrote (relative) and the running total (absolute) - compiled out
   * entirely in release/profile builds (is_debug_build() gates logger::Logger::
   * debug(), see logger/logger.hpp), so it costs nothing there.
   */
  template <typename Db>
  insert_timing timed_insert(Db& db, size_t rows_per_block, size_t blocks)
  {
    auto&                 log = dbgen4::test::test_logger();
    dbx::crud::s_perf_ins::stmt ins(&db, dbx::crud::s_perf_ins::qry::sql());

    /// sized before prepare(): prepare() hands the driver pointers into these
    /// arrays, and resizing moves them
    auto par = ins.get_param();
    par->set_buffer_size(rows_per_block);
    REQUIRE(par->buffer_size() == rows_per_block);
    require_ok(ins.prepare(), "prepare(perf_ins)");

    insert_timing t;
    const auto    started      = std::chrono::steady_clock::now();
    const size_t  commit_after = commit_every();
    size_t        rows_written = 0;
    std::mt19937  tran_gen{std::random_device{}()};

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
        par->set_tran(bench_tran(tran_gen), row);
      }
      const auto exec_from = std::chrono::steady_clock::now();
      t.filling += std::chrono::duration_cast<std::chrono::microseconds>(exec_from - fill_from);

      require_ok(ins.execute(), "execute(perf_ins)");
      rows_written += rows_per_block;
      const auto commit_from = std::chrono::steady_clock::now();
      t.executing += std::chrono::duration_cast<std::chrono::microseconds>(commit_from - exec_from);

      /// see commit_every() - one transaction over the whole run does not fit
      /// in the database's log
      if ((block + 1) % commit_after == 0)
      {
        REQUIRE(rtl::is_success(db.commit()));
        ++t.commits;
        t.committing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - commit_from);
        log.debug("commit #{}: {} rows this commit, {} rows total", t.commits, rows_per_block * commit_after, rows_written);
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
      log.debug("commit #{}: {} rows this commit, {} rows total", t.commits, rows_per_block * (blocks % commit_after), rows_written);
    }

    t.total = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    return t;
  }

  /**
   * @brief fill perf_test1 with rows_per_block x blocks rows
   *
   * The select benchmark needs a populated table but is not measuring how it
   * got that way, so it shares the insert benchmark's writing loop rather than
   * carrying a second copy of it.
   */
  template <typename Db>
  void fill_table(Db& db, size_t rows_per_block, size_t blocks)
  {
    dbx::crud::s_perf_ins::stmt ins(&db, dbx::crud::s_perf_ins::qry::sql());
    auto                  par = ins.get_param();
    par->set_buffer_size(rows_per_block);
    require_ok(ins.prepare(), "prepare(perf_ins setup)");

    const size_t commit_after = commit_every();
    int32_t      next_id      = 1;
    std::mt19937 tran_gen{std::random_device{}()};
    for (size_t block = 0; block < blocks; ++block)
    {
      for (size_t row = 0; row < rows_per_block; ++row)
      {
        const auto id = next_id++;
        par->set_id(id, row);
        par->set_name(bench_name(id), row);
        par->set_created(bench_date, row);
        par->set_tran(bench_tran(tran_gen), row);
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
  const size_t commit_after     = commit_every();

  live_db live;
  auto&   db  = live.db;
  auto&   log = dbgen4::test::test_logger();

  /// live_db's constructor drops the level to warn to keep ordinary test
  /// output readable; this benchmark wants its own info/debug lines back, and
  /// debug()/trace() calls compile out entirely outside debug builds anyway
  /// (see logger.hpp), so raising the runtime level here is free in
  /// release/profile.
  log.set_level(logger::level::debug);

  log.info("insert benchmark starting: backend={} host={} db={} buffer_size={} commit_every={} iterations={} total_rows={}",
            rtl::backend_name(),
            test_db::env_or("DBGEN4_TEST_HOST", "localhost"),
            test_db::env_or("DBGEN4_TEST_DB", "test"),
            rows_per_execute,
            commit_after,
            execute_count,
            total_rows);

  clear_table(db);

  const auto t = timed_insert(db, rows_per_execute, execute_count);

  log.info("insert benchmark finished: {} rows in {} ms ({:.0f} rows/s)",
            total_rows,
            t.total.count() / us_per_ms,
            per_second(total_rows, t.total));
  log.debug("insert phase breakdown: filling={} ms executing={} ms committing={} ms ({} commits)",
             t.filling.count() / us_per_ms,
             t.executing.count() / us_per_ms,
             t.committing.count() / us_per_ms,
             t.commits);

  /// the assertion: every row the benchmark claims to have written is there
  CHECK(count_rows(db) == total_rows);

  /// Leave the table as it was found. A million rows left behind is not just
  /// untidy: the ordinary perf tests clear perf_test1 with a DELETE, and a
  /// DELETE that size does not fit in the log, so the next plain ctest run
  /// would fail on a table this benchmark filled.
  clear_table(db);
}

TEST_CASE("select throughput: read the whole table through one buffer", "[.benchmark][perf][crud][live-db]")
{
  const size_t rows_per_fetch = buffer_size();

  live_db live;
  auto&   db  = live.db;
  auto&   log = dbgen4::test::test_logger();
  log.set_level(logger::level::debug); // see the insert benchmark for why

  /// The table is left populated by the insert benchmark, but a benchmark that
  /// only works when another one ran first is not one you can run on its own.
  /// Refilling costs a moment and makes the row count known.
  const size_t total_rows = rows_per_fetch * iterations();

  log.info("select benchmark starting: backend={} host={} db={} buffer_size={} iterations={} total_rows={}",
            rtl::backend_name(),
            test_db::env_or("DBGEN4_TEST_HOST", "localhost"),
            test_db::env_or("DBGEN4_TEST_DB", "test"),
            rows_per_fetch,
            iterations(),
            total_rows);

  if (count_rows(db) != total_rows)
  {
    clear_table(db);
    fill_table(db, rows_per_fetch, iterations());
  }
  REQUIRE(count_rows(db) == total_rows);

  dbx::crud::s_perf_sel_all::stmt sel(&db, dbx::crud::s_perf_sel_all::qry::sql());
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
    log.debug("fetch #{}: {} rows this fetch, {} rows total", fetches, rows->occupied(), seen);
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);

  log.info("select benchmark finished: {} rows in {} ms ({:.0f} rows/s, {} fetches)",
            seen,
            elapsed.count() / us_per_ms,
            per_second(seen, elapsed),
            fetches);

  CHECK(seen == total_rows);
  CHECK(in_order); // every row exactly once, in key order

  /// see the insert benchmark - the table goes back to empty
  clear_table(db);
}
