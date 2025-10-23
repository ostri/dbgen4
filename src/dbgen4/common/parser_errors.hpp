#pragma once

#include <cstdint>
#include <fmt/format.h>

namespace dbgen4
{
  enum class parser_err_enum : std::uint8_t
  {
    ok,
    file_cant_be_open,
    yaml_syntax_error,
    inv_top_level_struct,
    statements_attr_missing,
    inv_statement_syntax,
    stmt_unique_id_is_missing,
    duplicated_stmt_id,
    sql_statement_missing,
    no_sql_stmt_found,
    parse_error
  };

  /**
   * @brief Gets human readable string format for the provided error code
   * @param code error code
   * @return human readable string format compatible with fmt/spdlog
   */
  constexpr const char* get_parser_err_str(parser_err_enum code) noexcept
  {
    switch (code)
    {
      // clang-format off
    case parser_err_enum::ok:                        return "Document '{}' Operation successful '{}'";
    case parser_err_enum::file_cant_be_open:         return "Document '{}' Unable to open file '{}'";
    case parser_err_enum::yaml_syntax_error:         return "Document '{}' YAML syntax error '{}' line: {} column: {}";
    case parser_err_enum::inv_top_level_struct:      return "Document '{}' Invalid top level structure '{}' line: {} column: {}";
    case parser_err_enum::statements_attr_missing:   return "Document '{}' Statements attribute is missing '{}' line: {} column: {}";
    case parser_err_enum::inv_statement_syntax:      return "Document '{}' Invalid statement syntax '{}' line: {} column: {}";
    case parser_err_enum::stmt_unique_id_is_missing: return "Document '{}' Statement unique ID is missing '{}' line: {} column: {}";
    case parser_err_enum::duplicated_stmt_id:        return "Document '{}' Duplicate statement ID detected '{}' line: {} column: {}";
    case parser_err_enum::sql_statement_missing:     return "Document '{}' SQL statement is missing '{}' line: {} column: {}";
    case parser_err_enum::no_sql_stmt_found:         return "Document '{}' No SQL statement found '{}' line: {} column: {}";
    case parser_err_enum::parse_error:               return "Document '{}' Unknown parser error '{}' line: {} column: {}";
    default:                                         return "Document '{}' Unknown error '{}' line: {} column: {}";
      // clang-format on
    }
  }

}; // namespace dbgen4
