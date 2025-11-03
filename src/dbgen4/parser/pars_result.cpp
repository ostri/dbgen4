#include "pars_result.hpp"

namespace dbgen4
{

  pars_result::pars_result(parser_err_enum e)
  : pars_result({}, e) { };

  pars_result::pars_result(data_statements s, parser_err_enum e)
  : s_(std::move(s))
  , e_(e) { };

  parser_err_enum pars_result::e() const { return e_; }

  data_statements pars_result::s() const { return s_; }

  spdlog::logger* pars_result::log() const { return log::get(); }

} // namespace dbgen4
