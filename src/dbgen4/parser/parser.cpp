#include "parse_yaml.hpp"
#include <cstddef>
#include <expected>
// #include <filesystem>
#include <fmt/format.h>
#include <source_location>
// #include <fstream>
// #include <iostream>
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

// namespace fs = std::filesystem;
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
    auto res  = parse_yaml::load(filename_);
    if (! res) return std::unexpected(exit_status_enum::file_cant_be_open);
    return parse_yaml_file_json(res.value(), db_type);
  }
  e_data_statements parser::parse_yaml_string(const str_t& yaml_str, db_type_enum db_type)
  {
    auto res = parse_yaml::load_from_string(yaml_str);
    if (! res) return std::unexpected(exit_status_enum::yaml_syntax_error);
    return parse_yaml_file_json(res.value(), db_type);
  }
  /**
   * @brief walks through all sql statements of the yaml document and extract its metadata
   *
   * Processing stops upon first syntax error detected in sql statements
   *
   * @param s statements - vector of sql descriptions
   * @param db database connection
   * @return pars_result error code + updated statements structure
   */
  e_data_statements parser::load_file_meta_data(const data_statements& s, rtl::db_db2& db) const
  {
    data_statements res_stmts{s}; // result statements with updated metadata
    for (const auto& map_stmt_pair : s.map_statements())
    { /// walking through whole list of statements in one file
      auto sql = map_stmt_pair.second.sql();
      auto res = db.get_sql_metadata(sql);
      if (! res)
      {
        log()->error(
          "Invalid sql '{}' status: {} mnemonic {}", sql, ME::enum_integer(res.error()), ME::enum_name<rtl::db_sts>(res.error()));
        return std::unexpected(exit_status_enum::sql_syntax_err);
      }

      /// all ok. Update the sql description with metadata
      log()->trace("meta data: {}", res.value().dump());
      data_statement res_stmt(map_stmt_pair.second);
      res_stmt.set_results(res.value().columns());
      res_stmt.set_params(res.value().params());
      res_stmt.push_column_names();                   // overwrite database column names with user defined (if they exist)
      res_stmts.add_statement_with_replace(res_stmt); /// we are replacing existing statement values (meta data added,
                                                      /// everything else the same)
    };
    log()->info("{} sql statements processed", s.map_statements().size());
    return res_stmts;
  }

  str_t parser::filename() const { return filename_; }

  void parser::set_filename(const str_t& filename) { filename_ = filename; }

  /// @brief loads the data from the yaml file structure to data structures
  /// @param n internal yaml file structure
  /// @return result of the operation, optional loaded data structure
  e_data_statements parser::parse_yaml_file_json(const parse_yaml& n, db_type_enum db_type) const
  {
    data_statements stmts{};

    stmts.set_summary(n.get_or<std::string>("summary", ""));
    stmts.set_description(n.get_or<std::string>("description", ""));
    stmts.set_filename(filename_);
    auto yaml_statements = n.get_sequence_of_maps("statements");
    if (! yaml_statements) return std::unexpected(exit_status_enum::statements_attr_missing);
    for (const auto& s : yaml_statements.value())
    { /// walk over all sql statement description
      auto id          = s.get<std::string>("id");
      auto result_size = s.get_or<size_t>("result-size", 1);
      auto param_size  = s.get_or<size_t>("param-size", 1);
      auto dscr        = s.get_or<std::string>("dscr", "");
      // must be first - sql value or empty after then specializations
      auto sql = s.get_or<std::string>(str_t(ME::enum_name(db_type_enum::sql)), "");
      log()->trace("General sql '{0}'", sql);
      for (auto dbt : ME::enum_values<db_type_enum>())
      {
        if (db_type == dbt)
        {
          sql = s.get_or<std::string>(str_t(ME::enum_name(db_type)), sql);
          log()->trace("Specialized {0} sql found {1}", ME::enum_name(dbt), sql);
          break; // we found it. let's finish
        };
      }
      auto result_names = s.get_sequence_of_strings_or("result-names", {});
      auto param_names  = s.get_sequence_of_strings_or("parameter-names", {});
      if (! id)
      {
        log()->error(id.error().to_string());
        return std::unexpected(exit_status_enum::stmt_unique_id_is_missing);
      };
      if (sql.empty())
      {
        log()->error("SQL is missing. id: {}", id.value());
        return std::unexpected(exit_status_enum::no_sql_stmt_found);
      }
      data_statement statement;
      statement.set_id(id.value());
      statement.set_sql(sql);
      statement.set_dscr(dscr);
      statement.set_res_buf_size(result_size);
      statement.set_par_buf_size(param_size);
      statement.set_param_names(param_names);
      statement.set_result_names(result_names);
      if (! stmts.add_statement(statement))
      { /// duplicated statement id
        //       const auto msg = fmt::format("File: {} duplicate id {}", filename_, id.value());

        log()->error(fmt::format(get_exit_code_str(exit_status_enum::duplicated_stmt_id),
                                 filename_,
                                 id.value(),
                                 std::source_location::current().line(),
                                 std::source_location::current().column()));
        return std::unexpected(exit_status_enum::duplicated_stmt_id);
      }
    };
    return stmts;
  }
  /**
   * @brief fetch pointer to logger
   *
   * @return spdlog::logger*
   */
  spdlog::logger* parser::log() const { return log::get(); }

}; // namespace dbgen4