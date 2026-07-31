#pragma once
#include "buffer_dscr.hpp"
#include <cstddef>
#include <cstdint>
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
    static constexpr span_buffer_dscr_const     buffer_description_const() noexcept;
    [[nodiscard]] virtual span_buffer_dscr_init buffer_description_init() = 0;
    void                                        set_buffer_size(size_t rows);
    [[nodiscard]] virtual size_t                buffer_size() const noexcept = 0;
    [[nodiscard]] bool                          is_batch() const noexcept;
    [[nodiscard]] uint64_t                      layout_generation() const noexcept;
    static constexpr bool                       has_results() noexcept;
    [[nodiscard]] size_t                        occupied() const noexcept;
    void                                        set_occupied(size_t value) noexcept;
  protected:
    /// resize every column array to `rows` and republish buffer_description_init()
    virtual void resize_storage(size_t rows) = 0;
  private:
    size_t   occupied_          = 0; //< how many rows of the buffer the last fetch actually filled
    uint64_t layout_generation_ = 0; //< are prepare and execute still in sync with the buffer?
  };
  ///////////////////////////////////////////////////////////////////////////////////////////
  constexpr span_buffer_dscr_const result_root::buffer_description_const() noexcept { return span_buffer_dscr_const{}; }
  /**
   * @brief set how many rows the buffer holds
   *
   * Call before prepare(). Not virtual, and paired with a generation counter
   * - see parameter_root for both reasons.
   */
  inline void result_root::set_buffer_size(size_t rows)
  {
    if (rows == 0) rows = 1;
    resize_storage(rows);
    ++layout_generation_;
  }
  inline bool     result_root::is_batch() const noexcept { return buffer_size() > 1; }
  inline uint64_t result_root::layout_generation() const noexcept { return layout_generation_; }
  constexpr bool  result_root::has_results() noexcept { return ! buffer_description_const().empty(); }
  inline size_t   result_root::occupied() const noexcept { return occupied_; }
  inline void     result_root::set_occupied(size_t value) noexcept { occupied_ = value; }
} // namespace rtl
