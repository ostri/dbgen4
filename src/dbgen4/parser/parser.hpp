#ifndef PARSER_HPP
#define PARSER_HPP
// #include "appl.hpp"
#include "common.hpp"
#include "data_statement.hpp"
#include "data_statements.hpp"
#include "db2_rtl.hpp"
#include "parse_yaml.hpp"
#include "parser_errors.hpp"
#include <cstddef>
#include <expected>
#include <yaml-cpp/node/detail/iterator_fwd.h>
#include <yaml-cpp/node/node.h>


namespace dbgen4
{
  using e_data_statement  = std::expected<data_statement, exit_status_enum>;
  using e_data_statements = std::expected<data_statements, exit_status_enum>; ///< expected data_statements or error code

  /**
   * @brief parser of the yaml file
   *
   */
  class parser
  {
  public:
    parser()                         = default;
    ~parser()                        = default;
    parser(const parser&)            = delete;
    parser(parser&&) noexcept        = default;
    parser& operator=(const parser&) = delete;
    parser& operator=(parser&&)      = delete;
    /// @brief loads the data from the yaml file to data structures
    /// @param filename path to the yaml file
    /// @return result of the operation, optional loaded data structure
    [[nodiscard]] e_data_statements parse_yaml_file(const str_t& filename, db_type_enum db_type);
    [[nodiscard]] e_data_statements load_file_meta_data(const data_statements& s, rtl::db_db2& db) const;
    [[nodiscard]] e_data_statements parse_yaml_string(const str_t& yaml_str, db_type_enum db_type);
    /// getters
    [[nodiscard]] str_t filename() const; ///< YAML filename
    /// setters
    void set_filename(const str_t& filename);
  protected:
    [[nodiscard]] e_data_statements parse_yaml_file_json(const parse_yaml& n, db_type_enum db_type) const;
    [[nodiscard]] exit_status_enum  no_sql_found(e_data_statement& res) const;
    [[nodiscard]] e_data_statement  process_statement(const YAML::Node& yaml_stmt, const data_statements& s, db_type_enum db_type) const;
  private:
    static class log::log* log_() { return log::get(); };
    /// Member variables
    /// @brief extracts sql statements from the yaml node to data_statement structure
    /// @param stmt yaml node representing single statement
    /// @param s data_statement structure where sql statements will be stored
    /// @return new version of data_statement structure with loaded sql statements or error code
    [[nodiscard]] e_data_statement extract_sql_to_statement(const YAML::Node& n, const data_statement& s, db_type_enum db_type) const;
    [[nodiscard]] str_t            extract_sql(const YAML::Node& n, db_type_enum db_type) const;
    exit_status_enum               log_yaml_segment(const YAML::Node& n);
    [[nodiscard]] exit_status_enum log_id_is_missing(const YAML::Node& stmt, size_t pos) const;
    /// member(s)
    str_t filename_; //< YAML filename

    [[nodiscard]] exit_status_enum no_sql_found(const str_t& stmt_id) const;
  };
} // namespace dbgen4

#endif // PARSER_HPP
