// test_bench_async.cpp
/**
 * @file
 * @brief the same work written synchronously and through async_db
 *
 * One workload, two ways of driving it, so that what the worker thread buys
 * can be read off directly rather than inferred:
 *
 *   sync   fill three blocks, write all three, commit, repeat
 *   async  fill three blocks, submit all three, then fill the *next* three
 *          while those are in flight, and only then commit
 *
 * Each iteration writes one block into each of three tables of identical
 * shape (integer, varchar(255), date, varchar(5120)) before the transaction
 * ends. That is what a generator run actually does - several related tables
 * filled together inside one unit of work - and it matters here for a reason
 * beyond realism: with one table there was nothing between submit() and
 * commit() for the worker to still be doing, so the overlap had nowhere to
 * happen.
 *
 * Filling a block is made to cost DBGEN4_FILL_DELAY_MS (default 100) of wall
 * clock, standing in for the work a real generator does to produce the values.
 * Without it the fill is ~0.3 ms against ~23 ms of database time and the
 * overlap is unmeasurable by construction - the earlier version of this
 * benchmark measured exactly that and reported 0.94x, which said nothing
 * about the facade and everything about the workload having no application
 * work to overlap.
 *
 * Parameters, all from the environment:
 *
 *   DBGEN4_BUFFER_SIZE     rows per block per table   (default 4000)
 *   DBGEN4_ITERATIONS      iterations                 (default    3)
 *   DBGEN4_COMMIT_EVERY    iterations per commit      (default    1)
 *   DBGEN4_FILL_DELAY_MS   simulated work per block   (default  100)
 *   DBGEN4_REPORT_EVERY    commits between reports    (default   25)
 *
 * Note that the default writes 3 x 4000 x 3 = 36000 rows per run, each
 * carrying 5120 random characters of tran (roughly 185 MB in all). Raise
 * DBGEN4_ITERATIONS for a heavier run - at 250 (the previous default) this
 * moves 3M rows and, with the fill delay, takes on the order of a minute and
 * a half in each mode.
 *
 * Timings are reported, never asserted on. A wall clock threshold would fail
 * on a loaded machine or a slow link and would say nothing about whether the
 * code is right. What is asserted is that both runs wrote every row.
 *
 * Tagged [.benchmark] so an ordinary ctest run does not pay for it.
 */
#include "async_db.hpp"
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
#include <thread>

namespace
{
  constexpr std::size_t c_buffer_size   = 4000;
  constexpr std::size_t c_iterations    = 3;
  constexpr std::size_t c_commit_every  = 1;
  constexpr std::size_t c_fill_delay_ms = 100;
  constexpr std::size_t c_report_every  = 25;
  constexpr std::size_t us_per_ms       = 1000;
  constexpr std::size_t table_count     = 3; ///< how many tables one iteration writes

  size_t buffer_size() { return test_db::env_size("DBGEN4_BUFFER_SIZE", c_buffer_size); }
  size_t iterations() { return test_db::env_size("DBGEN4_ITERATIONS", c_iterations); }
  size_t report_every() { return test_db::env_size("DBGEN4_REPORT_EVERY", c_report_every); }

  /**
   * @brief how many iterations to run before committing
   *
   * One by default, which is the shape a bulk load has and the safest for the
   * transaction log - a million rows in one transaction does not fit (DB2
   * gives SQL0964). Raising it is the single most interesting knob here: the
   * commit is a barrier, so with a commit every iteration the overlap has
   * only the fill of the next three blocks to work with, while a commit every
   * n iterations gives it n-1 iterations of room.
   */
  size_t commit_every() { return test_db::env_size("DBGEN4_COMMIT_EVERY", c_commit_every); }

  /// stands in for the work a generator does to produce a block of values
  size_t fill_delay_ms() { return test_db::env_size("DBGEN4_FILL_DELAY_MS", c_fill_delay_ms); }

