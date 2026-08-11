// test_query_move.cpp
/**
 * @file
 * @brief moving a prepared query must not free the statement twice
 *
 * Both runtimes name a server side resource that the destructor releases:
 * psql keeps a prepared statement name (DEALLOCATE), db2 keeps an SQLHSTMT
 * (SQLFreeHandle). A defaulted move constructor copies that into the target
 * and leaves the source still claiming to own it, so both destructors free
 * the same thing.
 *
 * The second free is not harmless in either backend. On PostgreSQL the
 * DEALLOCATE of an already deallocated name fails with 26000, and inside a
 * transaction that puts the connection into 25P02 - every later statement in
 * that transaction is refused, so the damage lands on unrelated code. On DB2
 * it is a double SQLFreeHandle, which is undefined behaviour rather than a
 * diagnosable error.
 *
 * These tests are written against the observable consequence rather than the
 * mechanism: after a move, the surviving object still works and the
 * connection is still usable for further statements. That is what the bug
 * actually broke, and it is the same assertion for both backends.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep
#include "query.hpp"
#include "test_db.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
  constexpr int32_t  move_first = 7100; ///< key range of its own, so parallel test cases do not collide
  constexpr int16_t  move_year  = 2026;
  constexpr uint16_t move_month = 3;

  rtl::date move_date(int32_t id) { return {.year = move_year, .month = move_month, .day = static_cast<uint16_t>(1 + (id - move_first))}; }

  /// remove the rows this file uses, so a rerun starts from a known state
  template <typename Db>
  void clear_move_range(Db& db, int32_t first, int32_t last)
  {
    dbx::crud::s_del::stmt del(&db, dbx::crud::s_del::qry::sql());
    require_ok(del.prepare(), "prepare(del move range)");
    for (int32_t id = first; id <= last; ++id)
    {
      del.get_param()->set_id(id);
      require_ok(del.execute(), "execute(del move range)");
    }
  }

  /// @return how many rows of a key range the table holds
  template <typename Db>
  size_t count_range(Db& db, int32_t first, int32_t last)
  {
    dbx::crud::s_sel_range::stmt sel(&db, dbx::crud::s_sel_range::qry::sql());
    sel.get_result_buffer()->set_buffer_size(4);
    require_ok(sel.prepare(), "prepare(count move range)");
    sel.get_param()->set_id_from(first);
    sel.get_param()->set_id_to(last);
    require_ok(sel.execute(), "execute(count move range)");

    size_t rows = 0;
    for (auto got = sel.fetch(); got && *got; got = sel.fetch()) rows += sel.get_result()->occupied();
    return rows;
  }
} // namespace

TEST_CASE("a moved-from query does not free the statement its target now owns", "[crud][generated][live-db][move]")
{
  live_db live;
  auto&   db = live.db;

  constexpr int32_t last = move_first + 1;
  clear_move_range(db, move_first, last);

  {
    dbx::crud::s_ins::stmt src(&db, dbx::crud::s_ins::qry::sql());
    require_ok(src.prepare(), "prepare(src)");

    /// the move itself - after it, src must own nothing
    dbx::crud::s_ins::stmt dst(std::move(src));

    // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved,clang-analyzer-cplusplus.Move) - checking exactly that state
    CHECK_FALSE(src.is_prepared());
    CHECK(dst.is_prepared());

    /// the target inherited a working statement, not just the name of one
    dst.get_param()->set_id(move_first);
    dst.get_param()->set_name(std::string("moved"));
    dst.get_param()->set_created(move_date(move_first));
    require_ok(dst.execute(), "execute(dst after move)");
  }
  /// both destructors have now run. With the defaulted move they would have
  /// released the same statement twice; on psql that aborts the transaction,
  /// which the next statement is what detects.

  dbx::crud::s_ins::stmt after(&db, dbx::crud::s_ins::qry::sql());
  require_ok(after.prepare(), "prepare(after the moved pair was destroyed)");
  after.get_param()->set_id(last);
  after.get_param()->set_name(std::string("after"));
  after.get_param()->set_created(move_date(last));
  require_ok(after.execute(), "execute(after the moved pair was destroyed)");

  CHECK(count_range(db, move_first, last) == 2);

  clear_move_range(db, move_first, last);
}

TEST_CASE("a query with results survives a move with its result set intact", "[crud][generated][live-db][move]")
{
  live_db live;
  auto&   db = live.db;

  constexpr int32_t first = move_first + 10;
  constexpr int32_t last  = first + 2;
  clear_move_range(db, first, last);

  {
    dbx::crud::s_ins::stmt ins(&db, dbx::crud::s_ins::qry::sql());
    require_ok(ins.prepare(), "prepare(ins for select move)");
    for (int32_t id = first; id <= last; ++id)
    {
      ins.get_param()->set_id(id);
      ins.get_param()->set_name(fmt::format("row {}", id));
      ins.get_param()->set_created(move_date(id));
      require_ok(ins.execute(), "execute(ins for select move)");
    }
  }

  size_t rows = 0;
  {
    dbx::crud::s_sel_range::stmt src(&db, dbx::crud::s_sel_range::qry::sql());
    src.get_result_buffer()->set_buffer_size(2); ///< smaller than the range, so fetch() has to loop
    require_ok(src.prepare(), "prepare(sel src)");
    src.get_param()->set_id_from(first);
    src.get_param()->set_id_to(last);
    require_ok(src.execute(), "execute(sel src)");

    /// move after execute: the result set is mid-flight, and the bound
    /// buffers must travel with it rather than being freed underneath
    dbx::crud::s_sel_range::stmt dst(std::move(src));
    // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved,clang-analyzer-cplusplus.Move)
    CHECK_FALSE(src.is_prepared());

    for (auto got = dst.fetch(); got && *got; got = dst.fetch()) rows += dst.get_result()->occupied();
  }

  CHECK(rows == 3);

  /// the connection is still healthy after both objects went away
  CHECK(count_range(db, first, last) == 3);

  clear_move_range(db, first, last);
}

TEST_CASE("prepared queries can live in a vector that reallocates", "[crud][generated][live-db][move]")
{
  live_db live;
  auto&   db = live.db;

  constexpr int32_t first = move_first + 20;
  constexpr int32_t count = 4;
  constexpr int32_t last  = first + count - 1;
  clear_move_range(db, first, last);

  {
    /// no reserve() on purpose - every push_back past the capacity moves the
    /// elements already in the vector, which is the situation the defaulted
    /// move constructor made unusable
    std::vector<dbx::crud::s_ins::stmt> stmts;
    for (int32_t i = 0; i < count; ++i)
    {
      dbx::crud::s_ins::stmt q(&db, dbx::crud::s_ins::qry::sql());
      require_ok(q.prepare(), "prepare(vector element)");
      stmts.push_back(std::move(q));
    }

    /// every statement in the vector must still be the one it was prepared as
    for (int32_t i = 0; i < count; ++i)
    {
      auto& q = stmts.at(static_cast<size_t>(i));
      CHECK(q.is_prepared());
      const int32_t id = first + i;
      q.get_param()->set_id(id);
      q.get_param()->set_name(fmt::format("vec {}", id));
      q.get_param()->set_created(move_date(id));
      require_ok(q.execute(), "execute(vector element)");
    }
  }

  CHECK(count_range(db, first, last) == static_cast<size_t>(count));

  clear_move_range(db, first, last);
}
