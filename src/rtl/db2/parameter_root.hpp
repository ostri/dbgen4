// parameter_root.h
#pragma once
#include "buffer_dscr.hpp"
#include <span>

class parameter_root
{
public:
  parameter_root(const parameter_root&)            = default;
  parameter_root(parameter_root&&)                 = delete;
  parameter_root& operator=(const parameter_root&) = default;
  parameter_root& operator=(parameter_root&&)      = delete;
  virtual ~parameter_root()                        = default;

  static constexpr auto buffer_description_const() noexcept
  {
    return std::span<const buffer_dscr_const>{};
  }

  [[nodiscard]] virtual std::span<const buffer_dscr_init> buffer_description_init() const = 0;
  virtual void                                            reset_all_null() noexcept       = 0;

  static constexpr size_t batch_size = 1;
  static constexpr bool   has_parameters() noexcept { return ! buffer_description_const().empty(); }
  static constexpr bool   is_batch() noexcept { return batch_size > 1; }

  [[nodiscard]] virtual std::span<const SQLUSMALLINT> get_row_status() const noexcept { return {}; }
  virtual void                                        clear_row_status() noexcept { }
};