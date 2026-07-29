///
/// Created by ostri on 2024/02/04
///
#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#include "common.hpp"
#include "parser_errors.hpp"
#include <CLI/Error.hpp>
#include <cstddef>

namespace dbgen4
{
  class cmd_line_params
  {
  public:
    cmd_line_params()                                  = default;
    virtual ~cmd_line_params()                         = default;
    cmd_line_params& operator=(const cmd_line_params&) = default;
    cmd_line_params& operator=(cmd_line_params&&)      = default;
    cmd_line_params(const cmd_line_params& o)          = default;
    cmd_line_params(cmd_line_params&& o) noexcept      = default;
    [[nodiscard]] vec_str_t    files() const;      ///< fetch the list of YAML files to be processed
    [[nodiscard]] db_type_enum db_type() const;    ///< fetch the RDBMS type to generate code for
    [[nodiscard]] str_t        db_name() const;    ///< fetch database name
    [[nodiscard]] str_t        out_folder() const; ///< fetch the output folder name
    [[nodiscard]] bool         is_verbose() const; ///< fetch verbosity
    [[nodiscard]] str_t        user() const;
    [[nodiscard]] str_t        pass() const;
    [[nodiscard]] str_t        host() const;
    [[nodiscard]] size_t       port() const;
    /// width to assume for columns the database reports no declared length for
    [[nodiscard]] size_t       max_field_len() const;
    /**
     * @brief dump the object content to the string
     *
     * @return str_t
     */
    [[nodiscard]] str_t            dump(size_t offs) const;
    [[nodiscard]] exit_status_enum load_parameters(int argc, char** argv,
                                                   char** env); //< load the parameters
    /// set log level based on verbose flag andtype of build
    void set_log_level(bool verbose) const;
  private:
    static class log::log* log_() { return log::get(); };
    str_t                  db_name_;                    //< database name to connect to
    db_type_enum           db_type_{db_type_enum::sql}; //< database type
    str_t                  out_folder_;                 //< output folder for generated files
    bool                   verbose_{false};             //< should we make verbose output
    str_t                  user_;                       ///< db username
    str_t                  pass_;                       ///< db password
    size_t                 port_{};                     ///< port to which to connect
    str_t                  host_;                       ///< host to which connect
    vec_str_t              files_;                      //< set of files to be processed
    /// Fallback width for types with no declared length - PostgreSQL text,
    /// json, bytea, MariaDB TEXT/BLOB. Without it those columns would size
    /// their buffers at the protocol maximum.
    size_t max_field_len_{default_max_field_len};

  }; // namespace dbgen4

} // namespace dbgen4

#endif // PARAMETERS_HPP
