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

    static constexpr span_buffer_dscr_const buffer_description_const() noexcept;

    /// Not const on purpose: this hands out writable pointers into the buffer
    /// so that the backend can fill it in.
    [[nodiscard]] virtual span_buffer_dscr_init buffer_description_init() = 0;
    /**
     * @brief set how many rows the buffer holds
     *
     * Call before prepare(). Which columns a buffer has is fixed at generation
     * time; how many rows it carries is the caller's decision, so this one is a
     * run time value.
     *
     * Not virtual on purpose: the bookkeeping belongs in one place, and the
     * generated class only says how to move its storage. An override could
     * otherwise resize without the generation counter noticing, which is the
     * one thing that must not happen.
     */
    void set_buffer_size(size_t rows);
    /**
     * @brief how many rows the buffer holds
     *
     * Answered by the arrays themselves rather than by a counter kept beside
     * them: a cached size is one more thing that can disagree with the storage
     * it describes, and this one is read a handful of times per statement, all
     * of them outside any loop.
     */
    [[nodiscard]] virtual size_t buffer_size() const noexcept = 0;
    [[nodiscard]] bool           is_batch() const noexcept;
    /**
     * @brief how many times the storage has moved
     *
     * prepare() hands the driver raw pointers into the column arrays and
     * records this number; execute() compares. A buffer resized in between has
     * reallocated, so those pointers dangle and the driver reading them is a
     * use after free - the counter turns that into a refusal instead.
     */
    [[nodiscard]] uint64_t layout_generation() const noexcept;
    static constexpr bool  has_parameters() noexcept;
    /// per row status of a batch execute; empty when the buffer is not batched
    /// Mutable: the driver writes the per row status into this array.
    [[nodiscard]] virtual std::span<uint16_t> row_status() noexcept;
    virtual void                              clear_row_status() noexcept { }
  protected:
    /// resize every column array to `rows` and republish buffer_description_init()
    virtual void resize_storage(size_t rows) = 0;
    virtual void reset_all_null() noexcept   = 0;
  private:
    uint64_t layout_generation_ = 0;
  };
  ///////////////////////////////////////////////////////////////////////////////////////////////////
  constexpr span_buffer_dscr_const parameter_root::buffer_description_const() noexcept { return {}; }

  /// Stays static: it asks what the buffer is made of, not how big it is.
  constexpr bool parameter_root::has_parameters() noexcept { return ! buffer_description_const().empty(); }
} // namespace rtl
