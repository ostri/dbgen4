// log.h
#pragma once

#include "build_type.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>

#include <stacktrace>
#include <exception>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>

namespace fs                = std::filesystem;
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


  /* -------------------------------------------------------------
     Initialize from JSON config file
     ------------------------------------------------------------- */
  static spdlog::level::level_enum flush_level_from_string(const std::string& level)
  {
    if (level == "trace") return spdlog::level::trace;
    if (level == "debug") return spdlog::level::debug;
    if (level == "info") return spdlog::level::info;
    if (level == "warn") return spdlog::level::warn;
    if (level == "err" || level == "error") return spdlog::level::err;
    if (level == "critical") return spdlog::level::critical;
    if (level == "off") return spdlog::level::off;
    return spdlog::level::err; // default: err
  }
  static void init_fallback()
  {
    init_raw(std::string("fallback.") + build_type_name(),
             mode::sync,
             is_debug_build() ? spdlog::level::info : spdlog::level::warn,
             is_debug_build() ? spdlog::level::debug : spdlog::level::info,
             0,
             0,
             3,
             "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v",
             "./logs",
             spdlog::level::warn // default flush_on
    );
    get()->warn("Fallback log {} created to provide at least basic logging.",
                fs::absolute(fs::path(def_log_path)).string());
  }
  static void init_from_json(const std::string& config_path = def_log_cfg_path)
  {
    auto cfg_filename = config_path;
    if (cfg_filename.empty()) cfg_filename = def_log_cfg_path;

    /// make the path absolute
    auto absolute = fs::absolute(fs::path(cfg_filename));
    cfg_filename  = absolute.string();

    /// open configuration file or fail
    std::ifstream file(cfg_filename);
    if (! file.is_open())
    {
      init_fallback();
      get()->warn("Failed to open '{}' log config file.", cfg_filename);
      return;
    }

    /// read the json log configuration file
    nlohmann::json j;
    try
    {
      file >> j;
    }
    catch (const std::exception& e)
    {
      init_fallback();
      get()->warn(
        "Syntax of '{}' log config file is broken or missing. {}", cfg_filename, e.what());
      return;
    }

    try
    {
      auto app_name    = j.value("app_name", "app");
      auto m           = (j.value("mode", "sync") == "async") ? mode::async : mode::sync;
      auto console_lvl = level_from_string(j.value("console_level", "warn"));
      auto file_lvl    = level_from_string(j.value("file_level", "trace"));
      auto rot_h       = j.value("rotation_hour", 2);
      auto rot_m       = j.value("rotation_minute", 0);
      auto keep_days   = j.value("keep_days", keep_days_default);
      auto pattern     = j.value("pattern", "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v");
      auto log_folder  = j.value("log-folder", "./logs");
      auto flush_str   = j.value("flush_on", "warn");
      auto flush_lvl   = flush_level_from_string(flush_str);

      init_raw(app_name,
               m,
               console_lvl,
               file_lvl,
               rot_h,
               rot_m,
               keep_days,
               pattern,
               log_folder,
               flush_lvl);
    }
    catch (const std::exception& e)
    {
      init_fallback();
      get()->warn("Error parsing log config: {}. fallback activated", e.what());
      throw;
    }
  }

  /* -------------------------------------------------------------
     Raw init (used internally and for direct calls)
     ------------------------------------------------------------- */

  /**
   * @brief initialize logger when all parameters are know
   *
   * @param app_name    name of the application. should be short since it is written in every log
   * line
   * @param m           type of the logger (sync, async)
   * @param console_lvl logging level on console log
   * @param file_lvl    logging level on file log
   * @param rotation_hour when rotate file log (0-23) hour
   * @param rotation_minute when rotate file log minute
   * @param keep_days       how many days the logs are kept
   * @param pattern         the pattern of the log line
   * @param log_folder      where the logs are kept
   * @param flush_lvl       log message level that causes immediate flush
   */
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
    spdlog::level::level_enum flush_lvl  = spdlog::level::warn)
  {
    auto            log_folder_abs = fs::absolute(fs::path(log_folder)).string();
    std::error_code ec;
    auto            log_folder_created = fs::create_directories(log_folder_abs, ec);
    ///< console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(console_lvl);
    ///< filesink
    auto log_filename = fmt::format("{}/{}.log", log_folder_abs, app_name);
    auto file_sink    = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
      log_filename, rotation_hour, rotation_minute, true, keep_days);
    file_sink->set_level(file_lvl);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

    std::shared_ptr<spdlog::logger> logger;
    const auto                      log_buffer_size = 8192;
    if (m == mode::async)
    {
      spdlog::init_thread_pool(log_buffer_size, 1);
      logger =
        std::make_shared<spdlog::async_logger>(std::string(app_name),
                                               sinks.begin(),
                                               sinks.end(),
                                               spdlog::thread_pool(),
                                               spdlog::async_overflow_policy::overrun_oldest);
    }
    else
    {
      logger = std::make_shared<spdlog::logger>(std::string(app_name), sinks.begin(), sinks.end());
    }

    spdlog::set_default_logger(logger);
    spdlog::set_pattern(std::string(pattern));
    get()->flush_on(flush_lvl);

    console_sink_     = console_sink;
    file_sink_        = file_sink;
    const auto* debug = std::getenv("LOG_DEBUG"); // NOLINT(concurrency-mt-unsafe)
    if (debug != nullptr)
    {
      get()->info(
        "logger {} initialized. Log folders created {}", log_filename, log_folder_created);
      get()->info("log level: {} console: {} file: {} param: {} {} log folders created",
                  level_to_string(get()->level()),
                  level_to_string(console_sink_->level()),
                  level_to_string(file_sink_->level()),
                  level_to_string(console_lvl),
                  level_to_string(file_lvl));
    }
    if (log_folder_created) get()->info("Log folder {} created.", log_folder_abs);
    if (ec)
    {
      console_sink_->set_level(spdlog::level::level_enum::debug);
      get()->warn("Program was unable to create folders {} for logs. Logging only to console only.",
                  log_folder_abs);
    }
  }

  /* -------------------------------------------------------------
     Change console/file log level at runtime
     ------------------------------------------------------------- */
  static void set_console_level(spdlog::level::level_enum lvl)
  {
    if (console_sink_) console_sink_->set_level(lvl);
  }

  static void set_file_level(spdlog::level::level_enum lvl)
  {
    if (file_sink_) file_sink_->set_level(lvl);
  }

  /* -------------------------------------------------------------
     Access the logger – usage: log::get()->info("...")
     ------------------------------------------------------------- */
  // NOLINTNEXTLINE(readability-redundant-inline-specifier)
  inline static spdlog::logger* get() { return spdlog::default_logger().get(); }

  /* -------------------------------------------------------------
     Log exception with backtrace and nested cause chain
     ------------------------------------------------------------- */
  static void log_exception_with_chain(const std::exception&     e,
                                       spdlog::level::level_enum lvl = critical)
  {
    std::ostringstream oss;
    oss << "EXCEPTION: " << e.what() << "\nBACKTRACE:\n";
    for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
    oss << "CAUSE CHAIN:\n";
    get()->log(lvl, "{}", oss.str());
    log_nested_chain(e, 1);
  }

  static void log_current_exception_with_chain(spdlog::level::level_enum lvl = critical)
  {
    if (auto ex = std::current_exception())
    {
      try
      {
        std::rethrow_exception(ex);
      }
      catch (const std::exception& e)
      {
        log_exception_with_chain(e, lvl);
      }
      catch (...)
      {
        get()->log(lvl, "Unknown exception (not std::exception)");
      }
    }
  }

  /* -------------------------------------------------------------
     std::terminate handler
     ------------------------------------------------------------- */
  static void setup_terminate_handler()
  {
    std::set_terminate(
      []()
      {
        if (auto ex = std::current_exception())
        {
          try
          {
            std::rethrow_exception(ex);
          }
          catch (const std::exception& e)
          {
            log_exception_with_chain(e);
          }
          catch (...)
          {
            get()->critical("UNKNOWN EXCEPTION – terminating!");
          }
        }
        else { get()->critical("std::terminate() called without exception"); }

        std::ostringstream oss;
        oss << "BACKTRACE at terminate():\n";
        for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
        get()->critical("{}", oss.str());

        spdlog::shutdown();
        std::abort();
      });
  }

  /* -------------------------------------------------------------
     Signal handler – clean, readable, no nested ternaries
     ------------------------------------------------------------- */
  static void setup_signal_handler()
  {
    const std::array<int, 5> signals = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGTERM};
    // NOLINTNEXTLINE(cert-err33-c)
    for (int sig : signals) { std::signal(sig, signal_handler); }
  }
