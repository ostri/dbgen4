#pragma once

// #include <vector>


#include <expected>
#include <string>
#include <string_view>
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
#include "log.hpp"           // IWYU pragma: export
#include "build_type.hpp"    // IWYU pragma: export
#include "parser_errors.hpp" // IWYU pragma: export

#include <vector>
#include <utf8.hpp>
#include "hex.hpp"

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

  using e_string_ = std::expected<str_t, str_t>;
  e_string_                  read_file(const str_t& filename);
  [[maybe_unused]] e_string_ write_file(const str_t& filename, const str_t& contents);

  std::string lowercse(std::string_view vhodni_pogled);

  // // ali še krajše (inline) C++26
  // inline std::string to_utf8(std::wstring_view ws) { return std::to_utf8(std::wcstring_view(ws));
  // }
}; // namespace dbgen4
