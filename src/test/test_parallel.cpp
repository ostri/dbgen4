// test_parallel.cpp
/**
 * @file
 * @brief exercises dbgen4's -j/--parallel option against a live database
 *
 * Unlike the other crud tests, this one does not talk to the database
 * through generated code - it drives the dbgen4-<backend> executable itself
 * as a subprocess, the same way a user would from a shell, and checks that
 * -j2 against five yaml files produces the same ten output files (.hpp/.cpp
 * per file) that -j1 would, with a zero exit status.
 *
 * DBGEN4_GENERATOR_PATH and DBGEN4_SOURCE_YAML are supplied by CMake as
 * compile definitions (see add_crud_test_target in CMakeLists.txt); the
 * database connection details reuse the DBGEN4_TEST_* environment variables
 * the crud tests already run with.
 *
 * Deliberately does not include test_db.hpp: it only needs env_or(), and
 * pulling in the backend's live_db fixture would drag in query.hpp (for
 * rtl::psql_error/rtl::odbc_error) as an ordering-dependent side effect
 * rather than an explicit include.
 */
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
  std::string env_or(const char* name, std::string_view fallback)
  {
    const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    return (value != nullptr && *value != '\0') ? std::string(value) : std::string(fallback);
  }
} // namespace

#ifndef DBGEN4_GENERATOR_PATH
#  error "DBGEN4_GENERATOR_PATH must be defined by CMake"
#endif
#ifndef DBGEN4_SOURCE_YAML
#  error "DBGEN4_SOURCE_YAML must be defined by CMake"
#endif
#ifndef DBGEN4_BACKEND_NAME
#  error "DBGEN4_BACKEND_NAME must be defined by CMake"
#endif

namespace
{
  namespace fs = std::filesystem;

  constexpr int file_count   = 5;
  constexpr int worker_count = 2;

  /// quote a single argument for /bin/sh -c, so a path containing a space
  /// still comes through as one argument
  std::string shell_quote(const std::string& arg)
  {
    std::string q = "'";
    for (const char c : arg)
    {
      if (c == '\'') q += "'\\''";
      else q += c;
    }
    q += "'";
    return q;
  }
} // namespace

TEST_CASE("parallel generation of five yaml files with two worker threads", "[parallel][generated][live-db]")
{
  const fs::path work_dir =
    fs::temp_directory_path() / fs::path(std::string("dbgen4_parallel_") + DBGEN4_BACKEND_NAME + "_" + std::to_string(::getpid()));
  const fs::path yaml_dir = work_dir / "yaml";
  const fs::path out_dir  = work_dir / "out";
  fs::create_directories(yaml_dir);
  fs::create_directories(out_dir);

  // five distinct yaml files - same content, distinct basenames, so each
  // gets its own generated .hpp/.cpp pair
  std::vector<fs::path> yaml_files;
  for (int i = 1; i <= file_count; ++i)
  {
    auto dest = yaml_dir / (std::string("p") + std::to_string(i) + ".yaml");
    fs::copy_file(DBGEN4_SOURCE_YAML, dest, fs::copy_options::overwrite_existing);
    yaml_files.push_back(dest);
  }

  const auto host = env_or("DBGEN4_TEST_HOST", "localhost");
  const auto port = env_or("DBGEN4_TEST_PORT", "0");
  const auto name = env_or("DBGEN4_TEST_DB", "test");
  const auto user = env_or("DBGEN4_TEST_USER", "dbgen4");
  const auto pass = env_or("DBGEN4_TEST_PASS", "dbgen4");

  std::string cmd = shell_quote(DBGEN4_GENERATOR_PATH);
  cmd += " -t " + std::string(DBGEN4_BACKEND_NAME);
  cmd += " -n " + shell_quote(name);
  cmd += " -u " + shell_quote(user);
  cmd += " -p " + shell_quote(pass);
  cmd += " --host " + shell_quote(host);
  cmd += " --port " + shell_quote(port);
  cmd += " -o " + shell_quote(out_dir.string());
  cmd += " -j" + std::to_string(worker_count);
  for (const auto& f : yaml_files) cmd += " " + shell_quote(f.string());

  // NOLINTNEXTLINE(cert-env33-c,concurrency-mt-unsafe,bugprone-command-processor) - the generator has no library API, only this CLI
  const int raw_status = std::system(cmd.c_str());
  REQUIRE(raw_status != -1);
  const int exit_code = WIFEXITED(raw_status) ? WEXITSTATUS(raw_status) : -1; // NOLINT(concurrency-mt-unsafe)
  CHECK(exit_code == 0);

  for (const auto& f : yaml_files)
  {
    const auto barename = f.stem().string();
    CHECK(fs::exists(out_dir / (barename + ".hpp")));
    CHECK(fs::exists(out_dir / (barename + ".cpp")));
  }

  std::error_code ec;
  fs::remove_all(work_dir, ec);
}
