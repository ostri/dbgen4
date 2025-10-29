#include "log.hpp"
#include "build_type.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <magic_enum.hpp>
#include <memory>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/logger.h>

namespace
{
  // NOLINTNEXTLINE(readability-static-definition-in-anonymous-namespace)
  std::mutex mtx; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace
namespace dbgen4
{
  /// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
  sink_t log::sink_ = nullptr;
  log::log()
  : l(spd::get(log_name_))
  {
    try
    {
      //      std::cerr << "log konstruktor\n";
      if (! l) // log or reference to log does not exists
      {
        // NOLINTNEXTLINE(concurrency-mt-unsafe, misc-const-correctness)
        std::lock_guard<std::mutex> lock(mtx); // wait until be served
        l = spd::get(log_name_); // it is our turn. maybe somebody creaated log in between
        if (l) l->debug("log reference was established");
        else establish_log(); // no it didn't we should do it
      }
    }
    catch (const spd::spdlog_ex& e)
    {
      std::cerr << fmt::format("Can't initialize log '{}' reason: ", log_name_, e.what());
      throw;
    }
  };

  log::~log()
  {
    //    std::cerr << "log destruktor\n";
    spd::drop_all();
  }

  void log::set_sink_level(spd::level::level_enum level) const
  {
    auto ndx = find_sink();
    if (ndx >= 0) l->sinks()[ndx]->set_level(level);
    else l->error("no file log defined. Something is wrong.");
  }

  spd::level::level_enum log::get_sink_level() const
  {
    auto ndx = find_sink();
    if (ndx >= 0) { return l->sinks()[ndx]->level(); }
    l->error("no file log defined. Something is wrong.");
    return spd::level::level_enum::critical;
  }

  /// @brief establish the log
  void log::establish_log()
  {
    if (l != nullptr) return; // already established
    auto log_filename = fmt::format("{}.log", log_name_);
    auto console_sink = std::make_shared<spd::sinks::stdout_color_sink_mt>();
    auto file_sink    = std::make_shared<spd::sinks::daily_file_sink_mt>(log_filename,
                                                                      2,
                                                                      0); // rotate 2:00 am
    if constexpr (is_debug_build())
    {
      console_sink->set_level(spd::level::info);
      file_sink->set_level(spd::level::trace);
    }
    else
    {
      console_sink->set_level(spd::level::err);
      file_sink->set_level(spd::level::info);
    }

    l = std::make_shared<spd::logger>(log_name_, spd::sinks_init_list{file_sink, console_sink});
    spd::register_logger(l);
    if constexpr (is_debug_build()) l->set_level(spd::level::trace);
    else l->set_level(spd::level::info);
    auto level = l->level();
    l->info("Logger '{}' is successfully initialized at '{}' level build type '{}'.",
            log_name_,
            magic_enum::enum_name(level),
            build_type_name());
  }

  int log::find_sink() const
  {
    // sink_t sink;
    auto cnt = 0;
    for (const auto& tmp : l->sinks())
    {
      // NOLINTNEXTLINE(hicpp-use-auto, modernize-use-auto)
      spd::sinks::daily_file_sink_mt* x = dynamic_cast<spd::sinks::daily_file_sink_mt*>(tmp.get());
      if (x != nullptr) return cnt;
      cnt++;
    }
    return -1;
  }

}; // namespace dbgen4