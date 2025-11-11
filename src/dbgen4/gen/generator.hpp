#pragma once
#include "cmd_line_params.hpp"
#include "common.hpp"
// #include "data_model.hpp"
#include "context.hpp"
#include "data_statements.hpp"
#include "inja.hpp"
#include "parser_errors.hpp"
#include <expected>
#include <fmt/base.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <spdlog/logger.h>

namespace dbgen4
{

  using map_fn     = std::map<tpl_types, str_t>;
  using json       = nlohmann::ordered_json;
  using e_template = std::expected<inja::Template, exit_status_enum>;
  using e_string   = std::expected<std::string, exit_status_enum>;
  using e_json     = std::expected<json, exit_status_enum>;

  class generator
  {
  public:
    explicit generator(const context& ctx);
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
    [[nodiscard]] str_t                  hpp_fn() const { return filename(tpl_types::hpp); }
    [[nodiscard]] str_t                  cpp_fn() const { return filename(tpl_types::cpp); }
    [[nodiscard]] str_t                  json_fn() const { return filename(tpl_types::json); }
    [[nodiscard]] str_t                  filename(tpl_types tpl_type) const;
    [[nodiscard]] map_fn                 get_fn_tpl() const;
    /// setters
    void set_s(data_statements* s);
    void set_yaml_fn_and_barename(cstr_t yaml_fn);
    void set_filename(const str_t& filename);
    /// utility methods
    e_template prepare_templates();
    e_string   generate(const data_statements& s);
    e_json     internal_model_to_json(const data_statements& s);
    e_string   generate_file_through_template(const json& data, tpl_types tpl_type);
  private:
    spdlog::logger* log() { return log::get(); }; /// Member variables

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const context&   ctx_;      ///< command line parameters stucture (readonly, not owner)
    data_statements* s_{};      ///< data statements structure (readonly, not owner)
    str_t            yaml_fn_;  ///< yaml file name
    str_t            json_fn_;  ///< generated json file name
    json             j_data_;   ///< json data model
    str_t            barename_; ///< barename of yaml file which is core for hpp, cpp and json
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const map_fn fn_tpl_ ///< names of the generated files (hpp, cpp, etc)
                         ///< path/basename
      {
        {tpl_types::hpp, "{}/{}.hpp"},  // template for hpp filename
        {tpl_types::cpp, "{}/{}.cpp"},  // template for cpp filename
        {tpl_types::json, "{}/{}.json"} // template for json filename
      };
  };
}; // namespace dbgen4