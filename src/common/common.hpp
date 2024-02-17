#ifndef COMMON_HPP
#define COMMON_HPP

#include <string_view>
#include <vector>
#include <string>
#include <magic_enum.hpp>
#include "log.hpp" // IWYU pragma: export

namespace dbgen4
{
  using str_t     = std::string;
  using cstr_t    = std::string_view;
  using vec_str_t = std::vector<str_t>;
  namespace ME    = magic_enum; // NOLINT(misc-unused-alias-decls)

  /**
   * @brief enumeration of supported databases
   *
   */
  enum class db_type_enum : int
  {
    none    = 0, /// none selected
    mariadb = 1, /// maria db
    psql    = 2, /// postgresql
    db2     = 3  /// ibm db2
  };

  str_t join(const vec_str_t& o, const str_t& delim);
}; // namespace dbgen4

#endif // COMMON_HPP
