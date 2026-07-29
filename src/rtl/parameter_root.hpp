// parameter_root.hpp
#pragma once
#include "buffer_dscr.hpp"
#include <cstdint>
#include <span>

namespace rtl
{
  /**
   * @brief base of every generated parameter buffer
   *
   * Backend neutral - a generated buffer only describes what it holds, the
   * backend decides how to hand that to its driver.
   */
  class parameter_root
  {
  public:
    parameter_root()                                 = default;
    parameter_root(const parameter_root&)            = default;
    parameter_root(parameter_root&&)                 = delete;
    parameter_root& operator=(const parameter_root&) = default;
    parameter_root& operator=(parameter_root&&)      = delete;
    virtual ~parameter_root()                        = default;

    static constexpr auto buffer_description_const() noexcept { return span_buffer_dscr_const{}; }

    [[nodiscard]] virtual span_buffer_dscr_init buffer_description_init() const = 0;
    virtual void                                reset_all_null() noexcept       = 0;

    static constexpr size_t batch_size = 1;
    static constexpr bool   has_parameters() noexcept { return ! buffer_description_const().empty(); }
    static constexpr bool   is_batch() noexcept { return batch_size > 1; }

    /// per row status of a batch execute; empty when the buffer is not batched
    [[nodiscard]] virtual std::span<const uint16_t> get_row_status() const noexcept { return {}; }
    virtual void                                    clear_row_status() noexcept { }
  };
} // namespace rtl
