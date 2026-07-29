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
 */

#include "crud.hpp"
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include <cstdio>
#include <string>
#include <string_view>

namespace
{
  int failures = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  void check(bool ok, std::string_view what)
  {
    std::printf("  %-52s %s\n", std::string(what).c_str(), ok ? "ok" : "FAILED");
    if (! ok) ++failures;
  }

  /// report why a step failed - without this a failure says only "failed"
  void report(const rtl::odbc_error& e, std::string_view step)
  {
    std::printf("  %s failed: sqlstate '%s' native %d\n  %s\n",
                std::string(step).c_str(),
                e.sql_state_.c_str(),
                static_cast<int>(e.native_error_),
                e.message_.c_str());
    ++failures;
  }

  bool same(const rtl::date& a, const rtl::date& b) noexcept
  {
    return a.year == b.year && a.month == b.month && a.day == b.day;
  }

  std::string to_text(const rtl::date& d)
  {
    return fmt::format("{:04d}-{:02d}-{:02d}", d.year, d.month, d.day);
  }

  constexpr int32_t   test_id       = 4242;
  constexpr rtl::date first_date    = {.year = 2026, .month = 1, .day = 15};
  constexpr rtl::date second_date   = {.year = 2026, .month = 7, .day = 29};
  constexpr auto      first_name    = "first value";
  constexpr auto      second_name   = "second value";
} // namespace

int main(int argc, char** argv)
{
  log::get()->set_level(log::level::warn); // keep the test output readable

  /// connection details come from the command line so that the same binary can
  /// be pointed at another server; the defaults match the development database
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::string host = (argc > 1) ? argv[1] : "localhost";
  const auto        port = static_cast<uint16_t>((argc > 2) ? std::stoi(argv[2]) : rtl::default_port());
  const std::string name = (argc > 3) ? argv[3] : "test";
  const std::string user = (argc > 4) ? argv[4] : "ostri";
  const std::string pass = (argc > 5) ? argv[5] : "";
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  rtl::db_db2 db;
  const auto  sts = db.connect(host, port, name, user, pass);
  if (! rtl::is_success(sts))
  {
    std::printf("cannot connect to the database - is it running?\n");
    return 1;
  }

  // ------------------------------------------------------------------
  // leave no trace of an earlier run
  // ------------------------------------------------------------------
  {
    rtl::query<dbx::s_del::p> del(&db, dbx::s_del::sql);
    if (auto r = del.prepare(); ! r) { report(r.error(), "prepare(del)"); return 1; }
    del.get_param()->set_id(test_id);
    if (auto r = del.execute(); ! r) { report(r.error(), "execute(del)"); return 1; }
  }

  // ------------------------------------------------------------------
  // insert
  // ------------------------------------------------------------------
  std::printf("insert\n");
  {
    rtl::query<dbx::s_ins::p> ins(&db, dbx::s_ins::sql);
    if (auto r = ins.prepare(); ! r) { report(r.error(), "prepare(ins)"); return 1; }
    auto par = ins.get_param();
    par->set_id(test_id);
    par->set_name(first_name);
    par->set_created(first_date);
    const auto res = ins.execute();
    if (! res) { report(res.error(), "execute(ins)"); return 1; }
    check(true, "insert executes");
  }

  // ------------------------------------------------------------------
  // read back and compare against what went in
  // ------------------------------------------------------------------
  std::printf("read back after insert\n");
  {
    rtl::query<dbx::s_sel::p, dbx::s_sel::r> sel(&db, dbx::s_sel::sql);
    if (auto r = sel.prepare(); ! r) { report(r.error(), "prepare(sel)"); return 1; }
    sel.get_param()->set_id(test_id);
    if (auto r = sel.execute(); ! r) { report(r.error(), "execute(sel)"); return 1; }

    const auto got = sel.fetch();
    check(got.has_value() && *got, "one row comes back");
    if (got.has_value() && *got)
    {
      auto row = sel.get_result();
      check(row->id() == test_id, "id survives the round trip");
      check(row->name() == first_name, "name survives the round trip");
      check(same(row->created(), first_date), "date survives the round trip");
      if (row->name() != first_name)
        std::printf("      name: got '%s' want '%s'\n", std::string(row->name()).c_str(), first_name);
      if (! same(row->created(), first_date))
        std::printf("      date: got '%s' want '%s'\n", to_text(row->created()).c_str(), to_text(first_date).c_str());
    }
  }

  // ------------------------------------------------------------------
  // update
  // ------------------------------------------------------------------
  std::printf("update\n");
  {
    rtl::query<dbx::s_upd::p> upd(&db, dbx::s_upd::sql);
    if (auto r = upd.prepare(); ! r) { report(r.error(), "prepare(upd)"); return 1; }
    auto par = upd.get_param();
    par->set_name(second_name);
    par->set_created(second_date);
    par->set_id(test_id);
    const auto res = upd.execute();
    if (! res) report(res.error(), "execute(upd)");
    else check(true, "update executes");
  }

  // ------------------------------------------------------------------
  // read back again - the new values must have reached the database
  // ------------------------------------------------------------------
  std::printf("read back after update\n");
  {
    rtl::query<dbx::s_sel::p, dbx::s_sel::r> sel(&db, dbx::s_sel::sql);
    if (auto r = sel.prepare(); ! r) { report(r.error(), "prepare(sel)"); return 1; }
    sel.get_param()->set_id(test_id);
    if (auto r = sel.execute(); ! r) { report(r.error(), "execute(sel)"); return 1; }

    const auto got = sel.fetch();
    check(got.has_value() && *got, "the row is still there");
    if (got.has_value() && *got)
    {
      auto row = sel.get_result();
      check(row->id() == test_id, "id is unchanged");
      check(row->name() == second_name, "name shows the update");
      check(same(row->created(), second_date), "date shows the update");
      if (row->name() != second_name)
        std::printf("      name: got '%s' want '%s'\n", std::string(row->name()).c_str(), second_name);
      if (! same(row->created(), second_date))
        std::printf("      date: got '%s' want '%s'\n", to_text(row->created()).c_str(), to_text(second_date).c_str());
    }
  }

  // ------------------------------------------------------------------
  // delete, then prove it is gone
  // ------------------------------------------------------------------
  std::printf("delete\n");
  {
    rtl::query<dbx::s_del::p> del(&db, dbx::s_del::sql);
    if (auto r = del.prepare(); ! r) { report(r.error(), "prepare(del)"); return 1; }
    del.get_param()->set_id(test_id);
    const auto res = del.execute();
    if (! res) report(res.error(), "execute(del)");
    else check(true, "delete executes");
  }
  {
    rtl::query<dbx::s_sel::p, dbx::s_sel::r> sel(&db, dbx::s_sel::sql);
    if (auto r = sel.prepare(); ! r) { report(r.error(), "prepare(sel)"); return 1; }
    sel.get_param()->set_id(test_id);
    if (auto r = sel.execute(); ! r) { report(r.error(), "execute(sel)"); return 1; }

    const auto got = sel.fetch();
    check(got.has_value() && ! *got, "no row comes back after the delete");
  }

  db.commit();
  db.disconnect();

  std::printf("\n%s\n", failures == 0 ? "all checks passed" : "THERE WERE FAILURES");
  return failures == 0 ? 0 : 1;
}
