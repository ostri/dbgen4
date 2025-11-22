#pragma once

// #include "rtl.hpp"
#include <cstdint>
#include <fmt/format.h>

namespace dbgen4
{
  /**
   * @brief Enumeration of possible program exit codes
   */
  enum class exit_status_enum : std::uint8_t
  {
    not_implemented           = 255, // feature not implemented
    ok                        = 0,   // everything is nice
    file_cant_be_open         = 2,   // provided input file can't be open
    yaml_syntax_error         = 3,   // there is syntax error in the yaml file
    inv_top_level_struct      = 4,   // invalid structure of the yaml file
    statements_attr_missing   = 5,   // id or sql is missing
    inv_statement_syntax      = 6,   // invalid statement syntax
    stmt_unique_id_is_missing = 7,   // statement unique id is missing
    duplicated_stmt_id        = 8,   // duplicate statement id found
    no_sql_stmt_found   = 9,  // sql statement is missing in the statement definition in yaml file
    parse_error         = 10, // generic parse error
    unhandled_exception = 11, // exception we are not prepared for
    connection_error    = 12, // cant connect to the databaase
    sql_syntax_err      = 13, // there is syntax in error in sql, so that prepare can not be done
    inja_parser_error   = 14, // inja parser error
    inja_render_error   = 15, // inja template rendering error
    inja_data_error     = 16, // inja data error
    inja_file_error     = 17, // inja file error
    inja_general_error  = 18, // inja general error
    error_writing_file  = 19, // error writing file
    error_reading_file  = 20, // error reading file
    runtime_error       = 21, // runtime error normally signals internal error
  };
  // parser_err_enum cvt(rtl::db_sts s) { return reinterpret_cast<parser_err_enum>(s); }
  /**
   * @brief Gets human readable string format for the provided error code
   * @param code error code
   * @return human readable string format compatible with fmt/spdlog
   */
  constexpr const char* get_exit_code_str(exit_status_enum code) noexcept
  {
    switch (code)
    {
      // clang-format off
    case exit_status_enum::ok:                        return "Document '{}' Operation successful '{}'";
    case exit_status_enum::file_cant_be_open:         return "Document '{}' Unable to open file '{}'";
    case exit_status_enum::yaml_syntax_error:         return "Document '{}' YAML syntax error '{}' line: {} column: {}";
    case exit_status_enum::inv_top_level_struct:      return "Document '{}' Invalid top level structure '{}' line: {} column: {}";
    case exit_status_enum::statements_attr_missing:   return "Document '{}' Statements attribute is missing '{}' line: {} column: {}";
    case exit_status_enum::inv_statement_syntax:      return "Document '{}' Invalid statement syntax '{}' line: {} column: {}";
    case exit_status_enum::stmt_unique_id_is_missing: return "Document '{}' Statement unique ID is missing '{}' line: {} column: {}";
    case exit_status_enum::duplicated_stmt_id:        return "Document '{}' Duplicate statement ID detected '{}' line: {} column: {}";
    case exit_status_enum::no_sql_stmt_found:         return "Document '{}' No SQL statement found '{}' line: {} column: {}";
    case exit_status_enum::parse_error:               return "Document '{}' Unknown parser error '{}' line: {} column: {}";
    case exit_status_enum::unhandled_exception:       return "Document '{}' unhandled exception";
    case exit_status_enum::connection_error:          return "Can't connect to db host: {} db {} user {} err: {} error '{}";
    case exit_status_enum::sql_syntax_err:            return "SQL statement {} syntax error {}.";
    case exit_status_enum::not_implemented:           return "Not implemented feature.";
    case exit_status_enum::inja_parser_error:         return "Document '{}' Inja parser error: '{}'";
    case exit_status_enum::inja_render_error:         return "Document '{}' Inja template rendering error: '{}'";
    case exit_status_enum::inja_file_error:           return "Document '{}' Inja file error: '{}'";
    case exit_status_enum::inja_data_error:           return "Document '{}' Inja data error: '{}'";
    case exit_status_enum::inja_general_error:        return "Document '{}' Inja general error: '{}'";
    case exit_status_enum::error_writing_file:        return "Document '{}' error writing file: '{}'";
    case exit_status_enum::error_reading_file:        return "Document '{}' error reading file: '{}'";
    case exit_status_enum::runtime_error:             return "Document '{}' runtime/internal error: '{}'";
    default:
      // clang-format on
      __builtin_unreachable();
      // return "Document '{}' Unknown error '{}' line: {} column: {}";
    }
  }

}; // namespace dbgen4