  constexpr auto   bench_date = rtl::date{.year = 2026, .month = 7, .day = 31};
  constexpr size_t name_width = 255;  ///< full declared width of the name column
  constexpr size_t tran_width = 5120; ///< full declared width of the tran column

  /// the name column of row `id` - own number, padded out to the full column
  /// width, '!' last so that a row bleeding into its neighbour is visible
  std::string bench_name(int32_t id)
  {
    auto s = fmt::format("{:05d}", id);
    s.resize(name_width - 1, '*');
    s.push_back('!');
    return s;
  }

  /// tran_width random printable characters - see test_bench.cpp::bench_tran
  /// for why random rather than repeated content
  std::string bench_tran(std::mt19937& gen)
  {
    constexpr int                      first_printable = 0x20;
    constexpr int                      last_printable  = 0x7E;
    std::uniform_int_distribution<int> dist(first_printable, last_printable);

    std::string s(tran_width, '\0');
    for (auto& c : s) c = static_cast<char>(dist(gen));
    return s;
  }

  double per_second(size_t rows, std::chrono::microseconds took)
  {
    constexpr std::size_t mega = 1000000;
    if (took.count() <= 0) return 0.0;
    return static_cast<double>(rows) * mega / static_cast<double>(took.count());
  }

  /**
   * @brief empty the three tables
   *
   * TRUNCATE rather than DELETE, and a commit before it: a delete of a
   * million rows overruns the log, and DB2 requires TRUNCATE to be the first
   * statement of a unit of work (SQL0428N otherwise) - after a benchmark
   * there is always a transaction open.
   */
  template <typename Db>
  void clear_tables(Db& db)
  {
    REQUIRE(rtl::is_success(db.commit()));
    {
      dbx::crud::s_perf_truncate::stmt t(&db, dbx::crud::s_perf_truncate::qry::sql());
      require_ok(t.prepare(), "prepare(truncate 1)");
      require_ok(t.execute(), "execute(truncate 1)");
      REQUIRE(rtl::is_success(db.commit()));
    }
    {
      dbx::crud::s_perf_truncate2::stmt t(&db, dbx::crud::s_perf_truncate2::qry::sql());
      require_ok(t.prepare(), "prepare(truncate 2)");
      require_ok(t.execute(), "execute(truncate 2)");
      REQUIRE(rtl::is_success(db.commit()));
    }
    {
      dbx::crud::s_perf_truncate3::stmt t(&db, dbx::crud::s_perf_truncate3::qry::sql());
      require_ok(t.prepare(), "prepare(truncate 3)");
      require_ok(t.execute(), "execute(truncate 3)");
      REQUIRE(rtl::is_success(db.commit()));
    }
  }

  /// row count of each of the three tables, and their sum
  struct table_counts
  {
    size_t perf_test1 = 0;
    size_t perf_test2 = 0;
    size_t perf_test3 = 0;
    size_t total      = 0;
  };

  /// how many rows each of the three tables holds, and their sum
  template <typename Db>
  table_counts count_all(Db& db)
  {
    table_counts c;
    {
      dbx::crud::s_perf_count::stmt s(&db, dbx::crud::s_perf_count::qry::sql());
      require_ok(s.prepare(), "prepare(count 1)");
      require_ok(s.execute(), "execute(count 1)");
      auto got = s.fetch();
      require_ok(got, "fetch(count 1)");
      REQUIRE(*got);
      c.perf_test1 = static_cast<size_t>(s.get_result()->cnt());
    }
    {
      dbx::crud::s_perf_count2::stmt s(&db, dbx::crud::s_perf_count2::qry::sql());
      require_ok(s.prepare(), "prepare(count 2)");
      require_ok(s.execute(), "execute(count 2)");
      auto got = s.fetch();
      require_ok(got, "fetch(count 2)");
      REQUIRE(*got);
      c.perf_test2 = static_cast<size_t>(s.get_result()->cnt());
    }
    {
      dbx::crud::s_perf_count3::stmt s(&db, dbx::crud::s_perf_count3::qry::sql());
      require_ok(s.prepare(), "prepare(count 3)");
      require_ok(s.execute(), "execute(count 3)");
      auto got = s.fetch();
      require_ok(got, "fetch(count 3)");
      REQUIRE(*got);
      c.perf_test3 = static_cast<size_t>(s.get_result()->cnt());
    }
    c.total = c.perf_test1 + c.perf_test2 + c.perf_test3;
    return c;
  }

