// parameter_root.cpp
#include "parameter_root.hpp"
namespace rtl
{

  void parameter_root::set_buffer_size(size_t rows)
  {
    if (rows == 0) rows = 1;
    resize_storage(rows);
    ++layout_generation_;
  }
  bool                parameter_root::is_batch() const noexcept { return buffer_size() > 1; }
  uint64_t            parameter_root::layout_generation() const noexcept { return layout_generation_; }
  std::span<uint16_t> parameter_root::row_status() noexcept { return {}; }


} // namespace rtl
