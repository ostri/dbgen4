#include "log_impl.hpp"
#include <csignal>
#include <spdlog/async.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <stacktrace>
#include <sys/stat.h>
#include <fstream>
// #include <utility>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
namespace
{
  /* -------------------------------------------------------------
     Helper functions
     ------------------------------------------------------------- */
  // [[maybe_unused]] enum log::level flush_level_from_string(const std::string& level)
  // {
  //   if (level == "trace") return log::level::trace;
  //   if (level == "debug") return log::level::debug;
  //   if (level == "info") return log::level::info;
  //   if (level == "warn") return log::level::warn;
  //   if (level == "err" || level == "error") return log::level::error;
  //   if (level == "critical") return log::level::critical;
  //   if (level == "off") return log::level::off;
  //   std::unreachable();
  // }

  [[maybe_unused]] enum log::level level_from_string(const std::string& str)
  {
    if (str == "trace") return log::level::trace;
    if (str == "debug") return log::level::debug;
    if (str == "info") return log::level::info;
    if (str == "warn") return log::level::warn;
    if ((str == "err") || (str == "error")) return log::level::error;
    if (str == "critical") return log::level::critical;
    if (str == "off") return log::level::off;
    auto msg = fmt::format("Unknown keyword '{}' file: {} line: {}", str, __FILE_NAME__, __LINE__);
    throw std::runtime_error(msg);
  }

  [[maybe_unused]] std::string level_to_string(enum log::level level)
  {
    switch (level)
    {
    case log::level::trace: return "trace";
    case log::level::debug: return "debug";
    case log::level::info: return "info";
    case log::level::warn: return "warn";
    case log::level::error: return "err";
    case log::level::critical: return "critical";
    case log::level::off: return "off";
    default: return "unknown";
    }
  }
} // namespace
// clang-format off
enum log::level log::impl::console_level() const { return console_sink_ ? static_cast<enum log::level>(console_sink_->level()) : log::level::off; };
enum log::level log::impl::file_level() const    { return file_sink_ ?    static_cast<enum log::level>(file_sink_->level()) : log::level::off; };
enum log::level log::impl::level() const         { return logger_ ?       static_cast<enum log::level>(logger_->level()) : log::level::off; };
// clang-format on
void log::impl::set_console_level(enum log::level l)
{
  if (console_sink_) console_sink_->set_level(static_cast<spdlog::level::level_enum>(l));
};

void log::impl::set_file_level(enum log::level l)
{
  if (file_sink_) file_sink_->set_level(static_cast<spdlog::level::level_enum>(l));
};

void log::impl::set_level(enum log::level l)
{
  if (logger_) logger_->set_level(static_cast<spdlog::level::level_enum>(l));
};
// clang-format off
void log::impl::flush() { if (logger_) logger_->flush(); }
void log::impl::flush_on(enum level l) {if (logger_) logger_->flush_on(static_cast<spdlog::level::level_enum>(l));}
// clang-format on
void log::impl::log_exception_with_chain(const std::exception& e, enum log::level lvl)
{
  std::ostringstream oss;
  oss << "EXCEPTION: " << e.what() << "\nBACKTRACE:\n";
  for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
  oss << "CAUSE CHAIN:\n";
  auto msg = fmt::format("{}", oss.str());
  _log(lvl, msg);
  log_nested_chain(e, 1);
}

void log::impl::log_current_exception_with_chain(enum level lvl)
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

