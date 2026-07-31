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
    [[nodiscard]] span_buffer_dscr_init buffer_description_init() override;
    [[nodiscard]] size_t                buffer_size() const noexcept override;
  protected:
    void resize_storage(size_t) override;
    void reset_all_null() noexcept override;
  };
  //////////////////////////////////////////////////////////////////////////////////
  inline span_buffer_dscr_init no_params::buffer_description_init() { return {}; }
  inline size_t                no_params::buffer_size() const noexcept { return 0; }
  inline void                  no_params::resize_storage(size_t) { }
  inline void                  no_params::reset_all_null() noexcept { }
} // namespace rtl
