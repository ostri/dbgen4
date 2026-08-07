// test_types.cpp
/**
 * @file
 * @brief every supported sql type through a live round trip
 *
 * test_crud.cpp covers one column per storage category, which is enough to
 * prove the plumbing works but not enough to catch a single mistyped binding.
 * This one writes a row with a column for every sql type the generator maps
 * today, reads it back and compares each column against what went in.
 *
 * A wrong SQL_C_* code, a wrong buffer width or a wrong indicator usually
 * survives a three column test and fails here.
 */
#include "crud.hpp"
#include "rtl.hpp"
#include "rtl_fmt.hpp" // IWYU pragma: keep - formatters for rtl::date and friends
#include "test_db.hpp"
#include <algorithm>
#include <array>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string_view>

namespace
{
  constexpr int32_t types_id = 7;

  /// what goes in, column by column. Values are deliberately not round
  /// numbers: a binding that reads the wrong half of a value, or truncates
  /// one, has to produce something visibly different.
  constexpr int16_t  in_smallint = -12345;
  constexpr int64_t  in_bigint   = 9'000'000'000LL; // past what an int32 holds
  constexpr float    in_real     = 2.5F;            // exact in binary, so == is safe
  constexpr double   in_double   = 1.0 / 4.0;       // likewise
  constexpr bool     in_boolean  = true;
  constexpr auto     in_decimal  = "1234.56";       // decimal travels as text
  constexpr auto     in_char     = "ab";            // CHAR(2), fills the column
  constexpr auto     in_varchar  = "varchar value with spaces";
  constexpr auto     in_clob     = "a longer piece of text for the clob column";

  constexpr rtl::date      in_date = {.year = 2026, .month = 7, .day = 31};
  constexpr rtl::time      in_time = {.hour = 13, .minute = 45, .second = 7};
  constexpr rtl::timestamp in_timestamp =
    {.year = 2026, .month = 7, .day = 31, .hour = 13, .minute = 45, .second = 7, .fraction = 0};

  /// std::array rather than std::vector: these are namespace scope constants,
  /// and a vector would allocate during static initialisation
  ///
  /// BINARY(8) is fixed width - the database pads a shorter value out to eight
  /// bytes, so the test writes exactly eight and expects exactly eight back
  constexpr std::array<uint8_t, 8> in_binary = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
  /// VARBINARY and BLOB keep the length they were given
  constexpr std::array<uint8_t, 4> in_varbinary = {0xCA, 0xFE, 0xBA, 0xBE};
  constexpr std::array<uint8_t, 7> in_blob      = {0x00, 0xFF, 0x10, 0x20, 0x30, 0x00, 0x40};

  template <size_t n>
  rtl::bcstr_t bytes_of(const std::array<uint8_t, n>& v)
  {
    return {v.data(), v.size()};
  }

  /// compare a fetched byte range against one of the constants above
  template <size_t n>
  bool same_bytes(rtl::bcstr_t got, const std::array<uint8_t, n>& want)
  {
    return std::ranges::equal(got, want);
  }

} // namespace

