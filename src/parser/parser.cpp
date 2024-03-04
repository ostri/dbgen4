#include "parser.hpp"
#include "data_statements.hpp"
#include <filesystem>
#include <fstream>
#include <magic_enum.hpp>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/yaml.h>

namespace
{
  // // NOLINTNEXTLINE
  // static const json person_schema = R"(
  //   {
  //     "$schema": "http://json-schema.org/draft-07/schema#",
  //     "title": "A person",
  //     "properties": {
  //         "name": {
  //             "description": "Name",
  //             "type": "string"
  //         },
  //         "age": {
  //             "description": "Age of the person",
  //             "type": "number",
  //             "minimum": 2,
  //             "maximum": 200
  //         }
  //     },
  //     "required": [
  //                 "name",
  //                 "age"
  //                 ],
  //     "type": "object"
  //   }
  //   )"_json;

} // namespace


namespace dbgen4
{
  /**
   * @brief The method parses the provided file and loads its contents to the internal structure
   *
   * @param filename
   * @return int - 0 - ok
                 - 1 - provided filename does not exists
                 - 2 - syntax error int tne provided file (i.e. not yaml compliant)
   */
  pars_result_t parser::exec(const str_t& filename)
  {
    std::ifstream fin(filename);
    if (! fin.is_open())
    {
      l->error("Error: Could not open file '{}'.", std::filesystem::absolute(filename).c_str());
      return {data_statements{}, parser_errors_enum::file_cant_be_open};
    }

    try
    {
      return exec(YAML::Load(fin));
    }
    catch (const YAML::BadFile& e)
    {
      l->error("File {} can not be read. '{}' '{}'", filename, e.msg, e.what());
      return {data_statements{}, parser_errors_enum::file_cant_be_open};
    }
    catch (const YAML::ParserException& e)
    {
      l->error("File {} has syntactical error(s). '{}' '{}'", filename, e.msg, e.what());
      return {data_statements{}, parser_errors_enum::yaml_syntax_error};
    };
  }

  str_t parser::filename() const { return filename_; }

  void parser::set_filename(const str_t& filename) { filename_ = filename; }

  bool parser::extract_sqls(const YAML::detail::iterator_value& stmt, data_statement& s)
  {
    size_t ndx = 0;
    for (auto v : ME::enum_entries<db_type_enum>())
    {
      str_t name(v.second);
      if (! stmt[name].IsNull())
      {
        ndx++;
        auto name_value = stmt[name].as<str_t>();
        s.set_sql(v.first, name_value);
        l->info("statement '{}' {} : '{}'", s.id(), name, name_value);
      }
    }
    if (ndx == 0U)
    { /** no sql provided*/
      l->error("No SQL statements provided in statement. '{}'", s.id());
      return false;
    }
    return true;
  }
  pars_result_t parser::exec(const YAML::Node& n)
  {
    data_statements p{};
    if (n.IsMap())
    {
      if (! n["summary"].IsNull()) p.set_summary(n["summary"].as<str_t>());
      if (! n["description"].IsNull()) p.set_description(n["description"].as<str_t>());
      if (n["statements"].IsSequence())
      {
        const YAML::Node& stmts = n["statements"];
        for (const auto& stmt : stmts)
        { /** process statement by statement
           */
          if (stmt.IsMap())
          {
            data_statement s{};
            if (! stmt["id"].IsNull())
            {
              s.set_id(stmt["id"].as<str_t>());
              /// walk over all sql variants
              extract_sqls(stmt, s);
              if (p.add_statement(s)) { return {p, parser_errors_enum::ok}; }
              l->error("duplicate statement id '{}'.", s.id());
              return {p, parser_errors_enum::duplicated_stmt_id};
            }
            l->error("id tag is missing.");
            return {p, parser_errors_enum::statement_unique_id_is_missing};
            l->trace("sql: '{}'", stmt["sql"].as<str_t>());
          }
          return {p, parser_errors_enum::inv_statement_syntax};
        }
      }
      return {p, parser_errors_enum::statements_attr_missing};
    }
    l->error("Invalid yaml file structure. Top level element should be object");
    return {p, parser_errors_enum::inv_top_level_struct};
  }
}; // namespace dbgen4