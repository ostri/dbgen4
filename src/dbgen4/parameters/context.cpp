#include "context.hpp"

namespace dbgen4
{
  const cmd_line_params& context::cmd() const { return cmd_; }

  // inja::Environment context::env() const { return env_; }

  // const map_inja_tpl& context::templates() const { return templates_; }

}; // namespace dbgen4