  /// where the time went, in whichever of the two runs produced it
  struct run_timing
  {
    std::chrono::microseconds filling{0}; ///< building the blocks, delay included
    std::chrono::microseconds writing{0}; ///< execute/submit plus commit
    std::chrono::microseconds total{0};   ///< wall clock over the whole run
    size_t                    commits = 0;
  };

  /**
   * @brief fill one block, at the cost a real generator would pay
   *
   * The sleep is what makes this benchmark about overlap rather than about
   * libpq's parameter marshalling - see the file comment.
   */
  template <typename Params>
  void fill_block(Params* par, size_t rows_per_block, int32_t& next_id, size_t delay_ms, std::mt19937& tran_gen)
  {
    for (size_t row = 0; row < rows_per_block; ++row)
    {
      const auto id = next_id++;
      par->set_id(id, row);
      par->set_name(bench_name(id), row);
      par->set_created(bench_date, row);
      par->set_tran(bench_tran(tran_gen), row);
    }
    if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }

  /**
   * @brief the synchronous run: fill a block, write it, three times, commit
   */
  template <typename Db>
  run_timing sync_insert(Db& db, size_t rows_per_block, size_t blocks)
  {
    auto& log = dbgen4::test::test_logger();

    dbx::crud::s_perf_ins::stmt  ins1(&db, dbx::crud::s_perf_ins::qry::sql());
    dbx::crud::s_perf_ins2::stmt ins2(&db, dbx::crud::s_perf_ins2::qry::sql());
    dbx::crud::s_perf_ins3::stmt ins3(&db, dbx::crud::s_perf_ins3::qry::sql());

    /// sized before prepare(): prepare() hands the driver pointers into these
    /// arrays, and resizing moves them
    auto p1 = ins1.get_param();
    auto p2 = ins2.get_param();
    auto p3 = ins3.get_param();
    p1->set_buffer_size(rows_per_block);
    p2->set_buffer_size(rows_per_block);
    p3->set_buffer_size(rows_per_block);
    require_ok(ins1.prepare(), "prepare(perf_ins sync)");
    require_ok(ins2.prepare(), "prepare(perf_ins2 sync)");
    require_ok(ins3.prepare(), "prepare(perf_ins3 sync)");

    run_timing   t;
    const auto   started      = std::chrono::steady_clock::now();
    const size_t commit_after = commit_every();
    const size_t report_after = report_every();
    const size_t delay        = fill_delay_ms();
    size_t       rows_written = 0;
    int32_t      next_id      = 1;
    std::mt19937 tran_gen{std::random_device{}()};

    for (size_t block = 0; block < blocks; ++block)
    {
      /// one block into each table, filling and writing them one at a time -
      /// the main thread does nothing else while each execute is on the wire
      const auto fill1 = std::chrono::steady_clock::now();
      fill_block(p1.get(), rows_per_block, next_id, delay, tran_gen);
      const auto write1 = std::chrono::steady_clock::now();
      t.filling += std::chrono::duration_cast<std::chrono::microseconds>(write1 - fill1);
      require_ok(ins1.execute(), "execute(perf_ins sync)");
      t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - write1);

      const auto fill2 = std::chrono::steady_clock::now();
      fill_block(p2.get(), rows_per_block, next_id, delay, tran_gen);
      const auto write2 = std::chrono::steady_clock::now();
      t.filling += std::chrono::duration_cast<std::chrono::microseconds>(write2 - fill2);
      require_ok(ins2.execute(), "execute(perf_ins2 sync)");
      t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - write2);

