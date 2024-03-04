#ifndef PARSER_ERRORS_HPP
#define PARSER_ERRORS_HPP

namespace dbgen4
{
  enum class parser_errors_enum
  {
    ok,
    file_cant_be_open,
    yaml_syntax_error,
    inv_top_level_struct,
    statements_attr_missing,
    inv_statement_syntax,
    statement_unique_id_is_missing,
    duplicated_stmt_id
  };
};     // namespace dbgen4
#endif // PARSER_ERRORS_HPP
