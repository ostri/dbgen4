#ifndef PARSER_HPP
#define PARSER_HPP
#include "../common/common.hpp"


namespace dbgen4
{
  using nlohmann::json;
  using nlohmann::json_schema::json_validator;
  /**
   * @brief parser of the gsql file
   *
   */
  class parser : log
  {
  public:
    parser()                         = default;
    ~parser()                        = default;
    parser(const parser&)            = delete;
    parser(parser&&) noexcept        = default;
    parser& operator=(const parser&) = delete;
    parser& operator=(parser&&)      = delete;
    int     load_grammar(const str_t& file);
    int     load_grammar();
    int     exec(const str_t& filename);
    int     exec(const json& o);
    /// getters
    [[nodiscard]] str_t filename() const;
    /// setters
    void set_filename(const str_t& filename);
  private:
    str_t          filename_{};  //< filename where gsql definition is stored
    json_validator validator_{}; //< json validator grammar
    json           file_{};      //< parsed json file or empty
  };
} // namespace dbgen4

#endif // PARSER_HPP
