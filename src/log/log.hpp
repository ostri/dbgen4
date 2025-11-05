// log.h
#pragma once
#ifndef SPDLOG_ACTIVE_LEVEL
#  define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "build_type.hpp"
// #include <filesystem>
#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>

#include <exception>
// #include <cstdlib>
#include <memory>
#include <string_view>
#include <nlohmann/json.hpp>
// #include <system_error>

const int keep_days_default = 7; ///< how long we keep the logs by default
// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members, cert-err58-cpp)
constexpr const char* def_log_cfg_path =
  is_debug_build() ? "config/log.debug.conf" : "config/log.release.conf";
constexpr const char* def_log_path =
  is_debug_build() ? "logs/fallback.debug.log" : "logs/fallback.release.log";
class log
{
public:
  enum class mode : uint8_t
  {
    sync,
    async
  };

  // Mnemonic level names (above 3)
  static constexpr auto trace    = spdlog::level::trace;
  static constexpr auto debug    = spdlog::level::debug;
  static constexpr auto info     = spdlog::level::info;
  static constexpr auto warn     = spdlog::level::warn;
  static constexpr auto err      = spdlog::level::err;
  static constexpr auto critical = spdlog::level::critical;
  static constexpr auto off      = spdlog::level::off;


  static spdlog::level::level_enum flush_level_from_string(const std::string& level);
  static void                      init_fallback();
  static void init_from_json(const std::string& config_path = def_log_cfg_path);

  static void init_raw(
    std::string_view          app_name        = "app",
    mode                      m               = mode::sync,
    spdlog::level::level_enum console_lvl     = is_debug_build() ? info : warn,
    spdlog::level::level_enum file_lvl        = is_debug_build() ? debug : trace,
    int                       rotation_hour   = 2,
    int                       rotation_minute = 0,
    int keep_days = 7, // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    std::string_view          pattern    = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v",
    std::string_view          log_folder = "logs",
    spdlog::level::level_enum flush_lvl  = spdlog::level::warn);

  static void set_console_level(spdlog::level::level_enum lvl);

  static void set_file_level(spdlog::level::level_enum lvl);

  /* -------------------------------------------------------------
     Access the logger – usage: log::get()->info("...")
     ------------------------------------------------------------- */
  // NOLINTNEXTLINE(readability-redundant-inline-specifier)
  inline static spdlog::logger* get() { return spdlog::default_logger().get(); }

  static void log_exception_with_chain(const std::exception&     e,
                                       spdlog::level::level_enum lvl = critical);

  static void log_current_exception_with_chain(spdlog::level::level_enum lvl = critical);

  static void setup_terminate_handler();

  static void setup_signal_handler();
private:
  static void                      signal_handler(int sig);
  static const char*               get_signal_name(int sig);
  static void                      log_backtrace(const std::string& title);
  static spdlog::level::level_enum level_from_string(const std::string& str);
  static std::string               level_to_string(spdlog::level::level_enum level);
  static void                      log_nested_chain(const std::exception& e, int depth);
  /// members ///
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::logger> logger_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::string cfg_filename_; ///< configuration filename
};