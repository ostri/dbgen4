#include "appl.hpp"
#include "build_type.hpp"
#include "context.hpp"
#include "parser.hpp"
#include <stdexcept>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)

#include "common.hpp"
#include "parser_errors.hpp"
#include "rtl.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>
#include <vector>

namespace dbgen4::gen
{
  using rtl::db_sts;
  using clock_t = std::chrono::steady_clock;

  appl::appl(logger::Logger& log)
  : log_ref_(log)
  , p_(log)
  {
  }

  appl::~appl() { log_().flush(); };
  /**
   * @brief method process one yaml file from parsing to code generation
   *
   * @param prsr parser instance dedicated to the calling thread
   * @param db access to the database, connected on the calling thread
   * @param gen generator instance dedicated to the calling thread
   * @return true all went ok
   * @return false there were errors / check the logs
   */
  e_data_statements appl::process_one_file(parser& prsr, rtl::db& db, generator& gen)
  {
    auto filename = gen.yaml_fn();
    auto r        = prsr.parse_yaml_file(filename, gen.db_type());
    if (! r)
    {
      auto sts = ME::enum_integer(r.error());
      log_().info("File '{}' parser status: {} db status {}", filename, magic_enum::enum_name(r.error()), sts);
      return std::unexpected(r.error());
    }
    r = prsr.load_file_meta_data(r.value(), db, p_.max_field_len()); /// statements enriched with metadata
    if (! r)
    {
      log_().info("File '{}' metadata load failed. status: {}", filename, ME::enum_name(r.error()));
      return std::unexpected(r.error());
    }
    auto res = gen.generate(r.value());
    if (! res)
    {
      log_().info("File '{}' source code generation failed. status: {}", filename, ME::enum_name(res.error()));
      return std::unexpected(res.error());
    }
    log_().info("Data model generation from file '{}' successful", filename);
    return r.value();
  }
  /**
   * @brief body of one worker thread
   *
   * Each worker owns its own database connection, parser and generator so
   * that no state is shared with the other workers - only the shared
   * work queue (next_file_idx) and the error/statistics reporting are
   * synchronized. A worker keeps pulling the next unprocessed file until
   * the queue is empty, then reports how many files it handled and how
   * long it spent doing so.
   *
   * @param worker_id 0-based index of this worker, used only for logging
   * @param gen_prototype generator with templates already loaded, copied once per worker
   * @param next_file_idx shared cursor into cmd_line_params::files()
   * @param sts_mutex guards first_error
   * @param first_error set to the first error encountered by any worker, if any
   * @return worker_stats files processed and total time spent by this worker
   */
  appl::worker_stats appl::worker_run(size_t               worker_id,
                                      const generator&     gen_prototype,
                                      std::atomic<size_t>& next_file_idx,
                                      std::mutex&          sts_mutex,
                                      exit_status_enum&    first_error)
  {
    worker_stats stats;
    const auto&  all_files = p_.files();

    auto  db_owner = rtl::make_db(log_());
    auto& db       = *db_owner;
    auto  r        = db.connect(p_.host(), p_.port(), p_.db_name(), p_.user(), p_.pass());
    if (! rtl::is_success(r))
    {
      log_().error("Worker {} unable to connect to database '{}'", worker_id, p_.db_name());
      const std::scoped_lock lock(sts_mutex);
      if (first_error == exit_status_enum::ok) first_error = exit_status_enum::connection_error;
      return stats;
    }
    log_().info("Worker {} started", worker_id);

    parser    prsr(log_());
    generator gen(gen_prototype); /// independent copy - own yaml_fn/barename/json state

    for (size_t idx = next_file_idx.fetch_add(1); idx < all_files.size(); idx = next_file_idx.fetch_add(1))
    {
      const auto& filename = all_files.at(idx);
      gen.set_yaml_fn_and_barename(filename);

      const auto file_start = clock_t::now();
      auto       res        = process_one_file(prsr, db, gen);
      const auto file_time  = clock_t::now() - file_start;

      if (! res)
      {
        log_().error("Worker {} error processing file '{}' error {}", worker_id, filename, ME::enum_name(res.error()));
        const std::scoped_lock lock(sts_mutex);
        if (first_error == exit_status_enum::ok) first_error = res.error();
      }
      else
      {
        const auto file_ms = std::chrono::duration_cast<std::chrono::milliseconds>(file_time).count();
        log_().info(
          "Worker {} processed file '{}' in {} ms, generated '{}' and '{}'", worker_id, filename, file_ms, gen.hpp_fn(), gen.cpp_fn());
      }
      ++stats.files_processed;
      stats.total_time += file_time;
    }

    db.rollback();
    db.disconnect();

    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stats.total_time).count();
    log_().info("Worker {} finished: {} file(s) processed, {} ms total", worker_id, stats.files_processed, total_ms);
    return stats;
  }
  /**
   * @brief main execution method of the application
   *
   * @param argc number of command line arguments
   * @param argv array of command line arguments
   * @param env array of environment variables
   * @return int exit status
   */
  exit_status_enum appl::exec(int argc, char** argv, char** env)
  {
    log_().info("build type: {}", build_type_name());

    log_().info("=========== Application initialized ===========");
    auto sts = p_.load_parameters(argc, argv, env);
    log_().info("Command line parsing. status: '{}'", ME::enum_name(sts));
    if (sts != exit_status_enum::ok) return sts; // exit on help or error in parsing
    display_raw_command_line_log(argc, argv);
    try
    {
      /// access to the RDBMS - whichever backend this executable was linked with, used
      /// here only as a connectivity pre-flight check before any worker thread is started
      auto  db_owner = rtl::make_db(log_());
      auto& db       = *db_owner;

      /// the sql dialect picked from the yaml file and the backend that has to
      /// describe those statements are separate choices - warn when they disagree
      const auto dialect = ME::enum_name(p_.db_type());
      if (p_.db_type() != db_type_enum::sql && dialect != rtl::backend_name())
        log_().warn("Requested sql dialect '{}' but this executable is built against the '{}' backend. "
                    "Statements will be described by '{}'.",
                    dialect,
                    rtl::backend_name(),
                    rtl::backend_name());

      auto r = db.connect(p_.host(), p_.port(), p_.db_name(), p_.user(), p_.pass());
      log_().info("Database connection status: {}", ME::enum_name<db_sts>(r));
      if (! rtl::is_success(r))
      {
        log_().error("Unable to connect to database '{}'", p_.db_name());
        return exit_status_enum::connection_error;
      }
      db.disconnect();

      const context ctx(p_);          /// package cmd line parameters
      generator     gen(ctx, log_()); /// bare bone generator, template for the per-worker copies
      auto          res = gen.register_callbacks();
      if (! res) return res.error(); /// errors in template generation
      res = gen.prepare_templates();
      if (! res) return res.error(); /// errors in template generation

      /// create the output folder once, up front - avoids every worker racing to create it
      std::error_code ec;
      std::filesystem::create_directories(std::filesystem::path(p_.out_folder()), ec);

      const auto& files        = p_.files();
      const auto  worker_count = std::min(p_.parallel(), std::max<size_t>(files.size(), 1));

      log_().info("Processing {} yaml file(s) with {} worker thread(s)", files.size(), worker_count);

      std::atomic<size_t>       next_file_idx{0};
      std::mutex                sts_mutex;
      exit_status_enum          first_error{exit_status_enum::ok};
      std::vector<worker_stats> results(worker_count);
      std::vector<std::thread>  workers;
      workers.reserve(worker_count);

      const auto run_start = clock_t::now();
      for (size_t w = 0; w < worker_count; ++w)
      {
        workers.emplace_back([this, w, &gen, &next_file_idx, &sts_mutex, &first_error, &results]()
                             { results.at(w) = worker_run(w, gen, next_file_idx, sts_mutex, first_error); });
      }
      for (auto& t : workers) t.join();
      const auto run_time = clock_t::now() - run_start;

      if (first_error != exit_status_enum::ok) sts = first_error;

      size_t total_files{};
      for (const auto& r : results) total_files += r.files_processed;
      const auto run_ms = std::chrono::duration_cast<std::chrono::milliseconds>(run_time).count();
      const auto avg_ms = total_files > 0 ? std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(run_time).count() /
                                              static_cast<double>(total_files)
                                          : 0.0;
      log_().info("Summary: {} worker thread(s), {} ms total, {} yaml file(s) processed, {:.2f} ms average per file",
                  worker_count,
                  run_ms,
                  total_files,
                  avg_ms);

      log_().info("Application exit code '{}' '{}'", ME::enum_integer(sts), ME::enum_name(sts));
      return sts;
    }
    catch (const CLI::CallForHelp& e)
    {
      log_().debug("Help exit");
      return exit_status_enum::ok;
    }
    catch (const std::runtime_error& e)
    {
      log_().critical("Runtime error: '{}'", e.what());
      return exit_status_enum::unhandled_exception;
    }
    catch (...)
    {
      const auto* const msg = "Unexpected error during application execution";
      log_().error(msg);
      return exit_status_enum::unhandled_exception;
    };
  };

  void appl::display_raw_command_line_log(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const vec_str_t vec(argv, argv + argc);
    auto            cmd_line = join(vec, " ");
    log_().trace("command line: {}", cmd_line);
  }


}; // namespace dbgen4::gen