#include <magic_enum.hpp>
#include "log.hpp"
#include <iostream>
#include <memory>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "spdlog/sinks/basic_file_sink.h" // IWYU pragma: export // support for basic file logging
#include "spdlog/sinks/daily_file_sink.h" // IWYU pragma: export // support for basic file logging

namespace spd = spdlog;
namespace dbgen4
{
  dbgen4::log::log()
  : l(spd::get(log_name_)) // fetch reference to existing log
  {
    try
    {
      if (! l) // log does not exist yet. Create it
      {
        auto log_filename = fmt::format("{}.log", log_name_);
        auto console_sink = std::make_shared<spd::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spd::level::critical);

        auto file_sink =
          std::make_shared<spd::sinks::daily_file_sink_mt>(log_filename, 2, 0); // rotate 2:00 am
        file_sink->set_level(spd::level::info);

        spd::sinks_init_list sink_list = {file_sink, console_sink};
        l = std::make_shared<spd::logger>(log_name_, sink_list.begin(), sink_list.end());
#ifndef NDEBUG // debug build - we are tracing till trace
        l->set_level(spd::level::debug);
#else // release build - we are tracing till info
        l->set_level(spd::level::warn);
#endif
        auto level = l->level();
        l->warn("Logger '{}' is successfully initialized at '{}' level class '{}'.",
                log_name_,
                magic_enum::enum_name(level),
                typeid(*this).name());
      }
    }
    catch (const spd::spdlog_ex& e)
    {
      std::cerr << "Can't initialize log " << e.what() << "\n";
      throw;
    }
  };

  dbgen4::log::~log() { spd::drop_all(); }

}; // namespace dbgen4