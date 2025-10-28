///
/// Created by ostri on 2024/02/04
///
#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#include "common.hpp"
#include <CLI/Error.hpp>

namespace dbgen4
{
  class cmd_line_params : log
  {
  public:
    cmd_line_params()                                  = default;
    virtual ~cmd_line_params()                         = default;
    cmd_line_params& operator=(const cmd_line_params&) = default;
    cmd_line_params& operator=(cmd_line_params&&)      = delete;
    cmd_line_params(const cmd_line_params& o)          = default;
    cmd_line_params(cmd_line_params&& o) noexcept      = default;
    [[nodiscard]] vec_str_t    files() const; //< fetch the list of gsql file to be processed
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
    [[nodiscard]] int   load_parameters(int argc, char** argv,
                                        char** env); //< load the parameters
    /// set log level based on verbose flag andtype of build
    void set_log_level(bool verbose) const;
  protected:
  private:
    str_t        db_name_;                    //< database name to connect to
    db_type_enum db_type_{db_type_enum::sql}; //< database type
    str_t        out_folder_;                 //< output folder for generated files
    bool         verbose_{false};             //< should we make verbose output
    vec_str_t    files_;                      //< set of files to be processed

  }; // namespace dbgen4

} // namespace dbgen4

#endif // PARAMETERS_HPP
