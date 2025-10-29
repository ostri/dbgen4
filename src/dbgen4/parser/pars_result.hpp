#pragma once
#include "data_statements.hpp"
#include "parser_errors.hpp"
namespace dbgen4
{

  class pars_result
  {
  public:
    explicit pars_result(parser_err_enum e);
    pars_result(const data_statements& s, parser_err_enum e);
    virtual ~pars_result()                                          = default;
    pars_result(const pars_result&)                                 = delete;
    pars_result(pars_result&&) noexcept                             = delete;
    pars_result&                  operator=(const pars_result&)     = delete;
    pars_result&                  operator=(pars_result&&) noexcept = delete;
    [[nodiscard]] parser_err_enum e() const;
    [[nodiscard]] data_statements s() const;
  protected:
    // NOLINTNEXTLINE(cert-err58-cpp)
    inline static const auto log = log::get();
  private:
    data_statements s_;                      ///< parsed statements
    parser_err_enum e_{parser_err_enum::ok}; ///< error code of the parsing operation
  };
}; // namespace dbgen4