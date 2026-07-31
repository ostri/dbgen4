// test_db.hpp
#pragma once
/**
 * @file
 * @brief the live database fixture the crud tests share
 *
 * Extracted so that a second test file does not have to repeat the connection
 * handling. Header only and inline: there are only two of these, and a
 * separate translation unit for a fixture nobody links against twice would be
 * more build than it is worth.
 */
#include "db2_rtl.hpp"
#include "rtl.hpp"
#include "logger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <exception>
#include <fmt/format.h>
#include <string>
#include <string_view>

namespace test_db
{
  inline std::string env_or(const char* name, std::string_view fallback)
  {
    const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    return (value != nullptr && *value != '\0') ? std::string(value) : std::string(fallback);
  }

  inline std::string describe(const rtl::odbc_error& e)
  { return fmt::format("sqlstate '{}' native {}: {}", e.sql_state_, static_cast<int>(e.native_error_), e.message_); }

  /**
   * @brief a size taken from the environment, or the built in default
   *
   * Anything that is not a positive number - empty, unset, text, zero - falls
   * back to the default rather than failing: these tune a benchmark, and a
   * typo should not turn a test run into an error.
   *
   * @param name environment variable to read
   * @param fallback value to use when it says nothing usable
   */
  inline size_t env_size(const char* name, size_t fallback)
  {
    const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    if (value == nullptr || *value == '\0') return fallback;
    try
    {
      const auto parsed = std::stoll(value);
      if (parsed > 0) return static_cast<size_t>(parsed);
    }
    catch (const std::exception&)
    { /* fall through to the default */ }
    return fallback;
  }
} // namespace test_db

/**
 * @brief fail the step with the driver's own diagnosis
 *
 * Without this a failed prepare or execute would report only
 * `REQUIRE(r.has_value())`, which says nothing about why the database refused.
 * Aborts the test case - once a step has failed the ones after it would only
 * produce noise.
 */
template <typename value, typename error>
void require_ok(const std::expected<value, error>& r, std::string_view step)
{
  if (! r) FAIL(fmt::format("{} failed: {}", step, test_db::describe(r.error())));
  SUCCEED(step);
}

/**
 * @brief a connection to the test database, opened for one test case
 *
 * Connection details come from the environment rather than from argv, which
 * Catch2 now owns; ctest fills them in from the DB2_TEST_* cache variables.
 * The defaults match the development database, so the binary is still usable
 * by hand.
 */
class live_db
{
public:
  rtl::db_db2 db; // NOLINT(misc-non-private-member-variables-in-classes)

  live_db()
  {
    rtl::logger::get()->set_level(rtl::logger::level::warn); // keep the test output readable

    const auto host = test_db::env_or("DBGEN4_TEST_HOST", "localhost");
    const auto port =
      static_cast<uint16_t>(std::stoi(test_db::env_or("DBGEN4_TEST_PORT", std::to_string(rtl::default_port()))));
    const auto name = test_db::env_or("DBGEN4_TEST_DB", "test");
    const auto user = test_db::env_or("DBGEN4_TEST_USER", "dbgen4");
    const auto pass = test_db::env_or("DBGEN4_TEST_PASS", "dbgen4");

    if (! rtl::is_success(db.connect(host, port, name, user, pass)))
      FAIL(fmt::format("cannot connect to {}:{} database '{}' as '{}' - is it running?", host, port, name, user));
  }

  ~live_db()
  {
    db.commit();
    db.disconnect();
  }

  live_db(const live_db&)            = delete;
  live_db& operator=(const live_db&) = delete;
  live_db(live_db&&)                 = delete;
  live_db& operator=(live_db&&)      = delete;
};
