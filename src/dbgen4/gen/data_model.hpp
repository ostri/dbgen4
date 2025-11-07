#pragma once
#include "common.hpp"

namespace dbgen4
{
  namespace gen
  {
    const auto block_align_128 = 128; ///< 128 bytes alignment for code generation blocks
    const auto block_align_64  = 64;  ///< 64 bytes alignment for code generation blocks
    struct statement
    {
      str_t id;   ///< statement unique id
      str_t sql;  ///< sql statement
      str_t desc; ///< statement description
    } __attribute__((aligned(block_align_128)));
    struct document
    {
      str_t                  name;       ///< document name
      std::vector<statement> statements; ///< statements in the document
    } __attribute__((aligned(block_align_64)));
  }; // namespace gen
}; // namespace dbgen4