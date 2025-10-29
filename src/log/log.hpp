// Log.h
#pragma once

#include <cstdint>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>
#include <memory>
#include <string_view>
// #include <chrono>

class log
{
public:
  enum class Mode : uint8_t
  {
    Sync,
    Async
  };

  static void init(std::string_view          app_name      = "app",
                   Mode                      mode          = Mode::Sync,
                   spdlog::level::level_enum console_level = spdlog::level::warn // privzeto: warn+
  );

  static void set_console_level(spdlog::level::level_enum level);
  static void set_file_level(spdlog::level::level_enum level);

  static spdlog::logger* get();
private:
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
  // NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  inline static std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink_;
};