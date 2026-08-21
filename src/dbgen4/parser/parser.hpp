#ifndef PARSER_HPP
#define PARSER_HPP
// #include "appl.hpp"
#include "common.hpp"
#include "data_statement.hpp"
#include "data_statements.hpp"
#include "rtl.hpp"
#include "parse_yaml.hpp"
#include "parser_errors.hpp"
#include <logger/logger.hpp>
#include <cstddef>
#include <expected>
#include <yaml-cpp/node/detail/iterator_fwd.h>
#include <yaml-cpp/node/node.h>


namespace dbgen4::gen
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
    explicit parser(logger::Logger& log)
    : logger_(log)
    {
    }
    ~parser()                        = default;
    parser(const parser&)            = delete;
    parser(parser&&) noexcept        = default;
    parser& operator=(const parser&) = delete;
    parser& operator=(parser&&)      = delete;
    /// @brief loads the data from the yaml file to data structures
    /// @param filename path to the yaml file
    /// @return result of the operation, optional loaded data structure
    [[nodiscard]] e_data_statements parse_yaml_file(const str_t& filename, db_type_enum db_type);
    [[nodiscard]] e_data_statements load_file_meta_data(const data_statements& s, rtl::db& db, size_t max_field_len) const;
    [[nodiscard]] e_data_statements parse_yaml_string(const str_t& yaml_str, db_type_enum db_type);
    /// getters
    [[nodiscard]] str_t filename() const; ///< YAML filename
    /// setters
    void set_filename(const str_t& filename);
  protected:
    [[nodiscard]] e_data_statements parse_yaml_file_json(const parse_yaml& n, db_type_enum db_type) const;
    /**
     * @brief the "sql/db2/psql/mariadb dialect, most specific wins" pattern,
     * factored out so it works the same for a statement's own sql and for
     * its before/after blocks (see parse_yaml_file_json())
     *
     * @param n the parse_yaml node carrying the dialect keys directly (a
     *          statement itself for sql, or its "before"/"after" sub-map)
     * @param db_type which dialect key overrides the generic "sql" one, if present
     * @return the resolved sql, or empty if neither the generic key nor db_type's own key exists
     */
    [[nodiscard]] str_t            resolve_dialect_sql(const parse_yaml& n, db_type_enum db_type) const;
    [[nodiscard]] exit_status_enum no_sql_found(e_data_statement& res) const;
    [[nodiscard]] e_data_statement process_statement(const YAML::Node& yaml_stmt, const data_statements& s, db_type_enum db_type) const;
  private:
    [[nodiscard]] logger::Logger& log_() const { return logger_; }
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    logger::Logger& logger_; ///< reference to the shared Logger, not owner
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
} // namespace dbgen4::gen

#endif // PARSER_HPP
