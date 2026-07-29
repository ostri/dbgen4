// no_params.hpp
#pragma once
#include "buffer_dscr.hpp"
#include "parameter_root.hpp"

namespace rtl
{
  /// parameter buffer of a statement that takes no parameters
  class no_params : public parameter_root
  {
  public:
    [[nodiscard]] span_buffer_dscr_init buffer_description_init() const override { return {}; }
    void                                reset_all_null() noexcept override { }
  };
} // namespace rtl
