///
/// Created by ostri on 2024/02/04
///
#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#include "../common/common.hpp"
#include <CLI/Error.hpp>

namespace dbgen4
{
  class parameters : log
  {
  public:
    parameters()                             = default;
    virtual ~parameters()                    = default;
    parameters& operator=(const parameters&) = delete;
    parameters& operator=(parameters&&)      = delete;
    parameters(const parameters& o)          = delete;
    parameters(parameters&& o) noexcept      = delete;
    [[nodiscard]] vec_str_t files()
      const; //< fetch the list of gsql file to be processed
    [[nodiscard]] db_type_enum db_type()
      const; //< fetch the database type to which to generate code
    [[nodiscard]] str_t db_name()
      const; //< fetch the database name that holds the table definitions
    [[nodiscard]] str_t out_folder() const; //< fetch the outpu folred name
    [[nodiscard]] bool  is_verbose() const; //< fetch verbosity
    /**
     * @brief dump the object content to the string
     *
     * @return str_t
     */
    [[nodiscard]] str_t dump(int offs) const;
    [[nodiscard]] int   load_parameters(int    argc,
                                        char** argv,
                                        char** env); //< load the parameters
  protected:
  private:
    str_t        db_name_{};                   //< database name to connect to
    db_type_enum db_type_{db_type_enum::none}; //< database type
    str_t        out_folder_{}; //< output folder for generated files
    bool         verbose_{};    //< should we make verbose output
    vec_str_t    files_{};      //< set of files to be processed

  }; // namespace dbgen4

} // namespace dbgen4

#endif // PARAMETERS_HPP
