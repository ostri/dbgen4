#include "logger_impl.hpp" // IWYU pragma: keep
#include <sys/stat.h>

namespace rtl
{
  class logger& logger::instance() { return logger::impl::instance(); }
  void          logger::init_from_json(const std::string& config_path) { pimpl_->init_from_json(config_path); }
  void          logger::set_level(enum level l)
  {
    if (pimpl_) pimpl_->set_level(l);
  };
  void logger::log_exception_with_chain(const std::exception& e, enum level l)
  {
    if (pimpl_) pimpl_->log_exception_with_chain(e, l);
  }
  void logger::log_current_exception_with_chain(enum level l)
  {
    if (pimpl_) pimpl_->log_current_exception_with_chain(l);
  }
  void logger::setup_terminate_handler()
  {
    if (pimpl_) pimpl_->setup_terminate_handler();
  }
  void logger::setup_signal_handler()
  {
    if (pimpl_) pimpl_->setup_signal_handler();
  }
  void logger::log_backtrace(const std::string& title)
  {
    if (pimpl_) logger::impl::log_backtrace(title);
  }
  enum logger::level logger::level() const { return pimpl_ ? pimpl_->level() : logger::level::off; }
  void               logger::_log(enum logger::level l, std::string_view s) { pimpl_->_log(l, s); }
  enum logger::level logger::console_level() const { return pimpl_ ? pimpl_->level() : logger::level::off; }
  enum logger::level logger::file_level() const { return pimpl_ ? pimpl_->level() : logger::level::off; }
  class logger*      logger::get() noexcept { return logger::impl::get(); }
  void               logger::flush() { pimpl_->flush(); }
} // namespace rtl
