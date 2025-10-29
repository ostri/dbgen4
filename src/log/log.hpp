#pragma once
// 1 - external fmt (dnf install)
// 0 - embeded fmt (e.g. part of API)
// #define SPDLOG_FMT_EXTERNAL
#ifndef NDEBUG // debug build - we are tracing till trace
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#else // release build - we are tracing till info
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif
#include <spdlog/spdlog.h> // IWYU pragma: export
#include <spdlog/sinks/daily_file_sink.h>

namespace spd = spdlog;

namespace dbgen4
{
  using log_t  = std::shared_ptr<spd::logger>;
  using sink_t = std::shared_ptr<spd::sinks::daily_file_sink_mt>;
  class log
  {
  public:
    log();
    ~log();
    log(const log&)                                            = default;
    log(log&&)                                                 = default;
    log&                                 operator=(const log&) = default;
    log&                                 operator=(log&&)      = default;
    void                                 set_sink_level(spd::level::level_enum level) const;
    [[nodiscard]] spd::level::level_enum get_sink_level() const;
    // NOLINTNEXTLINE (misc-non-private-member-variables-in-classes)
    static log_t l; //< log instance
  private:
    void              establish_log();
    [[nodiscard]] int find_sink() const;
    /// members
    static constexpr const char* log_name_ = "dbgen4";
    static sink_t                sink_;
  };
}; // namespace dbgen4
