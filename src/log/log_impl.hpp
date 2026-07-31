#pragma once
#include "log.hpp" // IWYU pragma: keep
#include <spdlog/common.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class log::impl
{
public:
  static class ::log& instance() noexcept;
  static class log*   get() noexcept { return &instance(); }
  void                init_from_json(const std::string& config_path);
  void                init_fallback();

  void init_raw(std::string_view app_name,
                mode             m,
                enum level       console_lvl,
                enum level       file_lvl,
                int              rotation_hour,
                int              rotation_minute,
                int              keep_days,
                std::string_view pattern,
                std::string_view log_folder,
                enum level       flush_lvl);

  [[nodiscard]] enum log::level console_level() const;
  [[nodiscard]] enum log::level file_level() const;
  [[nodiscard]] enum log::level level() const;
  void                          set_console_level(enum log::level l);
  void                          set_file_level(enum log::level l);
  void                          set_level(enum log::level l);
  void                          flush();
  void                          flush_on(enum level l);

  void        _log(enum log::level l, std::string_view s);
  void        log_exception_with_chain(const std::exception& e, enum log::level lvl);
  void        log_nested_chain(const std::exception& e, int depth);
  void        setup_terminate_handler();
  void        setup_signal_handler();
  static void signal_handler(int sig);
  static void log_backtrace(const std::string& title);
  void        log_current_exception_with_chain(enum level lvl);
private:
  // Member variables
  std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
  std::shared_ptr<spdlog::sinks::daily_file_sink_mt>   file_sink_;
  std::shared_ptr<spdlog::logger>                      logger_;
  std::string                                          cfg_filename_;
  /// private methods
};
