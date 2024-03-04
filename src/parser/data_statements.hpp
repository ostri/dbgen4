#ifndef DATA_STATEMENTS_HPP
#define DATA_STATEMENTS_HPP

#include "../common/common.hpp"
#include "data_statement.hpp"
#include <map>

namespace dbgen4
{
  class data_statements;
  // using data_statement_vec_t = std::vector<data_statement>;
  using data_statement_map_t = std::map<str_t, data_statement>;
  using krneki               = std::pair<data_statement_map_t::const_iterator, bool>;

  class data_statements : private log
  {
  public:
    /// getters
    [[nodiscard]] str_t summary() const;
    [[nodiscard]] str_t description() const;
    /// setters
    void set_summary(const str_t& summary);
    void set_description(const str_t& description);
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
    str_t                summary_{};     ///< what is the purpose of this sql statements set
    str_t                description_{}; ///< description of the usage of this sql statement set
    data_statement_map_t map_{};         ///< individual statements
  };
} // namespace dbgen4
#endif // DATA_STATEMENTS_HPP
