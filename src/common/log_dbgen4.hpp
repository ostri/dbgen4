#ifndef LOG_HPP
#define LOG_HPP
// 1 - external fmt (dnf install)
// 0 - embeded fmt (e.g. part of API)
#define SPDLOG_FMT_EXTERNAL 1
#include "spdlog/spdlog.h" // IWYU pragma: export

namespace dbgen4
{
  using log_t = std::shared_ptr<spdlog::logger>;
  class log
  {
  private:
    static constexpr const char* log_name_ = "dbgen4";
  public:
    log();
    ~log();
    log(const log&)            = default;
    log(log&&)                 = default;
    log& operator=(const log&) = delete;
    log& operator=(log&&)      = delete;
    // NOLINTNEXTLINE (clang-tidymisc-non-private-member-variables-in-classes)
    log_t l{}; //< log instance
  };
};     // namespace dbgen4
#endif // LOG_HPP
