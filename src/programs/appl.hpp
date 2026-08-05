#pragma once

#include "cmd_line_params.hpp"
// #include "context.hpp"
#include "data_statements.hpp"
#include "rtl.hpp"
#include "generator.hpp"
#include "parser.hpp"
#include "parser_errors.hpp"
#include <logger/logger.hpp>
#include <atomic>
#include <chrono>
#include <expected>
#include <mutex>
namespace dbgen4
{
  using e_data_statements = ::std::expected<data_statements, exit_status_enum>;
  class appl
  {
  public:
    explicit appl(logger::Logger& log);
    ~appl();
    appl(const appl&)                       = delete;
    appl(appl&&)                            = delete;
    appl&            operator=(const appl&) = delete;
    appl&            operator=(appl&&)      = delete;
    exit_status_enum exec(int argc, char** argv, char** env); /// execute application
  private:
    /// per-worker-thread processing statistics, reported when the thread finishes
    struct worker_stats
    {
      size_t                   files_processed{};
      std::chrono::nanoseconds total_time{};
    };
    [[nodiscard]] logger::Logger& log_() const { return log_ref_; }
    /// method logs raw command line
    void display_raw_command_line_log(int argc, char** argv);
    /// process one yaml file with a dedicated parser instance (thread-safe, no shared state)
    e_data_statements process_one_file(parser& prsr, rtl::db& db, generator& gen);
    /// body of one worker thread: pulls filenames off the shared queue until it is empty
    worker_stats worker_run(size_t               worker_id,
                            const generator&     gen_prototype,
                            std::atomic<size_t>& next_file_idx,
                            std::mutex&          sts_mutex,
                            exit_status_enum&    first_error);
    /// member(s)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    logger::Logger& log_ref_; ///< reference to the shared Logger, not owner
    cmd_line_params p_;       /// comand line parameter structure
  };
}; // namespace dbgen4
