// no_results.h
#pragma once
#include "result_root.hpp"

class no_results : public result_root
{
public:
  [[nodiscard]] std::span<const buffer_dscr_init> buffer_description_init() const override
  {
    return {};
  }
};