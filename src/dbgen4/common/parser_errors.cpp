#include "parser_errors.hpp"

namespace dbgen4
{

  //   /**
  //    * @brief gets mapping of error codes to human readable strings
  //    */
  //   /// @return mapping of error codes to human readable strings
  //   consteval const std::map<parser_err_enum, const char*> get_parser_err_fmt()
  //   {
  //     // clang-format off
  //     ///  mapping of error codes to human readable strings

  //     static const std::map<parser_err_enum, const char*> parser_err_fmt_map =
  //     {
  //       {parser_err_enum::ok,                        "File '{}' operation completed
  //       successfully."}, {parser_err_enum::file_cant_be_open,         "File '{}' can't be
  //       opened."}, {parser_err_enum::yaml_syntax_error,         "File '{}' contains invalid YAML
  //       syntax. Error details: {}"}, {parser_err_enum::inv_top_level_struct,      "File '{}' has
  //       invalid top level structure. Expected a mapping."},
  //       {parser_err_enum::statements_attr_missing,   "File '{}' is missing 'statements'
  //       attribute."}, {parser_err_enum::inv_statement_syntax,      "File '{}' statement id: '{}'
  //       contains statement with invalid syntax."}, {parser_err_enum::stmt_unique_id_is_missing,
  //       "File '{}' contains statement without unique id.  Each statement must have a unique 'id'
  //       attribute."}, {parser_err_enum::duplicated_stmt_id,        "File '{}' contains duplicated
  //       statement id: '{}'."}, {parser_err_enum::sql_statement_missing,     "File '{}' statement
  //       id: '{}' is missing 'sql' or specific database sql attribute."},
  //       {parser_err_enum::no_sql_stmt_found,         "File '{}' statement id: '{}' has no SQL
  //       statements."},
  //     };
  //     // clang-format on
  //     return parser_err_fmt_map;
  //   }


  //   /**
  //    * @brief gets human readable string for the provided error code
  //    * @param code error code
  //    * @return human readable string for the provided error code
  //    */
  //   consteval const char* get_parser_err_str(parser_err_enum code)
  //   {
  //     const auto& fmt_map = get_parser_err_fmt();
  //     const auto  it      = fmt_map.find(code);
  //     return it != fmt_map.end() ? it->second : "Unknown parser error code.";
  //   }
}; // namespace dbgen4
