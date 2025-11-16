// no_params.h
#pragma once
#include "buffer_dscr.hpp"
#include "parameter_root.hpp"

class no_params : public parameter_root
{
public:
  [[nodiscard]] std::span<const buffer_dscr_init> buffer_description_init() const override
  {
    return {};
  }
  void reset_all_null() noexcept override { }
};
