// log.hpp
#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include "build_type.hpp"
#include <fmt/format.h>
#include <memory>
#include <string_view>

const int             keep_days_default = 7; ///< how long we keep the logs by default
constexpr const char* def_log_cfg_path  = is_debug_build() ? "config/log.debug.conf" : "config/log.release.conf";
constexpr const char* def_log_path      = is_debug_build() ? "logs/fallback.debug.log" : "logs/fallback.release.log";

class log
{
public:
  static class log& instance();

  enum class mode : uint8_t
  {
    sync,
    async
  };

  // Mnemonic level names
  enum class level : int
  {
    trace    = 0,
    debug    = 1,
    info     = 2,
    warn     = 3,
    error    = 4,
    critical = 5,
    off      = 6,
  };

  void init_fallback();
  void init_from_json(const std::string& config_path = def_log_cfg_path);

  void init_raw(std::string_view app_name        = "app",
                mode             m               = mode::sync,
                enum level       console_lvl     = is_debug_build() ? level::info : level::warn,
                enum level       lvl             = is_debug_build() ? level::debug : level::trace,
                int              rotation_hour   = 2,
                int              rotation_minute = 0,
                int              keep_days       = keep_days_default,
                std::string_view pattern         = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v",
                std::string_view log_folder      = "logs",
                enum level       flush_lvl       = level::warn);

  enum level level() const;
  enum level console_level() const;
  enum level file_level() const;
  void       set_console_level(enum level l);
  void       set_file_level(enum level l);
  void       set_level(enum level l);


  void log_exception_with_chain(const std::exception& e, enum level l = level::critical);
  void log_current_exception_with_chain(enum level l = level::critical);

  void setup_terminate_handler();
  void setup_signal_handler();
  void log_backtrace(const std::string& title);


  // Delete copy/move
  log(const log&)                    = delete;
  log& operator=(const log&)         = delete;
  log(log&&)                         = delete;
  log&              operator=(log&&) = delete;
  static class log* get();

  // clang-format off
  template <typename... Args> void trace   (fmt::format_string<Args...> fmt, Args&&... args);
  template <typename... Args> void debug   (fmt::format_string<Args...> fmt, Args&&... args);
  template <typename... Args> void info    (fmt::format_string<Args...> fmt, Args&&... args);
  template <typename... Args> void warn    (fmt::format_string<Args...> fmt, Args&&... args);
  template <typename... Args> void error   (fmt::format_string<Args...> fmt, Args&&... args);
  template <typename... Args> void critical(fmt::format_string<Args...> fmt, Args&&... args);
  // clang-format on
  void trace(std::string_view sv);
  void debug(std::string_view sv);
  void info(std::string_view sv);
  void warn(std::string_view sv);
  void error(std::string_view sv);
  void critical(std::string_view sv);

  void flush();
  void flush_on(enum level l);
private:
  log();
  ~log();
  // Helper functions
  static enum level flush_level_from_string(const std::string& level);
  // Signal handler must be static to be passed to std::signal
  static void        signal_handler(int sig);
  static const char* get_signal_name(int sig);
  void               log_backtrace(const std::string& title) const;
  void               log_nested_chain(const std::exception& e, int depth);

  void _log(enum log::level l, std::string_view s);

  class impl;
  std::unique_ptr<impl> pimpl_;
};

template <typename... Args>
inline void log::trace(fmt::format_string<Args...> fmt, Args&&... args)
{
  try
  {
    _log(level::trace, fmt::format(fmt, std::forward<Args>(args)...));
  }
  catch (...)
  {
  }
}

template <typename... Args>
inline void log::debug(fmt::format_string<Args...> fmt, Args&&... args)
{
  try
  {
    _log(level::debug, fmt::format(fmt, std::forward<Args>(args)...));
  }
  catch (...)
  {
  }
}

template <typename... Args>
inline void log::info(fmt::format_string<Args...> fmt, Args&&... args)
{
  try
  {
    _log(level::info, fmt::format(fmt, std::forward<Args>(args)...));
  }
  catch (...)
  {
  }
}

template <typename... Args>
inline void log::warn(fmt::format_string<Args...> fmt, Args&&... args)
{
  try
  {
    _log(level::warn, fmt::format(fmt, std::forward<Args>(args)...));
  }
  catch (...)
  {
  }
}

template <typename... Args>
inline void log::error(fmt::format_string<Args...> fmt, Args&&... args)
{
  try
  {
    _log(level::error, fmt::format(fmt, std::forward<Args>(args)...));
  }
  catch (...)
  {
  }
}

template <typename... Args>
inline void log::critical(fmt::format_string<Args...> fmt, Args&&... args)
{
  try
  {
    _log(level::critical, fmt::format(fmt, std::forward<Args>(args)...));
  }
  catch (...)
  {
  }
}
inline void log::trace(std::string_view sv) { _log(level::trace, sv); }
inline void log::debug(std::string_view sv) { _log(level::debug, sv); }
inline void log::info(std::string_view sv) { _log(level::info, sv); }
inline void log::warn(std::string_view sv) { _log(level::warn, sv); }
inline void log::error(std::string_view sv) { _log(level::error, sv); }
inline void log::critical(std::string_view sv) { _log(level::critical, sv); }