#include "common.hpp"

namespace dbgen4
{
  str_t join(const vec_str_t& o, const str_t& delim)
  {
    str_t s{};
    for (const str_t& el : o) s += el + delim;
    if (! s.empty()) s.resize(s.size() - delim.size());
    return s;
  };
}; // namespace dbgen4