#pragma once
#include "data_statements.hpp"
#include "parser_errors.hpp"
namespace dbgen4
{

  constexpr size_t ALIGN_128 = 128;
  struct pars_result
  {
  public:
    explicit pars_result(parser_err_enum e);
    explicit pars_result(const data_statements& s)
    : pars_result(s, parser_err_enum::ok) { };
    pars_result(data_statements s, parser_err_enum e);
    virtual ~pars_result()                                          = default;
    pars_result(const pars_result&)                                 = default;
    pars_result(pars_result&&) noexcept                             = default;
    pars_result&                  operator=(const pars_result&)     = default;
    pars_result&                  operator=(pars_result&&) noexcept = default;
    [[nodiscard]] parser_err_enum e() const;
    [[nodiscard]] data_statements s() const;
    [[nodiscard]] spdlog::logger* log() const;
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    data_statements s_;                      ///< parsed statements NOLINT
    parser_err_enum e_{parser_err_enum::ok}; ///< error code of the parsing operation NOLINT
    // NOLINTEND
  } __attribute__((aligned(ALIGN_128)));
}; // namespace dbgen4