TEST_CASE("every supported sql type survives a round trip", "[crud][types][generated][live-db]")
{
  live_db live;
  auto&   db = live.db;

  // ------------------------------------------------------------------
  // leave no trace of an earlier run
  // ------------------------------------------------------------------
  {
    dbx::crud::s_types_del::stmt del(&db, dbx::crud::s_types_del::qry::sql());
    require_ok(del.prepare(), "prepare(types_del)");
    require_ok(del.execute(), "execute(types_del)");
  }

  // ------------------------------------------------------------------
  // one row, every column
  // ------------------------------------------------------------------
  {
    dbx::crud::s_types_ins::stmt ins(&db, dbx::crud::s_types_ins::qry::sql());
    require_ok(ins.prepare(), "prepare(types_ins)");

    auto par = ins.get_param();
    par->set_id(types_id);
    par->set_col_smallint(in_smallint);
    par->set_col_bigint(in_bigint);
    par->set_col_real(in_real);
    par->set_col_double(in_double);
    par->set_col_boolean(in_boolean);
    par->set_col_decimal(in_decimal);
    par->set_col_char(in_char);
    par->set_col_varchar(in_varchar);
    par->set_col_clob(in_clob);
    par->set_col_binary(bytes_of(in_binary));
    par->set_col_varbinary(bytes_of(in_varbinary));
    par->set_col_blob(bytes_of(in_blob));
    par->set_col_date(in_date);
    par->set_col_time(in_time);
    par->set_col_timestamp(in_timestamp);

    require_ok(ins.execute(), "execute(types_ins)");
    CHECK(ins.affected_rows() == 1);
  }

  // ------------------------------------------------------------------
  // read it back and compare every column
  // ------------------------------------------------------------------
  {
    dbx::crud::s_types_sel::stmt sel(&db, dbx::crud::s_types_sel::qry::sql());
    require_ok(sel.prepare(), "prepare(types_sel)");
    sel.get_param()->set_id(types_id);
    require_ok(sel.execute(), "execute(types_sel)");

    auto got = sel.fetch();
    require_ok(got, "fetch(types_sel)");
    REQUIRE(*got);

    auto row = sel.get_result();

    INFO("atomic types");
    CHECK(row->id() == types_id);
    CHECK(row->col_smallint() == in_smallint);
    CHECK(row->col_bigint() == in_bigint);
    CHECK(row->col_real() == in_real);
    CHECK(row->col_double() == in_double);
    CHECK(row->col_boolean() == in_boolean);

    INFO("character types");
    /// DECIMAL travels as text, so the decimal separator is the client's.
    /// Compared against a literal period on purpose: db_db2::connect() asks
    /// for one with PATCH2=15, and this assertion is what would notice if that
    /// stopped working - without it the column would read "1234,56" under a
    /// Slovenian locale and nothing would say so.
    CHECK(row->col_decimal() == in_decimal);
    CHECK(row->col_char() == in_char);
    CHECK(row->col_varchar() == in_varchar);
    CHECK(row->col_clob() == in_clob);

    INFO("binary types");
    /// BINARY(8) is fixed width, so this is the one place the value comes back
    /// exactly as long as the column, not as long as what was written
    CHECK(same_bytes(row->col_binary(), in_binary));
    CHECK(same_bytes(row->col_varbinary(), in_varbinary));
    CHECK(same_bytes(row->col_blob(), in_blob));

    INFO("date and time structures");
    CHECK(row->col_date() == in_date);
    CHECK(row->col_time().hour == in_time.hour);
    CHECK(row->col_time().minute == in_time.minute);
    CHECK(row->col_time().second == in_time.second);
    CHECK(row->col_timestamp().year == in_timestamp.year);
    CHECK(row->col_timestamp().month == in_timestamp.month);
    CHECK(row->col_timestamp().day == in_timestamp.day);
    CHECK(row->col_timestamp().hour == in_timestamp.hour);
    CHECK(row->col_timestamp().minute == in_timestamp.minute);
    CHECK(row->col_timestamp().second == in_timestamp.second);

    /// nothing was written as NULL, so nothing may read back as NULL - a
    /// mis-sized indicator array shows up here rather than as a wrong value
    CHECK_FALSE(row->is_col_smallint_null());
    CHECK_FALSE(row->is_col_bigint_null());
    CHECK_FALSE(row->is_col_binary_null());
    CHECK_FALSE(row->is_col_blob_null());
    CHECK_FALSE(row->is_col_timestamp_null());
  }
}

TEST_CASE("a null in every nullable column reads back as null", "[crud][types][generated][live-db]")
{
  live_db live;
  auto&   db = live.db;

  {
    dbx::crud::s_types_del::stmt del(&db, dbx::crud::s_types_del::qry::sql());
    require_ok(del.prepare(), "prepare(types_del)");
    require_ok(del.execute(), "execute(types_del)");
  }

  {
    dbx::crud::s_types_ins::stmt ins(&db, dbx::crud::s_types_ins::qry::sql());
    require_ok(ins.prepare(), "prepare(types_ins)");

    auto par = ins.get_param();
    /// Every column null, then the key set back to a value. This is the
    /// sequence that used to fail: reset_all_null() marks the key NULL too,
    /// and an atomic setter that wrote only the value would leave it that way,
    /// so the insert came back with SQL0407N. Setting a value now clears the
    /// indicator whatever the column's category, which is what makes this
    /// read the way it looks.
    par->reset_all_null();
    par->set_id(types_id);

    require_ok(ins.execute(), "execute(types_ins null row)");
  }

  {
    dbx::crud::s_types_sel::stmt sel(&db, dbx::crud::s_types_sel::qry::sql());
    require_ok(sel.prepare(), "prepare(types_sel)");
    sel.get_param()->set_id(types_id);
    require_ok(sel.execute(), "execute(types_sel)");

    auto got = sel.fetch();
    require_ok(got, "fetch(types_sel)");
    REQUIRE(*got);

    auto row = sel.get_result();
    CHECK(row->id() == types_id); // the key is there
    CHECK(row->is_col_smallint_null());
    CHECK(row->is_col_bigint_null());
    CHECK(row->is_col_real_null());
    CHECK(row->is_col_double_null());
    CHECK(row->is_col_boolean_null());
    CHECK(row->is_col_decimal_null());
    CHECK(row->is_col_char_null());
    CHECK(row->is_col_varchar_null());
    CHECK(row->is_col_clob_null());
    CHECK(row->is_col_binary_null());
    CHECK(row->is_col_varbinary_null());
    CHECK(row->is_col_blob_null());
    CHECK(row->is_col_date_null());
    CHECK(row->is_col_time_null());
    CHECK(row->is_col_timestamp_null());
  }
}
