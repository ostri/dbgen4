#include "parse_yaml.hpp"
#include <cstddef>
#include <expected>
#include <fmt/format.h>
#include <source_location>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/detail/iterator.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>
#include "common.hpp"
#include "data_statement.hpp"
#include "data_statements.hpp"
#include "rtl.hpp"
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
  e_data_statements parser::parse_yaml_file(const str_t& filename, db_type_enum db_type)
  {
    filename_ = filename;
    auto res  = parse_yaml::load(filename_, log_());
    if (! res) return std::unexpected(exit_status_enum::file_cant_be_open);
    return parse_yaml_file_json(res.value(), db_type);
  }
  e_data_statements parser::parse_yaml_string(const str_t& yaml_str, db_type_enum db_type)
  {
    auto res = parse_yaml::load_from_string(yaml_str, log_());
    if (! res)
    {
      log_().debug("File {} syntax error.", this->filename());
      return std::unexpected(exit_status_enum::yaml_syntax_error);
    }
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
  e_data_statements parser::load_file_meta_data(const data_statements& s, rtl::db& db, size_t max_field_len) const
  {
    data_statements res_stmts{s}; // result statements with updated metadata
    for (const auto& map_stmt_pair : s.map_statements())
    { /// walking through whole list of statements in one file
      const auto& stmt       = map_stmt_pair.second;
      const auto  before_sql = stmt.before_sql();
      const auto  after_sql  = stmt.after_sql();

      if (! before_sql.empty())
      {
        if (auto before_sts = db.exec(before_sql); ! rtl::is_success(before_sts))
        {
          log_().error("before sql '{}' failed: {}", before_sql, ME::enum_name<rtl::db_sts>(before_sts));
          db.rollback();
          return std::unexpected(exit_status_enum::sql_syntax_err);
        }
      }

      auto sql = stmt.sql();
      auto res = db.get_sql_metadata(sql);

      // after runs whether or not the statement's own sql validated, and
      // still in the same unit of work before_sql (if any) opened - some
      // backends (DB2) roll a unit of work's DDL back together, so before_sql
      // 's side effects (e.g. a staging table) would otherwise vanish before
      // after_sql gets a chance to tear them down cleanly.
      if (! after_sql.empty())
      {
        if (auto after_sts = db.exec(after_sql); ! rtl::is_success(after_sts))
          log_().error("after sql '{}' failed: {}", after_sql, ME::enum_name<rtl::db_sts>(after_sts));
        // not itself fatal to this statement's own result - the statement
        // already validated (or failed to) on its own merits above; a
        // cleanup failure is logged, not folded into that outcome.
      }

      // only now end the unit of work before_sql/get_sql_metadata/after_sql
      // ran in - commit so before_sql's and after_sql's own DDL both stick
      // (harmless if there was neither), rollback if this statement's sql
      // itself failed to validate.
      if (! res)
      {
        db.rollback();
        log_().error(
          "Invalid sql '{}' status: {} mnemonic {}", sql, ME::enum_integer(res.error()), ME::enum_name<rtl::db_sts>(res.error()));
        return std::unexpected(exit_status_enum::sql_syntax_err);
      }
      db.commit();

      /// all ok. Update the sql description with metadata
      log_().trace("meta data: {}", res.value().dump());
      data_statement res_stmt(stmt);
      res_stmt.set_results(res.value().columns());
      res_stmt.set_params(res.value().params());
      res_stmt.push_column_names(log_());              // overwrite database column names with user defined (if they exist)
      res_stmt.apply_field_len(max_field_len, log_()); // settle the width of columns the database gave none for
      res_stmts.add_statement_with_replace(res_stmt);  /// we are replacing existing statement values (meta data added,
                                                       /// everything else the same)
    };
    log_().info("{} sql statements processed", s.map_statements().size());
    return res_stmts;
  }

  str_t parser::filename() const { return filename_; }

  void parser::set_filename(const str_t& filename) { filename_ = filename; }

  str_t parser::resolve_dialect_sql(const parse_yaml& n, db_type_enum db_type) const
  {
    // must be first - sql value or empty after then specializations
    auto sql = n.get_or<std::string>(str_t(ME::enum_name(db_type_enum::sql)), "");
    log_().trace("General sql '{0}'", sql);
    for (auto dbt : ME::enum_values<db_type_enum>())
    {
      if (db_type == dbt)
      {
        sql = n.get_or<std::string>(str_t(ME::enum_name(db_type)), sql);
        log_().trace("Specialized {0} sql found {1}", ME::enum_name(dbt), sql);
        break; // we found it. let's finish
      };
    }
    return sql;
  }

  /// @brief loads the data from the yaml file structure to data structures
  /// @param n internal yaml file structure
  /// @return result of the operation, optional loaded data structure
  e_data_statements parser::parse_yaml_file_json(const parse_yaml& n, db_type_enum db_type) const
  {
    data_statements stmts{};

    stmts.set_summary(n.get_or<std::string>("summary", ""));
    stmts.set_description(n.get_or<std::string>("description", ""));
    stmts.set_filename(filename_);
    auto ys = n.get_seq_of_maps("statements");
    if (! ys)
    {
      constexpr exit_status_enum err = exit_status_enum::statements_attr_missing;
      auto msg = fmt::format(get_exit_code_str(err), ys.error().filename, ys.error().to_string(), ys.error().line, ys.error().column);
      log_().error(msg);
      return std::unexpected(err);
    }
    for (const auto& s : ys.value())
    { /// walk over all sql statement description
      auto id = s.get<std::string>("id");
      /// absent means one row - the buffer a statement gets when the yaml says
      /// nothing about batching
      auto result_size = s.get_or<size_t>("res-buf-size", 1);
      auto param_size  = s.get_or<size_t>("par-buf-size", 1);
      auto summary     = s.get_or<std::string>("summary", "");
      auto dscr        = s.get_or<std::string>("dscr", "");
      auto sql         = resolve_dialect_sql(s, db_type);
      // "before"/"after" are optional sub-maps with the same sql/db2/psql/mariadb
      // dialect keys as the statement itself - see data_statement::before_sql()/
      // after_sql() for what they are for. Absent from most statements - a
      // default constructed parse_yaml{} carries no Logger to log through
      // (see parse_yaml.hpp), so resolve_dialect_sql() is only ever called on
      // one actually read from the yaml file, never as a stand-in for "missing".
      const str_t before_sql   = s.is_map("before") ? resolve_dialect_sql(s.get_map("before").value(), db_type) : "";
      const str_t after_sql    = s.is_map("after") ? resolve_dialect_sql(s.get_map("after").value(), db_type) : "";
      auto        field_len    = s.get_map_of_sizes_or("field-len");
      auto        result_names = s.get_seq_of_strings_or("result-names", {});
      auto        param_names  = s.get_seq_of_strings_or("parameter-names", {});
      if (! id)
      {
        log_().error(id.error().to_string());
        return std::unexpected(exit_status_enum::stmt_unique_id_is_missing);
      };
      if (sql.empty())
      {
        log_().error("SQL is missing. id: {}", id.value());
        return std::unexpected(exit_status_enum::no_sql_stmt_found);
      }
      data_statement statement;
      statement.set_id(id.value());
      statement.set_sql(sql);
      statement.set_before_sql(before_sql);
      statement.set_after_sql(after_sql);
      statement.set_summary(summary);
      statement.set_dscr(dscr);
      statement.set_res_buf_size(result_size);
      statement.set_par_buf_size(param_size);
      statement.set_param_names(param_names);
      statement.set_result_names(result_names);
      statement.set_field_len(field_len);
      if (! stmts.add_statement(statement, log_()))
      { /// duplicated statement id
        //       const auto msg = fmt::format("File: {} duplicate id {}", filename_, id.value());

        log_().error(fmt::format(get_exit_code_str(exit_status_enum::duplicated_stmt_id),
                                 filename_,
                                 id.value(),
                                 std::source_location::current().line(),
                                 std::source_location::current().column()));
        return std::unexpected(exit_status_enum::duplicated_stmt_id);
      }
    };
    return stmts;
  }

}; // namespace dbgen4