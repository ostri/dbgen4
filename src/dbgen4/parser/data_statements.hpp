#pragma once

#include "common.hpp"
#include <map>
#include "data_statement.hpp"
#include "parameters.hpp"

namespace dbgen4
{
  class data_statements;
  // using data_statement_vec_t = std::vector<data_statement>;
  using data_statement_map_t = std::map<str_t, data_statement>;
  // using krneki               = std::pair<data_statement_map_t::const_iterator, bool>;

  class data_statements : private log
  {
  public:
    data_statements()                                      = default;
    virtual ~data_statements()                             = default;
    data_statements(const data_statements&)                = default;
    data_statements(data_statements&&) noexcept            = delete;
    data_statements& operator=(const data_statements&)     = delete;
    data_statements& operator=(data_statements&&) noexcept = delete;
    /// getters
    [[nodiscard]] str_t           summary() const;
    [[nodiscard]] str_t           description() const;
    [[nodiscard]] cmd_line_params params() const;
    /// setters
    void set_summary(const str_t& summary);
    void set_description(const str_t& description);
    void set_params(const cmd_line_params& params);
    /**
     * @brief add statement to the map
     *
     * It adds the statement to the list, unless the statement with the same id already exists.
     * In both cases it returns std::pair with pointer to statement and boolean value, whether it
     * was added (second: true) or the statement with the same id already exists (second:false).
     *
     * @param s statement to be added
     * @return - added -> pointer to the added statement and true
     *         - duplicate -> pointer to the existing statement and false.
     */
    bool add_statement(const data_statement& s);
  protected:
    [[nodiscard]] data_statement_map_t map() const;
    void                               set_map(const data_statement_map_t& map);
  private:
    // NOLINTNEXTLINE (readability-redundant-member-init)
    str_t                summary_;     ///< description about the purpose of this sql statements set
    str_t                description_; ///< description of the usage of this sql statement set
    data_statement_map_t map_;         ///< individual statements
    cmd_line_params      params_;      ///< command line parameters
  };
} // namespace dbgen4
