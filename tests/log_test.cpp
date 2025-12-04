#include <catch2/catch_test_macros.hpp>
#include "log.hpp" //NOLINT

TEST_CASE("log subsystem initializes from config file", "[log][init]")
{
  // This test does not touch DB2 at all – pure unit test
  log::instance().init_from_json("config/log.debug.conf");

  auto* l = log::get(); // logger
  REQUIRE(l != nullptr);

  SECTION("default log level in debug config is trace") { CHECK(l->level() == spdlog::level::trace); }

  SECTION("logging a message does not crash") { l->info("Catch2 unit test – logging works correctly"); }
}