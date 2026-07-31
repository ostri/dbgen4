#include "logger_impl.hpp"
#include <csignal>
#include <spdlog/async.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <stacktrace>
#include <sys/stat.h>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
namespace rtl
{
  namespace
  {
    /* -------------------------------------------------------------
       Helper functions
       ------------------------------------------------------------- */
    [[maybe_unused]] enum logger::level level_from_string(const std::string& str)
    {
      if (str == "trace") return logger::level::trace;
      if (str == "debug") return logger::level::debug;
      if (str == "info") return logger::level::info;
      if (str == "warn") return logger::level::warn;
      if ((str == "err") || (str == "error")) return logger::level::error;
      if (str == "critical") return logger::level::critical;
      if (str == "off") return logger::level::off;
      auto msg = fmt::format("Unknown keyword '{}' file: {} line: {}", str, __FILE_NAME__, __LINE__);
      throw std::runtime_error(msg);
    }

    [[maybe_unused]] std::string level_to_string(enum logger::level level)
    {
      switch (level)
      {
      case logger::level::trace: return "trace";
      case logger::level::debug: return "debug";
      case logger::level::info: return "info";
      case logger::level::warn: return "warn";
      case logger::level::error: return "err";
      case logger::level::critical: return "critical";
      case logger::level::off: return "off";
      default: return "unknown";
      }
    }
  } // namespace
  // clang-format off
  enum logger::level logger::impl::console_level() const { return console_sink_ ? static_cast<enum logger::level>(console_sink_->level()) : logger::level::off; };
  enum logger::level logger::impl::file_level() const    { return file_sink_ ?    static_cast<enum logger::level>(file_sink_->level()) : logger::level::off; };
  enum logger::level logger::impl::level() const         { return logger_ ?       static_cast<enum logger::level>(logger_->level()) : logger::level::off; };
  // clang-format on
  void logger::impl::set_console_level(enum logger::level l)
  {
    if (console_sink_) console_sink_->set_level(static_cast<spdlog::level::level_enum>(l));
  };

  void logger::impl::set_file_level(enum logger::level l)
  {
    if (file_sink_) file_sink_->set_level(static_cast<spdlog::level::level_enum>(l));
  };

  void logger::impl::set_level(enum logger::level l)
  {
    if (logger_) logger_->set_level(static_cast<spdlog::level::level_enum>(l));
  };
  // clang-format off
  void logger::impl::flush() { if (logger_) logger_->flush(); }
  void logger::impl::flush_on(enum level l) {if (logger_) logger_->flush_on(static_cast<spdlog::level::level_enum>(l));}
  // clang-format on
  void logger::impl::log_exception_with_chain(const std::exception& e, enum logger::level lvl)
  {
    std::ostringstream oss;
    oss << "EXCEPTION: " << e.what() << "\nBACKTRACE:\n";
    for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
    oss << "CAUSE CHAIN:\n";
    auto msg = fmt::format("{}", oss.str());
    _log(lvl, msg);
    log_nested_chain(e, 1);
  }

