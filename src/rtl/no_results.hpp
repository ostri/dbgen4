// no_results.hpp
#pragma once
#include "result_root.hpp"

namespace rtl
{
  /// result buffer of a statement that returns no rows
  class no_results : public result_root
  {
  public:
    [[nodiscard]] span_buffer_dscr_init buffer_description_init() override { return {}; }
    /// no columns, so no rows either - and nothing to resize
    [[nodiscard]] size_t                buffer_size() const noexcept override { return 0; }
  protected:
    void                                resize_storage(size_t) override { }
  };
} // namespace rtl
