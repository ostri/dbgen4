#include "log_dbgen4.hpp"
#include <iostream>

#include "spdlog/sinks/basic_file_sink.h" // IW YU pragma: export // support for basic file logging

namespace dbgen4
{
  dbgen4::log::log()
  : l(spdlog::get(log_name_)) // fetch reference to existing log
  {
    try
    {
      if (! l) // log does not exist yet. Create it
      {
        l = spdlog::basic_logger_mt(log_name_, std::string(log_name_) + ".log");
        l->info("Logger '{}' is successfully initialized.", log_name_);
      }
    }
    catch (const spdlog::spdlog_ex& e)
    {
      std::cerr << "Can't initialize log " << e.what() << "\n";
      throw;
    }
  };

  dbgen4::log::~log() { spdlog::drop_all(); }

  // std::shared_ptr<spdlog::logger> log::l() const { return l_; };
}; // namespace dbgen4