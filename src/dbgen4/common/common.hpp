#pragma once

// #include <vector>

#include <string>
#include <string_view>
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
// #include <nlohmann/json.hpp>
// #include <nlohmann/json-schema.hpp>
#include "log.hpp"           // IWYU pragma: export
#include "build_type.hpp"    // IWYU pragma: export
#include "parser_errors.hpp" // IWYU pragma: export

#include <vector>

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

  str_t            join(const vec_str_t& o, const str_t& delim = "\n");
  std::string_view trim_whitespace_view(std::string_view s);

  /// Splits the input string by a single character delimiter,
  /// and prefixes each resulting token with the provided string.

  /**
   * @brief  Splits the input string by a single character delimiter, and prefixes each resulting
   * token with the provided string.
   *
   * @param input_sv  input string view to be split
   * @param delimiter character delimiter
   * @param prefix    prefix to be added to each token
   * @return vec_str_t  vector of prefixed tokens
   */
  vec_str_t prefix_split(std::string_view input_sv, char delimiter, const std::string& prefix);

  str_t offset_text(const str_t& text, size_t offs);

  str_t prefix_text(const str_t& text, size_t offs);
}; // namespace dbgen4
