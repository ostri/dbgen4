#include "pars_result.hpp"

namespace dbgen4
{

  pars_result::pars_result(exit_status_enum e)
  : pars_result({}, e) { };

  pars_result::pars_result(const data_statements& s)
  : pars_result(s, exit_status_enum::ok) { };

  pars_result::pars_result(data_statements s, exit_status_enum e)
  : s_(std::move(s))
  , e_(e) { };

  exit_status_enum pars_result::e() const { return e_; }

  data_statements pars_result::s() const { return s_; }

  spdlog::logger* pars_result::log() const { return log::get(); }

} // namespace dbgen4
