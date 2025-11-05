#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
#include <yaml-cpp/emitter.h>
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
#include "db2_rtl.hpp"
#include "pars_result.hpp"
#include "parser_errors.hpp"
#include "parser.hpp"

namespace fs = std::filesystem;
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
    filename_ = filename;
    std::ifstream fin(filename_);
    if (! fin.is_open())
    {
      log()->error("Error: Could not open file '{}'.",
                   std::filesystem::absolute(filename_).c_str());
      return pars_result{parser_err_enum::file_cant_be_open};
    }

    try
    {
      return parse_yaml_file(YAML::Load(fin), db_type);
    }
    catch (const YAML::BadFile& e)
    {
      log()->error("File {} can not be read. '{}' '{}'", filename_, e.msg, e.what());
      return pars_result(parser_err_enum::file_cant_be_open);
    }
    catch (const YAML::ParserException& e)
    {
      log()->error("File {} has syntactical error(s). '{}' '{}'", filename_, e.msg, e.what());
      return {data_statements{}, parser_err_enum::yaml_syntax_error};
    }
    catch (...)
    {
      auto msg = fmt::format(
        get_parser_err_str(parser_err_enum::parse_error), filename_, "unknown error", 0, 0);
      log()->error(msg);
      std::cerr << msg << '\n';
      return {data_statements{}, parser_err_enum::parse_error};
    }
  }

  pars_result parser::load_meta_data(const data_statements& s, rtl::db_db2& db) const
  {
    rtl::qry_metadata res;
    for (const auto& stmt : s.map())
    { /// walking through whole list of statements in one file
      auto sql_id = stmt.first;
      auto sql    = stmt.second.sql();
      auto res    = db.get_sql_metadata(sql);
      res.set_sql(sql);
      res.set_id(sql_id);
      log()->debug(
        "statement: {} sts: {} sql: {}", sql_id, ME::enum_name<rtl::db_sts>(res.status()), sql);
      if (! res.is_success()) return pars_result{parser_err_enum::sql_syntax_err};
      log()->debug("meta data: {}", res.dump());
    };
    log()->info("  {} sql statements processed", s.map().size());
    return {{}, parser_err_enum::ok};
  }

  str_t parser::filename() const { return filename_; }

  void parser::set_filename(const str_t& filename) { filename_ = filename; }

  str_t parser::extract_sql(const YAML::Node& n, db_type_enum db_type) const
  {
    auto db_type_name = ME::enum_name(db_type);
    return n[db_type_name].IsDefined() ? n[db_type_name].as<str_t>() : "";
  }

  /// extract sql statements from the yaml node to data_statement structure
  /// @param n yaml node representing sql statements
  /// @param s data_statement structure where sql statements will be stored
  /// @return new version of data_statement structure with loaded sql statements or error code
  stmt_result parser::extract_sql(const YAML::Node&     n,
                                  const data_statement& s,
                                  db_type_enum          db_type) const
  {
    auto res(s);
    auto sql = extract_sql(n, db_type); // specific sql
    if (sql.empty()) sql = extract_sql(n, db_type_enum::sql);
    if (sql.empty())
    {
      std::stringstream ss;
      ss << n;
      log()->warn("No SQL statement provided. '{}'", ss.str());
      return {{}, parser_err_enum::no_sql_stmt_found};
    }
    res.set_sql(sql);
    log()->trace("sql: '{}'", sql);
    return {res, parser_err_enum::ok};
  }
  pars_result parser::process_statement(const YAML::Node& stmt,
                                        pars_result&      p,
                                        db_type_enum      db_type) const
  {
    // auto stmts(p);
    if (stmt.IsMap())
    { /// valid statement - object
      data_statement s{};
      if (! stmt["id"].IsDefined())
      { // statement unique id is missing
        std::stringstream s;
        YAML::Emitter     e(s);
        e << stmt;
        auto pos = p.s_.map().size();
        log()->error("File: {} id tag is missing. Position {} in statements. "
                     "Definition:\n---\n{}\n---\n exiting",
                     filename_,
                     pos,
                     s.str());
        return pars_result{parser_err_enum::stmt_unique_id_is_missing};
      }
      // auto id = stmt["id"].as<str_t>();
      /// id is provided
      s.set_id(stmt["id"].as<str_t>());
      /// check standard and rdbms specific sql statement. Specific version takes over.
      auto res = extract_sql(stmt, s, db_type);
      if (res.e() == parser_err_enum::ok)
      {
        if (! p.s_.add_statement(res.s()))
        { /// duplicated statement id
          // const char* fmt = get_parser_err_str(parser_err_enum::duplicated_stmt_id);
          const auto msg = fmt::format("File: {} duplicate id {}", filename_, res.s().id());
          log()->error(msg);
          return pars_result{res.e()};
        }
        log()->debug("file {}: new sql : id {} sql '{}'", filename_, s.id(), res.s().sql());
      }
      else
      {
        log()->error(
          "file {}: No sql statements found for statement id '{}'.", filename_, res.s().id());
        return pars_result{parser_err_enum::no_sql_stmt_found};
      }
    }
    else { return pars_result{parser_err_enum::inv_statement_syntax}; }
    return p;
  }
  spdlog::logger* parser::log() const { return log::get(); }
  /// parse the provided YAML node and load its contents to the internal structure
  /// return parsed data_statements object and error code
  pars_result parser::parse_yaml_file(const YAML::Node& n, db_type_enum db_type)
  {
    // data_statements   p{};
    std::stringstream s;
    pars_result       r({}, parser_err_enum::ok); /// result: statemets + success/failure code
    YAML::Emitter     emiter(s);
    emiter << n;
    if (! emiter.good())
    {
      log()->error("Error in YAML file '{}' serialization.", emiter.GetLastError());
      return {{}, parser_err_enum::parse_error};
    }
    log()->debug(R"(yaml file '{}' contents: 
{}
)",
                 fs::relative(fs::absolute(fs::path(filename_))).string(),
                 s.str());
    if (n.IsMap())
    {
      /// walk over whole document
      if (! n["summary"].IsNull())
        r.s_.set_summary(n["summary"].as<str_t>()); /// fetch summary if any
      if (! n["description"].IsNull())
        r.s_.set_description(n["description"].as<str_t>()); /// fetch description if any
      r.s_.set_filename(filename_);
      // p.set_params(const cmd_line_params &params) TODO(ostri) parametri še niso pridruženi
      if (n["statements"].IsSequence())
      { /// walk all statements
        const YAML::Node& stmts = n["statements"];
        for (const auto& stmt : stmts)
        {
          r = process_statement(stmt, r, db_type);
          if (r.e() != parser_err_enum::ok) return r;
        }
      }
      else
      {
        log()->error("statements tag is missing.");
        return {{}, parser_err_enum::statements_attr_missing};
      }
    }
    else
    {
      log()->error("Invalid yaml file structure. Top level element should be object");
      return {{}, parser_err_enum::inv_top_level_struct};
    }
    return r; //{r, parser_err_enum::ok};
  }
}; // namespace dbgen4