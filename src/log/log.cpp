#include "log_impl.hpp" // IWYU pragma: keep
#include <sys/stat.h>


class log& log::instance() { return log::impl::instance(); }
void       log::init_from_json(const std::string& config_path) { pimpl_->init_from_json(config_path); }
void       log::set_level(enum level l)
{
  if (pimpl_) pimpl_->set_level(l);
};
void log::log_exception_with_chain(const std::exception& e, enum level l)
{
  if (pimpl_) pimpl_->log_exception_with_chain(e, l);
}
void log::log_current_exception_with_chain(enum level l)
{
  if (pimpl_) pimpl_->log_current_exception_with_chain(l);
}
void log::setup_terminate_handler()
{
  if (pimpl_) pimpl_->setup_terminate_handler();
}
void log::setup_signal_handler()
{
  if (pimpl_) pimpl_->setup_signal_handler();
}
void log::log_backtrace(const std::string& title)
{
  if (pimpl_) log::impl::log_backtrace(title);
}
enum log::level log::level() const { return pimpl_ ? pimpl_->level() : log::level::off; }
void            log::_log(enum log::level l, std::string_view s) { pimpl_->_log(l, s); }
enum log::level log::console_level() const { return pimpl_ ? pimpl_->level() : log::level::off; }
enum log::level log::file_level() const { return pimpl_ ? pimpl_->level() : log::level::off; }
class log*      log::get() noexcept { return log::impl::get(); }
void            log::flush() { pimpl_->flush(); }
