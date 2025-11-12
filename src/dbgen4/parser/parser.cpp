#include <expected>
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
// #include "pars_result.hpp"
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
  e_data_statements parser::parse_yaml_file(const str_t& filename, db_type_enum db_type)
  {
    filename_ = filename;
    std::ifstream fin(filename_);
    if (! fin.is_open())
    {
      log()->error("Error: Could not open file '{}'.",
                   std::filesystem::absolute(filename_).c_str());
      return std::unexpected(exit_status_enum::file_cant_be_open);
    }

    try
    {
      return parse_yaml_file(YAML::Load(fin), db_type);
    }
    catch (const YAML::BadFile& e)
    {
      log()->error("File {} can not be read. '{}' '{}'", filename_, e.msg, e.what());
      return std::unexpected(exit_status_enum::file_cant_be_open);
    }
    catch (const YAML::ParserException& e)
    {
      log()->error("File {} has syntactical error(s). '{}' '{}'", filename_, e.msg, e.what());
      return std::unexpected(exit_status_enum::yaml_syntax_error);
    }
    catch (...)
    {
      auto msg = fmt::format(
        get_exit_code_str(exit_status_enum::parse_error), filename_, "unknown error", 0, 0);
      log()->error(msg);
      std::cerr << msg << '\n';
      return std::unexpected(exit_status_enum::parse_error);
    }
  }
  /**
   * @brief walks through all sql statements of the yaml document and extract its metadata
   *
   * Processing stops upon first syntax error detected in sql statements
   *
   * @param s statements - vector of sql descriptions
   * @param db database conneciton
   * @return pars_result error code + updated statements structure
   */
  e_data_statements parser::load_file_meta_data(const data_statements& s, rtl::db_db2& db) const
  {
    //    rtl::qry_metadata res;          // temporary sql statement metadata as received from db2
    //    rtl
    data_statements res_stmts{s}; // result statements with updated metadata
    for (const auto& map_stmt_pair : s.map())
    { /// walking through whole list of statements in one file
      // auto sql_id = stmt.first;
      auto sql = map_stmt_pair.second.sql();
      auto res = db.get_sql_metadata(sql);
      if (! res)
      {
        log()->error("Invalid sql '{}' status: {} mnemonic {}",
                     sql,
                     ME::enum_integer(res.error()),
                     ME::enum_name<rtl::db_sts>(res.error()));
        return std::unexpected(exit_status_enum::sql_syntax_err);
      }

      /// all ok. Update the sql description with metadata
      log()->trace("meta data: {}", res.value().dump());
      data_statement res_stmt(map_stmt_pair.second);
      // res_stmt.set_par_set_size(map_stmt_pair.second.par_set_size());
      // res_stmt.set_res_set_size(map_stmt_pair.second.res_set_size());
      res_stmt.set_columns(res.value().columns());
      res_stmt.set_params(res.value().params());
      res_stmts.add_statement_with_replace(
        res_stmt); /// we are replacing existing statement values (meta data added,
                   /// everything else the same)
    };
    log()->info("{} sql statements processed", s.map().size());
    return res_stmts;
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
  e_data_statement parser::extract_sql_to_statement(const YAML::Node&     n,
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
      return std::unexpected(exit_status_enum::no_sql_stmt_found);
    }
    res.set_sql(sql);
    log()->trace("sql: '{}'", sql);
    return res;
  }
  exit_status_enum parser::log_id_is_missing(const YAML::Node& stmt, size_t pos) const
  {
    { // statement unique id is missing
      std::stringstream ss;
      YAML::Emitter     e(ss);
      e << stmt;
      log()->error("File: {} id tag is missing. Position {} in statements. "
                   "Definition:\n---\n{}\n---\n exiting",
                   filename_,
                   pos,
                   ss.str());
      return exit_status_enum::stmt_unique_id_is_missing;
    }
  }
  exit_status_enum parser::no_sql_found(const str_t& stmt_id) const
  {
    log()->error("file {}: No sql statements found for statement id '{}'.", filename_, stmt_id);
    return exit_status_enum::no_sql_stmt_found;
  }

  e_data_statement parser::process_statement(const YAML::Node&      yaml_stmt,
                                             const data_statements& stmts,
                                             db_type_enum           db_type) const
  {
    // auto stmts(p);
    if (yaml_stmt.IsMap())
    {
      data_statement s{}; /// statement to be filled
      if (! yaml_stmt["id"].IsDefined())
        return std::unexpected(log_id_is_missing(yaml_stmt, stmts.map().size()));
      /// id is provided
      s.set_id(yaml_stmt["id"].as<str_t>());
      /// dscr
      if (yaml_stmt["dscr"].IsDefined()) s.set_dscr(yaml_stmt["dscr"].as<str_t>());
      log()->debug("file {}: 'dscr' '{}'", filename_, s.dscr());
      /// result-size
      if (yaml_stmt["result-size"].IsDefined())
        s.set_res_set_size(yaml_stmt["result-size"].as<size_t>());
      log()->debug("file {}: 'result-size' is '{}'", filename_, s.res_set_size());
      /// parameter-size
      if (yaml_stmt["parameter-size"].IsDefined())
        s.set_par_set_size(yaml_stmt["parameter-size"].as<size_t>());
      log()->debug("file {}: 'parameter-size' is '{}'", filename_, s.par_set_size());

      /// check standard and rdbms specific sql statement. Specific version takes over.
      auto res = extract_sql_to_statement(yaml_stmt, s, db_type);
      if (! res) return std::unexpected(no_sql_found(s.id()));
      log()->debug("file {}: new sql : id {} sql '{}'", filename_, s.id(), res.value().sql());
      return res;
    };
    log()->error("Invalid statement syntax in file '{}'.", filename_);
    return std::unexpected(exit_status_enum::inv_statement_syntax);
  }
  /**
   * @brief fetch pointer to logger
   *
   * @return spdlog::logger*
   */
  spdlog::logger* parser::log() const { return log::get(); }
  /**
   * @brief log yaml file segment
   *
   * @param n
   * @return exit_status_enum
   */
  exit_status_enum parser::log_yaml_segment(const YAML::Node& n)
  {
    std::stringstream s;
    YAML::Emitter     emiter(s);
    emiter << n;
    if (! emiter.good())
    {
      log()->error("Error in YAML file '{}' serialization.", emiter.GetLastError());
      return exit_status_enum::parse_error;
    }
    log()->debug(R"(yaml file '{}' contents:
{}
)",
                 fs::relative(fs::absolute(fs::path(filename_))).string(),
                 s.str());
    return exit_status_enum::ok;
  }
  /// parse the provided YAML node and load its contents to the internal structure
  /// return parsed data_statements object and error code

  /**
   * @brief The method parses the provided YAML node and loads its contents to the internal
   * structure
   *
   * @param n   YAML node
   * @param db_type   database type
   * @return e_data_statements  (parsed data_statements object and error code (0 if successful,
   * non-zero if not))
   */
  e_data_statements parser::parse_yaml_file(const YAML::Node& n, db_type_enum db_type)
  {
    data_statements stmts{};
    auto            sts = log_yaml_segment(n);
    if (sts != exit_status_enum::ok) return std::unexpected(sts);

    if (n.IsMap())
    {
      /// walk over whole document
      if (! n["summary"].IsNull())
        stmts.set_summary(n["summary"].as<str_t>()); /// fetch summary if any

      if (! n["description"].IsNull())
        stmts.set_description(n["description"].as<str_t>()); /// fetch description

      stmts.set_filename(filename_);

      if (n["statements"].IsSequence())
      { /// walk all statements
        const YAML::Node& yaml_stmts = n["statements"];
        for (const auto& stmt : yaml_stmts)
        {
          auto r = process_statement(stmt, stmts, db_type);
          if (! r) return std::unexpected(r.error());
          if (! stmts.add_statement(r.value()))
          { /// duplicated statement id
            const auto msg = fmt::format("File: {} duplicate id {}", filename_, r.value().id());
            log()->error(msg);
            return std::unexpected(r.error());
          }
        }
        return stmts; // all ok
      }
      log()->error("statements tag is missing.");
      return std::unexpected(exit_status_enum::statements_attr_missing);
    }
    log()->error("Invalid yaml file structure. Top level element should be object");
    return std::unexpected(exit_status_enum::inv_top_level_struct);
  }
}; // namespace dbgen4