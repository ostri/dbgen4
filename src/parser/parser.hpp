#ifndef PARSER_HPP
#define PARSER_HPP
#include "common.hpp"
#include "data_statements.hpp"
#include <yaml-cpp/node/detail/iterator_fwd.h>
#include <yaml-cpp/node/node.h>


namespace dbgen4
{
  using pars_result_t = std::pair<data_statements, parser_errors_enum>;
  /**
   * @brief parser of the yaml file
   *
   */
  class parser : log
  {
  public:
    parser()                               = default;
    ~parser()                              = default;
    parser(const parser&)                  = delete;
    parser(parser&&) noexcept              = default;
    parser&       operator=(const parser&) = delete;
    parser&       operator=(parser&&)      = delete;
    pars_result_t exec(const str_t& filename);
    /// getters
    [[nodiscard]] str_t filename() const;
    /// setters
    void set_filename(const str_t& filename);
  protected:
    /// @brief loads the data from the yaml file structure to data structures
    /// @param n internal yaml file structure
    /// @return result of the operation, optional loaded data structure
    pars_result_t exec(const YAML::Node& n);
  private:
    bool extract_sqls(const YAML::detail::iterator_value& stmt, data_statement& s);
  private:
    str_t filename_{}; //< filename where gsql definition is stored
  };
} // namespace dbgen4

#endif // PARSER_HPP