  void logger::impl::log_current_exception_with_chain(enum level lvl)
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
        _log(lvl, "Unknown exception (not std::exception)");
      }
    }
  }

  void logger::impl::log_nested_chain(const std::exception& e, int depth) // NOLINT(misc-no-recursion)
  {
    try
    {
      std::rethrow_if_nested(e);
    }
    catch (const std::exception& nested)
    {
      std::ostringstream oss;
      oss << std::string(depth * 2UL, ' ') << "└─ " << nested.what() << "\n";
      auto msg = fmt::format("{}", oss.str());
      _log(logger::level::critical, msg);
      log_nested_chain(nested, depth + 1);
    }
    catch (...)
    {
      auto msg = fmt::format("{}  └─ [unknown nested exception]", std::string((depth + 1UL) * 2, ' '));
      _log(logger::level::critical, msg);
    }
  }
  void logger::impl::setup_terminate_handler()
  {
    std::set_terminate(
      []()
      {
        auto& self = logger::instance();

        if (auto e = std::current_exception())
        {
          try
          {
            std::rethrow_exception(e);
          }
          catch (const std::exception& e)
          {
            self.log_exception_with_chain(e);
          }
          catch (...)
          {
            get()->critical("UNKNOWN EXCEPTION – terminating!");
          }
        }
        else
        {
          get()->critical("std::terminate() called without exception");
        }

        std::ostringstream oss;
        oss << "BACKTRACE at terminate():\n";
        for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
        get()->critical("{}", oss.str());

        spdlog::shutdown();
        std::abort();
      });
  }

  void logger::impl::setup_signal_handler()
  {
    const std::array<int, 5> signals = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGTERM};
    // NOLINTNEXTLINE(cert-err33-c)
    for (const int sig : signals) { std::signal(sig, logger::impl::signal_handler); }
  }

  void logger::impl::signal_handler(int sig)
  {
    const char* name = get_signal_name(sig);
    get()->critical("SIGNAL {} ({}) – application terminating!", name, sig);
    instance().log_backtrace("BACKTRACE at signal:");
    // NOLINTNEXTLINE(concurrency-mt-unsafe, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    std::exit(128 + sig);
  }

  const char* logger::get_signal_name(int sig)
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

  void logger::impl::log_backtrace(const std::string& title)
  {
    std::ostringstream oss;
    oss << title << "\n";
    for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
    get()->critical("{}", oss.str());
  }

  void logger::impl::init_fallback()
  {
    init_raw(std::string("fallback.") + build_type_name(),
             mode::sync,
             is_debug_build() ? logger::level::info : logger::level::warn,
             is_debug_build() ? logger::level::debug : logger::level::info,
             0,
             0,
             3,
             "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v",
             "./logs",
             logger::level::warn);

    get()->warn("Fallback log {} created to provide at least basic logging.", fs::absolute(fs::path(def_logger_path)).string());
  }
  void logger::impl::init_from_json(const std::string& config_path)
  {
    auto cfg_filename = config_path;
    if (cfg_filename.empty()) cfg_filename = def_logger_cfg_path;

    auto absolute = fs::absolute(fs::path(cfg_filename));
    cfg_filename  = absolute.string();

    std::ifstream file(cfg_filename);
    if (! file.is_open())
    {
      init_fallback();
      get()->warn("Failed to open '{}' log config file.", cfg_filename);
      return;
    }
    cfg_filename_ = cfg_filename;

    nlohmann::json j;
    try
    {
      file >> j;
    }
    catch (const std::exception& e)
    {
      init_fallback();
      get()->warn("Syntax of '{}' log config file is broken or missing. {}", cfg_filename, e.what());
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
      auto keep_days   = j.value("keep_days", logger_keep_days_default);
      auto pattern     = j.value("pattern", "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v");
      auto log_folder  = j.value("log_folder", "./logs");
      auto flush_str   = j.value("flush_on", "warn");
      auto flush_lvl   = level_from_string(flush_str);

      init_raw(app_name, m, console_lvl, file_lvl, rot_h, rot_m, keep_days, pattern, log_folder, flush_lvl);
    }
    catch (const std::exception& e)
    {
      init_fallback();
      get()->warn("Error parsing log config: {}. fallback activated", e.what());
      throw;
    }
  }


  void logger::impl::init_raw(std::string_view app_name,
                              mode             m,
                              enum level       console_lvl,
                              enum level       file_lvl,
                              int              rotation_hour,
                              int              rotation_minute,
                              int              keep_days,
                              std::string_view pattern,
                              std::string_view log_folder,
                              enum level       flush_lvl)
  {
    auto log_folder_abs     = fs::absolute(fs::path(log_folder)).string();
    bool log_folder_created = false;

    if (! fs::exists(log_folder_abs))
    {
      // NOLINTNEXTLINE(concurrency-mt-unsafe, readability-magic-numbers)
      auto sts = mkdir(log_folder_abs.c_str(), 0755); /// NOLINT
      if (sts != 0) throw std::runtime_error(fmt::format("Can't create folder '{}'", log_folder_abs));
      log_folder_created = true;
    }

    console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    set_console_level(console_lvl);

    auto log_filename = fmt::format("{}/{}.log", log_folder_abs, app_name);
    file_sink_        = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, rotation_hour, rotation_minute, true, keep_days);
    set_file_level(file_lvl);

    std::vector<spdlog::sink_ptr> sinks{console_sink_, file_sink_};

    if (m == mode::async)
    {
      spdlog::init_thread_pool(8192, 1); // NOLINT
      logger_ = std::make_shared<spdlog::async_logger>(
        std::string(app_name), sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
    }
    else
    {
      logger_ = std::make_shared<spdlog::logger>(std::string(app_name), sinks.begin(), sinks.end());
    }

    spdlog::set_default_logger(logger_);
    spdlog::set_pattern(std::string(pattern));
    flush_on(flush_lvl);
    set_level(std::min(console_lvl, file_lvl));

    const auto* debug = std::getenv("LOG_DEBUG"); // NOLINT(concurrency-mt-unsafe)
    if (debug != nullptr)
    {
      get()->info(R"(
logger initialized
logger build type {}
log config file   {}
log file          {}
log folder:       {}
log path:         {}
log level:
  logger:         {}
  console:        {}
  file:           {}
provided parameters :
  logger (min)    {}
  console:        {}
  file            {}
)",
                  build_type_name(),
                  cfg_filename_,
                  log_filename,
                  (log_folder_created ? "created" : "existing"),
                  log_folder_abs,
                  level_to_string(level()),
                  level_to_string(console_level()),
                  level_to_string(file_level()),
                  level_to_string(std::min(console_level(), file_level())),
                  level_to_string(console_lvl),
                  level_to_string(file_lvl));
    }
  }

  // Meyers' Singleton – thread-safe, lazy initialization
  class rtl::logger& logger::impl::instance() noexcept
  {
    static logger singleton_;
    try
    {
      if (! singleton_.pimpl_ || singleton_.pimpl_->cfg_filename_.empty())
      {
        if (! singleton_.pimpl_) singleton_.pimpl_ = std::make_unique<impl>();
        const auto* config_file = std::getenv("LOG_CONFIG"); // NOLINT(concurrency-mt-unsafe)
        singleton_.init_from_json(config_file != nullptr ? config_file : "");
        singleton_.setup_terminate_handler();
        singleton_.setup_signal_handler();
      }
    }
    catch (...) // NOLINT(bugprone-empty-catch)
    {
    }
    return singleton_;
  }

  void logger::impl::_log(enum logger::level l, std::string_view s) { logger_->log(static_cast<spdlog::level::level_enum>(l), s); }
} // namespace rtl
