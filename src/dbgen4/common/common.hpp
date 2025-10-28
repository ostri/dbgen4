#pragma once

// #include <vector>
#include <string>
#include <string_view>

#include <magic_enum.hpp>
// #include <nlohmann/json.hpp>
// #include <nlohmann/json-schema.hpp>
#include "log.hpp"           // IWYU pragma: export
#include "build_type.hpp"    // IWYU pragma: export
#include "parser_errors.hpp" // IWYU pragma: export

// using json = nlohmann::json;

namespace dbgen4
{
  using str_t     = std::string;
  using cstr_t    = std::string_view;
  using vec_str_t = std::vector<str_t>;
  namespace ME    = magic_enum; // NOLINT(misc-unused-alias-decls)
  namespace spd   = spdlog;     // NOLINT(misc-unused-alias-decls)

  /**
   * @brief enumeration of supported databases
   *
   */
  enum class db_type_enum : std::uint8_t
  {
    sql     = 0, /// standard sql selected (must be the first one )
    mariadb = 1, /// maria db
    psql    = 2, /// postgresql
    db2     = 3  /// ibm db2
  };

  str_t join(const vec_str_t& o, const str_t& delim);
}; // namespace dbgen4