      const auto fill3 = std::chrono::steady_clock::now();
      fill_block(p3.get(), rows_per_block, next_id, delay, tran_gen);
      const auto write3 = std::chrono::steady_clock::now();
      t.filling += std::chrono::duration_cast<std::chrono::microseconds>(write3 - fill3);
      require_ok(ins3.execute(), "execute(perf_ins3 sync)");
      t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - write3);

      rows_written += rows_per_block * table_count;

      if ((block + 1) % commit_after == 0)
      {
        const auto commit_from = std::chrono::steady_clock::now();
        REQUIRE(rtl::is_success(db.commit()));
        ++t.commits;
        t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - commit_from);
        if (t.commits % report_after == 0) log.info("sync  commit #{}: {} rows written so far", t.commits, rows_written);
      }
    }

    /// only when the loop did not just commit - an unconditional commit here
    /// would report an empty transaction as if it were a real one
    if (blocks % commit_after != 0)
    {
      const auto commit_from = std::chrono::steady_clock::now();
      REQUIRE(rtl::is_success(db.commit()));
      ++t.commits;
      t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - commit_from);
    }

    t.total = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    return t;
  }

  /**
   * @brief the asynchronous run: submit three blocks, fill the next three
   *        while they fly, then commit
   *
   * The order is what makes this asynchronous, and it is not the obvious one.
   * Submitting and immediately committing would wait for the blocks straight
   * away and overlap nothing: a commit cannot overtake the statements it
   * commits, so it is a barrier by definition.
   *
   * So the loop submits iteration n's three blocks, fills iteration n+1's
   * three blocks, and only then commits n. The fill of n+1 - three times the
   * delay, by default 300 ms - is what happens while n is on the wire.
   */
  /**
   * @brief commit adb, timing it into t.writing and failing loudly on error
   *
   * Split out of async_insert(): that function was over clang-tidy's
   * cognitive-complexity threshold, and the two call sites (per-block commit
   * and the final one for a leftover partial group) were identical bodies
   * apart from the message in FAIL().
   */
  void commit_and_time(rtl::async_db& adb, run_timing& t, const std::chrono::steady_clock::time_point& write_from, const char* what)
  {
    const auto committed = adb.commit();
    if (! committed) FAIL(fmt::format("{} failed: {}", what, committed.error().str()));
    ++t.commits;
    t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - write_from);
  }

  template <typename Db>
  run_timing async_insert(Db& db, size_t rows_per_block, size_t blocks)
  {
    auto&         log = dbgen4::test::test_logger();
    rtl::async_db adb(db);

    auto ins1 = adb.prepare<dbx::crud::s_perf_ins::p, rtl::no_results>(dbx::crud::s_perf_ins::qry::sql(), rows_per_block);
    auto ins2 = adb.prepare<dbx::crud::s_perf_ins2::p, rtl::no_results>(dbx::crud::s_perf_ins2::qry::sql(), rows_per_block);
    auto ins3 = adb.prepare<dbx::crud::s_perf_ins3::p, rtl::no_results>(dbx::crud::s_perf_ins3::qry::sql(), rows_per_block);
    REQUIRE(ins1.has_value());
    REQUIRE(ins2.has_value());
    REQUIRE(ins3.has_value());

    run_timing   t;
    const auto   started      = std::chrono::steady_clock::now();
    const size_t commit_after = commit_every();
    const size_t report_after = report_every();
    const size_t delay        = fill_delay_ms();
    size_t       rows_written = 0;
    int32_t      next_id      = 1;
    std::mt19937 tran_gen{std::random_device{}()};

    for (size_t block = 0; block < blocks; ++block)
    {
      const auto fill_from = std::chrono::steady_clock::now();
      fill_block(ins1->param(), rows_per_block, next_id, delay, tran_gen);
      fill_block(ins2->param(), rows_per_block, next_id, delay, tran_gen);
      fill_block(ins3->param(), rows_per_block, next_id, delay, tran_gen);
      const auto write_from = std::chrono::steady_clock::now();
      t.filling += std::chrono::duration_cast<std::chrono::microseconds>(write_from - fill_from);

      /// each hands its block over and returns; the fill at the top of the
      /// next iteration is what runs while these are being sent
      adb.submit(*ins1);
      adb.submit(*ins2);
      adb.submit(*ins3);
      rows_written += rows_per_block * table_count;

      if ((block + 1) % commit_after == 0)
      {
        commit_and_time(adb, t, write_from, "commit");
        if (t.commits % report_after == 0) log.info("async commit #{}: {} rows written so far", t.commits, rows_written);
      }
      else
        t.writing += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - write_from);
    }

    if (blocks % commit_after != 0) commit_and_time(adb, t, std::chrono::steady_clock::now(), "final commit");

    t.total = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
    return t;
  }

  /// one line reporting the actual row count of each table after a run, same
  /// shape for both backends so a db2 and a psql run diff identically
  void report_counts(const char* what, const table_counts& c)
  {
    auto& log = dbgen4::test::test_logger();
    log.info("{}: perf_test1={} perf_test2={} perf_test3={} total={}", what, c.perf_test1, c.perf_test2, c.perf_test3, c.total);
  }

  /// one line per run, same shape for both, so they read side by side
  void report(const char* what, const run_timing& t, size_t rows)
  {
    auto& log = dbgen4::test::test_logger();
    log.info("{}: {} rows in {} ms ({:.0f} rows/s) - filling {} ms, writing {} ms, {} commits",
              what,
              rows,
              t.total.count() / us_per_ms,
              per_second(rows, t.total),
              t.filling.count() / us_per_ms,
              t.writing.count() / us_per_ms,
              t.commits);
  }
} // namespace