private:
  // Dedicated signal handler function
  static void signal_handler(int sig)
  {
    const char* name = get_signal_name(sig);
    get()->critical("SIGNAL {} ({}) – application terminating!", name, sig);

    log_backtrace("BACKTRACE at signal:");

    spdlog::shutdown();
    // NOLINTNEXTLINE(concurrency-mt-unsafe, readability-magic-numbers)
    std::exit(128 + sig); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
  }

  // Helper: get human-readable signal name
  static const char* get_signal_name(int sig)
  {
    switch (sig)
    {
    case SIGSEGV: return "SIGSEGV";
    case SIGABRT: return "SIGABRT";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGTERM: return "SIGTERM";
    default: return "UNKNOWN";
    }
  }

  // Helper: log current stack trace
  static void log_backtrace(const std::string& title)
  {
    std::ostringstream oss;
    oss << title << "\n";
    for (const auto& entry : std::stacktrace::current()) { oss << "  " << entry << "\n"; }
    get()->critical("{}", oss.str());
  }
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink_;

  // Helper: convert string to spdlog level
  static spdlog::level::level_enum level_from_string(const std::string& str)
  {
    if (str == "trace") return trace;
    if (str == "debug") return debug;
    if (str == "info") return info;
    if (str == "warn") return warn;
    if ((str == "err") || (str == "error")) return err;
    if (str == "critical") return critical;
    if (str == "off") return off;
    auto msg = fmt::format("Unknown keyword '{}' file: {} line: {}", str, __FILE_NAME__, __LINE__);
    throw std::runtime_error(msg);
    return info;
  }

  // Helper: convert spdlog level to string
  static std::string level_to_string(spdlog::level::level_enum level)
  {
    switch (level)
    {
    case spdlog::level::trace: return "trace";
    case spdlog::level::debug: return "debug";
    case spdlog::level::info: return "info";
    case spdlog::level::warn: return "warn";
    case spdlog::level::err: return "err";
    case spdlog::level::critical: return "critical";
    case spdlog::level::off: return "off";
    case spdlog::level::n_levels: // Ta raven je samo za štetje, ne smemo je doseči
    default:
      // Spdlog privzeto vrne ime ravni (npr. "unknown"),
      // vendar je za preprečevanje napak bolje podati znano raven.
      return "unknown";
    }
  }
  // Recursively log nested exceptions
  // NOLINTNEXTLINE(misc-no-recursion)
  static void log_nested_chain(const std::exception& e, int depth)
  {
    try
    {
      std::rethrow_if_nested(e);
    }
    catch (const std::exception& nested)
    {
      std::ostringstream oss;
      oss << std::string(depth * 2UL, ' ') << "└─ " << nested.what() << "\n";
      get()->critical("{}", oss.str());
      log_nested_chain(nested, depth + 1);
    }
    catch (...)
    {
      get()->critical("{}  └─ [unknown nested exception]", std::string((depth + 1UL) * 2, ' '));
    }
  }
};