#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <magic_enum.hpp>
#include <yaml-cpp/exceptions.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <yaml-cpp/node/detail/iterator.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <yaml-cpp/yaml.h>

// NOLINTNEXTLINE
#include "common.hpp"
#include "data_statement.hpp"
#include "data_statements.hpp"
#include "pars_result.hpp"
#include "parser_errors.hpp"
#include "parser.hpp"

namespace dbgen4
{
  /**
   * @brief The method parses the provided file and loads its contents to the
   internal structure
   *
   * @param filename
   * @return pars_result_t (parsed data_statements object and error code (0 if
   *         successful, non-zero if not))
   */
  pars_result parser::parse_yaml_file(const str_t& filename, db_type_enum db_type)
  {
    std::ifstream fin(filename);
    if (! fin.is_open())
    {
      l->error("Error: Could not open file '{}'.", std::filesystem::absolute(filename).c_str());
      return pars_result{parser_err_enum::file_cant_be_open};
    }

    try
    {
      return parse_yaml_file(YAML::Load(fin), db_type);
    }
    catch (const YAML::BadFile& e)
    {
      l->error("File {} can not be read. '{}' '{}'", filename, e.msg, e.what());
      return pars_result(parser_err_enum::file_cant_be_open);
    }
    catch (const YAML::ParserException& e)
    {
      l->error("File {} has syntactical error(s). '{}' '{}'", filename, e.msg, e.what());
      return {data_statements{}, parser_err_enum::yaml_syntax_error};
    }
    catch (...)
    {
      auto msg = fmt::format(
        get_parser_err_str(parser_err_enum::parse_error), filename, "unknown error", 0, 0);
      l->error(msg);
      std::cerr << msg << '\n';
      return {data_statements{}, parser_err_enum::parse_error};
    }
  }

  str_t parser::filename() const { return filename_; }

  void parser::set_filename(const str_t& filename) { filename_ = filename; }

  /// extract sql statements from the yaml node to data_statement structure
  /// @param n yaml node representing sql statements
  /// @param s data_statement structure where sql statements will be stored
  /// @return new version of data_statement structure with loaded sql statements or error code
  stmt_result_t parser::extract_sql(const YAML::Node&     n,
                                    const data_statement& s,
                                    db_type_enum          db_type) const
  {
    auto found_sql = false;
    auto res(s);
    /// walk over all sql variants going from general to more specific
    /// more specific sql ovevrites genero one.
    for (auto v : ME::enum_entries<db_type_enum>())
    {
      if ((v.first == db_type_enum::sql) || (v.first == db_type))
      { // generic sql type or specific requested type
        str_t db_type_name(v.second);
        found_sql = n[db_type_name].IsDefined();
        if (found_sql)
        { // we have generic or specific sql statement
          res.set_sql(n[db_type_name].as<str_t>());
          l->debug("statement '{}' {} sql:'{}'", s.id(), db_type_name, res.sql());
        }
      }
    }
    if (! found_sql)
    { /** no sql provided*/
      l->error("No general or specific SQL statements provided in the definition statement. "
               "statement '{}' genereic {} specific {}",
               s.id(),
               ME::enum_name(db_type_enum::sql),
               ME::enum_name(db_type));
      return {{}, parser_err_enum::no_sql_stmt_found};
    }
    return {res, parser_err_enum::ok};
  }
  pars_result parser::process_statement(const YAML::Node&      stmt,
                                        const data_statements& p,
                                        db_type_enum           db_type)
  {
    auto stmts(p);
    if (stmt.IsMap())
    { /// valid statement - object
      data_statement s{};
      auto           id = stmt["id"].as<str_t>();
      if (! id.empty())
      { /// id is provided
        s.set_id(id);
        /// walk over all sql RDBMS variants
        auto res = extract_sql(stmt, s, db_type);
        if (res.second == parser_err_enum::ok)
        {
          if (! stmts.add_statement(res.first))
          { /// duplicated statement id
            // const char* fmt = get_parser_err_str(parser_err_enum::duplicated_stmt_id);
            const auto msg = fmt::format("duplicate id {} {}", filename_, s.id());
            l->error(msg);
            return pars_result{res.second};
          }
        }
        else
        {
          l->error("No sql statements found for statement id '{}'.", s.id());
          return pars_result{parser_err_enum::no_sql_stmt_found};
        }
      }
      else
      {
        l->error("id tag is missing.");
        return pars_result{parser_err_enum::stmt_unique_id_is_missing};
      }
    }
    else { return pars_result{parser_err_enum::inv_statement_syntax}; }
    return {stmts, parser_err_enum::ok};
  }
  /// parse the provided YAML node and load its contents to the internal structure
  /// return parsed data_statements object and error code
  pars_result parser::parse_yaml_file(const YAML::Node& n, db_type_enum db_type)
  {
    data_statements p{};
    if (n.IsMap())
    {
      /// walk over whole document
      if (! n["summary"].IsNull()) p.set_summary(n["summary"].as<str_t>()); /// fetch summary if any
      if (! n["description"].IsNull())
        p.set_description(n["description"].as<str_t>()); /// fetch description if any
      if (n["statements"].IsSequence())
      { /// walk all statements
        const YAML::Node& stmts = n["statements"];
        for (const auto& stmt : stmts) { auto statement = process_statement(stmt, p, db_type); }
      }
      else
      {
        l->error("statements tag is missing.");
        return {{}, parser_err_enum::statements_attr_missing};
      }
    }
    else
    {
      l->error("Invalid yaml file structure. Top level element should be object");
      return {p, parser_err_enum::inv_top_level_struct};
    }
    return {p, parser_err_enum::ok};
  }
}; // namespace dbgen4