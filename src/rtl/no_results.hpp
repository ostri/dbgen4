// no_results.hpp
#pragma once
#include "result_root.hpp"

namespace rtl
{
  /// result buffer of a statement that returns no rows
  class no_results : public result_root
  {
  public:
    [[nodiscard]] span_buffer_dscr_init buffer_description_init() const override { return {}; }
  };
} // namespace rtl
