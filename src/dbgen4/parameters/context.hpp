#pragma once
#include <fmt/base.h>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include <spdlog/logger.h>
#include "cmd_line_params.hpp"
namespace dbgen4
{
  class context
  {
  public:
    explicit context(const cmd_line_params& cmd)
    : cmd_(cmd)
    {
    }
    [[nodiscard]] const cmd_line_params& cmd() const;
  private:
    class log::log* log_() { return log::get(); };
    /// Member variables
    /// members
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const cmd_line_params& cmd_; ///< reference to parameters
  };
} // namespace dbgen4