void log::impl::log_nested_chain(const std::exception& e, int depth) // NOLINT(misc-no-recursion)
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
    _log(log::level::critical, msg);
    log_nested_chain(nested, depth + 1);
  }
  catch (...)
  {
    auto msg = fmt::format("{}  └─ [unknown nested exception]", std::string((depth + 1UL) * 2, ' '));
    _log(log::level::critical, msg);
  }
}
void log::impl::setup_terminate_handler()
{
  std::set_terminate(
    []()
    {
      auto& self = log::instance();

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
      else {
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

void log::impl::setup_signal_handler()
{
  const std::array<int, 5> signals = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGTERM};
  // NOLINTNEXTLINE(cert-err33-c)
  for (const int sig : signals) { std::signal(sig, log::impl::signal_handler); }
}

void log::impl::signal_handler(int sig)
{
  const char* name = get_signal_name(sig);
  get()->critical("SIGNAL {} ({}) – application terminating!", name, sig);
  instance().log_backtrace("BACKTRACE at signal:");
  // NOLINTNEXTLINE(concurrency-mt-unsafe, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
  std::exit(128 + sig);
}

const char* log::get_signal_name(int sig)
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

void log::impl::log_backtrace(const std::string& title)
{
  std::ostringstream oss;
  oss << title << "\n";
  for (const auto& entry : std::stacktrace::current()) oss << "  " << entry << "\n";
  get()->critical("{}", oss.str());
}

void log::impl::init_fallback()
{
  init_raw(std::string("fallback.") + build_type_name(),
           mode::sync,
           is_debug_build() ? log::level::info : log::level::warn,
           is_debug_build() ? log::level::debug : log::level::info,
           0,
           0,
           3,
           "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v",
           "./logs",
           log::level::warn);

  get()->warn("Fallback log {} created to provide at least basic logging.", fs::absolute(fs::path(def_log_path)).string());
}
void log::impl::init_from_json(const std::string& config_path)
{
  auto cfg_filename = config_path;
  if (cfg_filename.empty()) cfg_filename = def_log_cfg_path;

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
    auto keep_days   = j.value("keep_days", keep_days_default);
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


void log::impl::init_raw(std::string_view app_name,
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
  else {
    logger_ = std::make_shared<spdlog::logger>(std::string(app_name), sinks.begin(), sinks.end());
  }

  spdlog::set_default_logger(logger_);
  spdlog::set_pattern(std::string(pattern));
  // logger_->flush_on(flush_lvl);
  flush_on(flush_lvl);
  set_level(std::min(console_lvl, file_lvl));

  // console_sink_ = console_sink;
  // file_sink_    = file_sink;

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

/* -------------------------------------------------------------
   Runtime level changes
   ------------------------------------------------------------- */
// void log::impl::set_console_level(log::level lvl)
// {
//   if (console_sink_)
//   {
//     console_sink_->set_level(static_cast<spdlog::level::level_enum>(lvl));
//     logger_->set_level(std::min(console_sink_->level(), file_sink_->level()));
//   }
// }

// void log::impl::set_file_level(log::level lvl)
// {
//   if (file_sink_)
//   {
//     file_sink_->set_level(static_cast<spdlog::level::level_enum>(lvl));
//     logger_->set_level(std::min(console_sink_->level(), file_sink_->level()));
//   }
// }
// Meyers' Singleton – thread-safe, lazy initialization
class ::log& log::impl::instance()
{
  static log singleton_;
  if (! singleton_.pimpl_ || singleton_.pimpl_->cfg_filename_.empty())
  {
    if (! singleton_.pimpl_) singleton_.pimpl_ = std::make_unique<impl>();
    const auto* config_file = std::getenv("LOG_CONFIG"); // NOLINT(concurrency-mt-unsafe)
    singleton_.init_from_json(config_file != nullptr ? config_file : "");
    singleton_.setup_terminate_handler();
    singleton_.setup_signal_handler();
  }
  return singleton_;
}

/* -------------------------------------------------------------
   Initialization
   ------------------------------------------------------------- */
// void log::impl::init_fallback()
// {
//   if (pimpl_) pimpl_->init_fallback();
// }
// void log::impl::init_fallback()
// {
//   init_raw(std::string("fallback.") + build_type_name(),
//            mode::sync,
//            is_debug_build() ? log::level::info : log::level::warn,
//            is_debug_build() ? log::level::debug : log::level::info,
//            0,
//            0,
//            3,
//            "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v",
//            "./logs",
//            log::level::warn);

//   get()->warn("Fallback log {} created to provide at least basic logging.", fs::absolute(fs::path(def_log_path)).string());
// }
void log::impl::_log(enum log::level l, std::string_view s) { logger_->log(static_cast<spdlog::level::level_enum>(l), s); }
