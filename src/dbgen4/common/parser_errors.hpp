#pragma once

// #include "rtl.hpp"
#include <cstdint>
#include <fmt/format.h>

namespace dbgen4
{
  enum class parser_err_enum : std::uint8_t
  {
    // clang-format off
    ok,                         // everything is nice
    file_cant_be_open,          // provided input file can't be open
    yaml_syntax_error,          // there is syntax error in the yaml file
    inv_top_level_struct,       // invalid structure of the yaml file
    statements_attr_missing,    // id or sql is missing
    inv_statement_syntax,
    stmt_unique_id_is_missing,
    duplicated_stmt_id,
//    sql_statement_missing,
    no_sql_stmt_found,          // sql statement is missing in the statement definition in yaml file
    parse_error,
    unhandled_exception,        // exception we are not prepared for
    connection_error,           // cant connect to the databaase
    sql_syntax_err,             // there is syntax in error in sql, so that prepare can not be done
    // clang-format on
  };
  // parser_err_enum cvt(rtl::db_sts s) { return reinterpret_cast<parser_err_enum>(s); }
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
//    case parser_err_enum::sql_statement_missing:     return "Document '{}' SQL statement is missing '{}' line: {} column: {}";
    case parser_err_enum::no_sql_stmt_found:         return "Document '{}' No SQL statement found '{}' line: {} column: {}";
    case parser_err_enum::parse_error:               return "Document '{}' Unknown parser error '{}' line: {} column: {}";
    case parser_err_enum::unhandled_exception:       return "Document '{}' unhandled exception";
    case parser_err_enum::connection_error:          return "Can't connect to db host: {} db {} user {} err: {} error '{}";
    case parser_err_enum::sql_syntax_err: return "SQL statement {} syntax error {}.";
    default:
      // clang-format on
      __builtin_unreachable();
      // return "Document '{}' Unknown error '{}' line: {} column: {}";
    }
  }

}; // namespace dbgen4
