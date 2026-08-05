#pragma once
#include "cmd_line_params.hpp"
#include "common.hpp"
// #include "data_model.hpp"
#include "context.hpp"
#include "data_statements.hpp"
#include "inja.hpp"
#include "parser_errors.hpp"
#include <logger/logger.hpp>
#include <expected>
#include <fmt/base.h>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace dbgen4
{
  /// enum of inja templates
  enum class inja_tpl_enum : uint8_t
  {
    main_hpp, // main header template
    main_cpp, // main source template
    buf_hpp,  // buffer hpp template
    buf_cpp   // buffer cpp template
  };
  /// filename templates for generated files
  enum class gen_fn_tpl_names : uint8_t
  {
    hpp,
    cpp,
    json,
  };
  using inja_tpl     = inja::Template;
  using map_inja_tpl = std::map<inja_tpl_enum, inja_tpl>;

  using map_fn       = std::map<gen_fn_tpl_names, str_t>;
  using map_tpl_2_fn = std::map<inja_tpl_enum, gen_fn_tpl_names>; // from tpl enum to fn enum
  using json         = nlohmann::ordered_json;
  using e_template   = std::expected<inja::Template, exit_status_enum>;
  using e_templates  = std::expected<map_inja_tpl, exit_status_enum>;
  using e_string     = std::expected<std::string, exit_status_enum>;
  using e_json       = std::expected<json, exit_status_enum>;
  using e_void       = std::expected<void, exit_status_enum>;

  class generator
  {
  public:
    generator(const context& ctx, logger::Logger& log);
    virtual ~generator()                   = default;
    generator(const generator&)            = default;
    generator(generator&&)                 = delete;
    generator& operator=(const generator&) = delete;
    generator& operator=(generator&&)      = delete;
    /// getters
    [[nodiscard]] data_statements*       s() const;
    [[nodiscard]] const cmd_line_params& cmd() const;
    [[nodiscard]] const context&         ctx() const;
    [[nodiscard]] str_t                  yaml_fn() const;
    [[nodiscard]] str_t                  out_folder() const;
    [[nodiscard]] db_type_enum           db_type() const;
    [[nodiscard]] str_t                  hpp_fn() const;
    [[nodiscard]] str_t                  cpp_fn() const;
    [[nodiscard]] str_t                  json_fn() const { return filename(gen_fn_tpl_names::json); }
    [[nodiscard]] str_t                  filename(gen_fn_tpl_names tpl_type) const;
    [[nodiscard]] map_fn                 get_fn_tpl() const;
    /// setters
    void set_s(data_statements* s);
    void set_yaml_fn_and_barename(cstr_t yaml_fn);
    void set_filename(const str_t& filename);
    /// utility methods
    e_void              prepare_templates();
    e_string            generate(const data_statements& s);
    void                attr_mappings(json& j_stmt, rtl::meta_dscr const& el);
    e_json              internal_model_to_json(const data_statements& s);
    e_string            generate_file_through_template(const json& data, inja_tpl_enum tpl_type);
    str_t               attr_dump_value_to_string(rtl::sql_type sql_type, const str_t& name);
    e_void              register_callbacks();
    [[nodiscard]] str_t template_filename(inja_tpl_enum tpl_id) const;
  private:
    [[nodiscard]] logger::Logger& log_() const { return log_ref_; }
    /// strip the rtl:: qualification - namespace dbx aliases these types
    static str_t    unqualified(rtl::cstr_t type_name);
    str_t           attr_storage_type(rtl::sql_type sql_type, const str_t& name);
    str_t           attr_getter_code(rtl::sql_type sql_type, const str_t& name);
    str_t           attr_setter_code(rtl::sql_type sql_type, const str_t& name);
    json            attr_mappings(rtl::meta_dscr const& el);
    // [[nodiscard]] gen_fn_tpl_names tpl_2_fn_enum(inja_tpl_enum v) const;
    [[nodiscard]] e_template       load_template(inja_tpl_enum tpl_id);
    [[nodiscard]] exit_status_enum error(const str_t&           filename_tpl,
                                         const str_t&           template_str,
                                         const inja::InjaError& e,
                                         exit_status_enum       code);

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const context& ctx_; ///< command line parameters stucture (readonly, not owner)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    logger::Logger&  log_ref_;   ///< reference to the shared Logger, not owner
    data_statements* s_{};      ///< data statements structure (readonly, not owner)
    str_t            yaml_fn_;  ///< yaml file name
    str_t            json_fn_;  ///< generated json file name
    json             j_data_;   ///< json data model
    str_t            barename_; ///< barename of yaml file which is core for hpp, cpp and json
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const map_fn fn_tpl_ ///< names of the generated files (hpp, cpp, etc)
                         ///< path/basename
      {
        {gen_fn_tpl_names::hpp, "{}/{}.hpp"},  // template for hpp filename
        {gen_fn_tpl_names::cpp, "{}/{}.cpp"},  // template for cpp filename
        {gen_fn_tpl_names::json, "{}/{}.json"} // template for json filename
      };
    inja::Environment env_{"template"}; ///< inja environment
    map_inja_tpl      templates_;       ///< array of templates

    str_t attr_storage_raw_type(rtl::sql_type sql_type, size_t len);

    str_t attr_base_type(rtl::sql_type sql_type);
  };

}; // namespace dbgen4