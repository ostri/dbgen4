// unique_id_test.cpp
/**
 * @file
 * @brief rtl::unique_id() - the Twitter-snowflake-style id generator
 *
 * Backend neutral - no database needed, same reasoning as db_error_test.cpp.
 */
#include "rtl.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

TEST_CASE("unique_id never repeats, even across threads racing it at once", "[unit][unique_id]")
{
  constexpr int num_threads    = 8;
  constexpr int ids_per_thread = 5000;

  std::mutex         mtx;
  std::set<uint64_t> ids;
  bool               duplicate = false;

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (int t = 0; t < num_threads; ++t)
  {
    threads.emplace_back(
      [&, t]
      {
        for (int i = 0; i < ids_per_thread; ++i)
        {
          const auto             id = rtl::unique_id(static_cast<uint16_t>(t));
          const std::scoped_lock lk(mtx);
          if (! ids.insert(id).second) duplicate = true;
        }
      });
  }
  for (auto& th : threads) th.join();

  CHECK_FALSE(duplicate);
  CHECK(ids.size() == static_cast<size_t>(num_threads * ids_per_thread));
}

TEST_CASE("unique_id is monotonically increasing on one thread", "[unit][unique_id]")
{
  constexpr int iterations = 1000;

  uint64_t prev = rtl::unique_id(0);
  for (int i = 0; i < iterations; ++i)
  {
    const uint64_t next = rtl::unique_id(0);
    CHECK(next > prev);
    prev = next;
  }
}

TEST_CASE("unique_id stays within a signed 64-bit range (fits bigint columns)", "[unit][unique_id]")
{
  constexpr int iterations = 100;

  for (int i = 0; i < iterations; ++i)
  {
    const uint64_t id = rtl::unique_id(0);
    CHECK(id <= static_cast<uint64_t>(INT64_MAX));
  }
}
