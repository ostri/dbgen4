#ifndef PARSER_HPP
#define PARSER_HPP
#include "common.hpp"
#include "data_statement.hpp"
#include "data_statements.hpp"
#include "db2_rtl.hpp"
#include "pars_result.hpp"
#include "parser_errors.hpp"
#include <yaml-cpp/node/detail/iterator_fwd.h>
#include <yaml-cpp/node/node.h>


namespace dbgen4
{
  using pars_result_t = std::pair<data_statements, parser_err_enum>;
  // using stmt_result_t = std::pair<data_statement, parser_err_enum>;
  struct stmt_result
  {
  public:
    stmt_result()
    : stmt_result({}, parser_err_enum::ok)
    {
    }
    explicit stmt_result(data_statement an_s)
    : stmt_result(std::move(an_s), parser_err_enum::ok)
    {
    }
    explicit stmt_result(parser_err_enum an_e)
    : stmt_result({}, an_e)
    {
    }
    stmt_result(data_statement an_s, parser_err_enum an_e)
    : s_(std::move(an_s))
    , e_(an_e)
    {
    }
    data_statement  s_; ///< contents of the statement if everything is ok NOLINT
    parser_err_enum e_; ///< error code NOLINT

    data_statement s() const { return s_; } // NOLINT

    parser_err_enum e() const { return e_; } // NOLINT
  } __attribute__((
    aligned(128))); // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
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
    pars_result parse_yaml_file(const str_t& filename, db_type_enum db_type);
    pars_result load_file_meta_data(const data_statements& s, rtl::db_db2& db) const;
    /// getters
    [[nodiscard]] str_t filename() const; ///< filename where gsql definition is stored
    /// setters
    void set_filename(const str_t& filename);
  protected:
    /// @brief loads the data from the yaml file structure to data structures
    /// @param n internal yaml file structure
    /// @return result of the operation, optional loaded data structure
    [[nodiscard]] pars_result parse_yaml_file(const YAML::Node& n, db_type_enum db_type);
    [[nodiscard]] pars_result process_statement(const YAML::Node& stmt,
                                                pars_result&      p,
                                                db_type_enum      db_type) const;
  private:
    [[nodiscard]] spdlog::logger* log() const;

    /// @brief extracts sql statements from the yaml node to data_statement structure
    /// @param stmt yaml node representing single statement
    /// @param s data_statement structure where sql statements will be stored
    /// @return new version of data_statement structure with loaded sql statements or error code
    [[nodiscard]] stmt_result extract_sql(const YAML::Node&     n,
                                          const data_statement& s,
                                          db_type_enum          db_type) const;
    [[nodiscard]] str_t       extract_sql(const YAML::Node& n, db_type_enum db_type) const;
    ///
    str_t filename_; //< filename where gsql definition is stored
  };
} // namespace dbgen4

#endif // PARSER_HPP
