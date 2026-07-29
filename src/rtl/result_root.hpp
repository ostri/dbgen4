// result_root.hpp
#pragma once
#include "buffer_dscr.hpp"
#include <cstddef>
#include <span>

namespace rtl
{
  /**
   * @brief base of every generated result buffer
   */
  class result_root
  {
  public:
    result_root()                              = default;
    result_root(const result_root&)            = default;
    result_root(result_root&&)                 = delete;
    result_root& operator=(const result_root&) = default;
    result_root& operator=(result_root&&)      = delete;
    virtual ~result_root()                     = default;

    static constexpr auto buffer_description_const() noexcept { return span_buffer_dscr_const{}; }

    [[nodiscard]] virtual span_buffer_dscr_init buffer_description_init() const = 0;

    static constexpr size_t batch_size = 1;
    static constexpr bool   has_results() noexcept { return ! buffer_description_const().empty(); }
    static constexpr bool   is_batch() noexcept { return batch_size > 1; }

    /// how many rows of the buffer the last fetch actually filled
    [[nodiscard]] size_t occupied() const noexcept { return occupied_; }
    void                 set_occupied(size_t value) noexcept { occupied_ = value; }
  private:
    size_t occupied_ = 0;
  };
} // namespace rtl
