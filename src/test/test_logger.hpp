#pragma once
/**
 * @file
 * @brief one shared logger::Logger for the whole test binary
 *
 * logger::Logger is not a singleton - every production code path takes one
 * by reference, built once in main() (see src/dbgen4.cpp). The test binaries
 * have no such single entry point of their own (Catch2 supplies main() via
 * CATCH_CONFIG_MAIN in src/test/main.cpp), so this is the one place a test
 * asks for a Logger& - a function-local static, lazily built on first use
 * and shared by every TEST_CASE in the binary. Catch2 runs test cases
 * sequentially on one thread, so the first call always finishes constructing
 * it before a second one could race it; test_bench_async.cpp's own worker
 * threads only ever start after this has already returned to the test body.
 */

#include <logger/logger.hpp>
#include <logger/logger_config.hpp>

namespace dbgen4::test
{
  /// @brief the Logger every test in this binary logs through
  [[nodiscard]] inline logger::Logger& test_logger()
  {
    static auto log_ptr = logger::Logger::create(logger::load_logger_config());
    return **log_ptr;
  }
} // namespace dbgen4::test
