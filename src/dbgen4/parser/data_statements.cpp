#include "common.hpp"
#include "data_statements.hpp"
#include "data_statement.hpp"
#include <cstddef>

namespace dbgen4
{

  std::string data_statements::dump(size_t offs) const
  {
    str_t      left_padding(offs, ' ');
    const auto text_ident = 4 + offs;
    /// walk over all statemnts and serialize them
    std::string stmt_str;
    for (const auto& [id, stmt] : map_statements_)
    { // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
      stmt_str += stmt.dump(text_ident);
    };
    /// prepare summary and description
    auto summary_str     = offset_text(summary_, text_ident + 2);
    auto description_str = offset_text(description_, text_ident + 2);
    auto msg             = fmt::format(R"(
  {0}Document description
  {0}  summary: {1}
  {0}  dscription: {2}
  {0}  filename: {4}
  {0}  statements:{4}
  )", /// this new line is on purpose to delimit sql statement from the metadata
                           left_padding,    /// document description
                           summary_str,     /// summary
                           description_str, /// description
                           filename_,       //// filename
                           stmt_str         /// statements
    );
    return msg;
  }
  /// getters
  str_t                data_statements::summary() const { return summary_; }
  str_t                data_statements::description() const { return description_; }
  str_t                data_statements::filename() const { return filename_; }
  data_statement_map_t data_statements::map_statements() const { return map_statements_; }
  spdlog::logger*      data_statements::log() const { return log::get(); }
  /// setters
  void data_statements::set_summary(const str_t& summary) { summary_ = summary; }
  void data_statements::set_description(const str_t& description) { description_ = description; }
  void data_statements::set_map(const data_statement_map_t& map) { map_statements_ = map; }

  /**
   * @brief add statement to the map (with replace if duplicate)
   * The statement is added to the list, unless the statement with the same id already exists. If
   * added true is returned, else false. Provided statement values are always written in the map.
   * @param s statement to be added
   * @return true the statement was added new
   * @return false the statement replaced existing one
   */
  bool data_statements::add_statement_with_replace(data_statement s)
  {
    auto [it, inserted] = map_statements_.try_emplace(s.id(), std::move(s));
    if (! inserted)
    {
      it->second = s; // overwrite existing
    }
    return inserted;
  }
  /**
   * @brief add statement to the map (no duplicate allowed)
   * The statement is added to the list, unless the statement with the same id already exists. If
   * added true is returned, else false. If duplicate exists, it is not replaced.
   * @param s statement to be added
   * @return true the statement was added new
   * @return false the statement with the same id already exists
   */
  bool data_statements::add_statement(const data_statement& s)
  {
    auto [it, success] = map_statements_.emplace(s.id(), s);
    if (! success) { log()->error("Statement id: {} is duplicated.", s.id()); }
    return success;
  }
}; // namespace dbgen4
