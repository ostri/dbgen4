#include "log.hpp"

// Inicializacija (kliče se enkrat na začetku aplikacije)
void log::init(std::string_view app_name, Mode mode, spdlog::level::level_enum console_level)
{
  const auto msg_max = 8192; /// maximum number of messages in buffer
  const auto keep    = 7;    /// keep last 7 log files
  // 1. Ustvarimo sink-e
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(console_level);

  // Dnevna datoteka: zamenja ob 02:00, hrani 7 dni
  auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
    std::string(app_name) + "_log", 2, 0, true, keep); // 02:00, rotacija 7 dni
  file_sink->set_level(spdlog::level::trace);          // vse v datoteko

  std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

  std::shared_ptr<spdlog::logger> logger;

  if (mode == Mode::Async)
  {
    // Async: thread pool (8192 sporočil, 1 nit)
    spdlog::init_thread_pool(msg_max, 1);
    logger = std::make_shared<spdlog::async_logger>(std::string(app_name),
                                                    sinks.begin(),
                                                    sinks.end(),
                                                    spdlog::thread_pool(),
                                                    spdlog::async_overflow_policy::overrun_oldest);
  }
  else
  {
    // Sync
    logger = std::make_shared<spdlog::logger>(std::string(app_name), sinks.begin(), sinks.end());
  }

  // Nastavimo globalni logger
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::trace); // globalno omogočimo vse
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] %v");

  // Shrani referenco na console sink za kasnejše spreminjanje
  console_sink_ = console_sink;
  // Shrani referenco na file sink za kasnejše spreminjanje
  file_sink_ = file_sink;
}

// Spremeni nivo izpisa na konzolo v teku
void log::set_console_level(spdlog::level::level_enum level)
{
  if (console_sink_) { console_sink_->set_level(level); }
}

// Spremeni nivo izpisa na konzolo v teku
void log::set_file_level(spdlog::level::level_enum level)
{
  if (file_sink_) { console_sink_->set_level(level); }
}

// Getter za logger (uporaba: Log::get()->info("...")
spdlog::logger* log::get() { return spdlog::default_logger().get(); }
