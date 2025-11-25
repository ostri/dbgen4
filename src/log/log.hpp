// log.hpp
#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include "build_type.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string_view>

const int             keep_days_default = 7; ///< how long we keep the logs by default
constexpr const char* def_log_cfg_path  = is_debug_build() ? "config/log.debug.conf" : "config/log.release.conf";
constexpr const char* def_log_path      = is_debug_build() ? "logs/fallback.debug.log" : "logs/fallback.release.log";

class log
{
public:
  // Meyers' Singleton – thread-safe, lazy initialization
  static log& instance()
  {
    static log singleton_;
    if (singleton_.cfg_filename_.empty())
    {
      const auto* config_file = std::getenv("LOG_CONFIG"); // NOLINT(concurrency-mt-unsafe)
      singleton_.init_from_json(config_file != nullptr ? config_file : "");
      singleton_.setup_terminate_handler();
      singleton_.setup_signal_handler();
    }
    return singleton_;
  }

  enum class mode : uint8_t
  {
    sync,
    async
  };

  // Mnemonic level names
  static constexpr auto trace    = spdlog::level::trace;
  static constexpr auto debug    = spdlog::level::debug;
  static constexpr auto info     = spdlog::level::info;
  static constexpr auto warn     = spdlog::level::warn;
  static constexpr auto err      = spdlog::level::err;
  static constexpr auto critical = spdlog::level::critical;
  static constexpr auto off      = spdlog::level::off;

  void init_fallback();
  void init_from_json(const std::string& config_path = def_log_cfg_path);

  void init_raw(std::string_view          app_name        = "app",
                mode                      m               = mode::sync,
                spdlog::level::level_enum console_lvl     = is_debug_build() ? info : warn,
                spdlog::level::level_enum file_lvl        = is_debug_build() ? debug : trace,
                int                       rotation_hour   = 2,
                int                       rotation_minute = 0,
                int                       keep_days       = keep_days_default,
                std::string_view          pattern         = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v",
                std::string_view          log_folder      = "logs",
                spdlog::level::level_enum flush_lvl       = spdlog::level::warn);

  void set_console_level(spdlog::level::level_enum lvl);
  void set_file_level(spdlog::level::level_enum lvl);


  void log_exception_with_chain(const std::exception& e, spdlog::level::level_enum lvl = critical);
  void log_current_exception_with_chain(spdlog::level::level_enum lvl = critical);

  void setup_terminate_handler();
  void setup_signal_handler();

  // Delete copy/move
  log(const log&)                                = delete;
  log& operator=(const log&)                     = delete;
  log(log&&)                                     = delete;
  log&                          operator=(log&&) = delete;
  [[nodiscard]] spdlog::logger* get_internal() const { return spdlog::default_logger().get(); }
  // Backward compatibility
  static spdlog::logger* get() { return log::instance().get_internal(); }
private:
  log()             = default; // creates fallback logger
  ~log()            = default;
  bool initialized_ = false; // full construction when singleton is already constructed

  // Helper functions
  static spdlog::level::level_enum flush_level_from_string(const std::string& level);
  static spdlog::level::level_enum level_from_string(const std::string& str);
  static std::string               level_to_string(spdlog::level::level_enum level);

  // Signal handler must be static to be passed to std::signal
  static void        signal_handler(int sig);
  static const char* get_signal_name(int sig);
  void               log_backtrace(const std::string& title) const;
  void               log_nested_chain(const std::exception& e, int depth);

  // Member variables
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  std::shared_ptr<spdlog::logger> logger_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  std::string cfg_filename_;
};