TEST_CASE("insert throughput: synchronous against async_db, three tables", "[.benchmark][async][perf][crud][live-db]")
{
  const size_t rows_per_block = buffer_size();
  const size_t blocks         = iterations();
  const size_t total_rows     = rows_per_block * blocks * table_count;

  live_db live;
  auto&   db  = live.db;
  auto&   log = dbgen4::test::test_logger();

  /// live_db drops the level to warn to keep ordinary test output readable;
  /// this benchmark wants its progress lines back
  log.set_level(logger::level::info);

  log.info("benchmark starting: backend={} host={} db={} buffer_size={} iterations={} tables={} "
            "commit_every={} fill_delay_ms={} total_rows={}",
            rtl::backend_name(),
            test_db::env_or("DBGEN4_TEST_HOST", "localhost"),
            test_db::env_or("DBGEN4_TEST_DB", "test"),
            rows_per_block,
            blocks,
            table_count,
            commit_every(),
            fill_delay_ms(),
            total_rows);

  clear_tables(db);
  const auto sync_t = sync_insert(db, rows_per_block, blocks);
  report("sync ", sync_t, total_rows);
  const auto sync_counts = count_all(db);
  report_counts("sync ", sync_counts);
  CHECK(sync_counts.total == total_rows);

  clear_tables(db);
  const auto async_t = async_insert(db, rows_per_block, blocks);
  report("async", async_t, total_rows);
  const auto async_counts = count_all(db);
  report_counts("async", async_counts);
  CHECK(async_counts.total == total_rows);

  /// The comparison the whole file exists for. Reported, not asserted: how
  /// much the overlap is worth depends on the link and on the machine, and a
  /// threshold here would fail for reasons that say nothing about the code.
  const double speedup =
    (async_t.total.count() > 0) ? static_cast<double>(sync_t.total.count()) / static_cast<double>(async_t.total.count()) : 0.0;
  log.info("async/sync: {} ms against {} ms - {:.2f}x", async_t.total.count() / us_per_ms, sync_t.total.count() / us_per_ms, speedup);

  clear_tables(db); ///< leave the tables the way the other tests expect them
}
