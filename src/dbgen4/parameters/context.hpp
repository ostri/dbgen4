#pragma once
#include <expected>
#include <fmt/base.h>
#define MAGIC_ENUM_RANGE_MIN -400
#define MAGIC_ENUM_RANGE_MAX 100
#include <magic_enum.hpp>
#include <spdlog/logger.h>
#include "cmd_line_params.hpp"
#include "inja.hpp"
// #include "log.hpp"
#include "parser_errors.hpp"
namespace dbgen4
{
  enum class tpl_types : uint8_t
  {
    hpp,
    cpp,
    json
  };
  using inja_tpl     = inja::Template;
  using map_inja_tpl = std::map<tpl_types, inja_tpl>;

  class context
  {
  public:
    explicit context(const cmd_line_params& cmd)
    : cmd_(cmd)
    {
    }
    std::expected<map_inja_tpl, exit_status_enum> prepare_templates()
    {
      str_t filename_tpl; /// must be outside due to catch
      str_t template_str; /// template file read as a string
      try
      {
        /// prepare templates only for hpp and cpp files
        for (const auto& tpl_type : {tpl_types::hpp, tpl_types::cpp})
        {
          auto tpl_type_name = ME::enum_name(tpl_type);
          filename_tpl       = fmt::format("template/{}_template.jinja", tpl_type_name);
          log()->trace("preparing template '{}'", filename_tpl);
          auto res = read_file(filename_tpl);
          if (! res)
          {
            const auto* fmt = get_exit_code_str(exit_status_enum::error_reading_file);
            log()->error(fmt::runtime_format_string<char>(fmt), filename_tpl, res.error());
            return std::unexpected(exit_status_enum::error_reading_file);
          }

          log()->trace("template read:\n'{}'", res.value());
          inja::Template tmpl = env_.parse(res.value());
          templates_.emplace(tpl_type, tmpl);
          log()->debug("template '{}' successfuly read and evaluated.", filename_tpl);
        }
      }
      catch (const inja::RenderError& e)
      {
        log()->critical("Render error file: '{}' error: '{}' line: {} col: {} template: \n{}.",
                        filename_tpl,
                        e.what(),
                        e.location.line,
                        e.location.column,
                        template_str);
        throw;
      }
      catch (const inja::InjaError& e)
      {
        log()->critical(
          "General inja error file; '{}' error: '{}' line: {} col: {} template: \n{}.",
          filename_tpl,
          e.what(),
          e.location.line,
          e.location.column,
          template_str);
        throw;
      }
      catch (const std::exception& e)
      {
        log()->critical("Unknown exception file: '{}' error: '{}' template: \n{}.",
                        filename_tpl,
                        e.what(),
                        template_str);
        throw;
      }
      return templates_;
    }
    [[nodiscard]] const cmd_line_params& cmd() const;
    [[nodiscard]] inja::Environment      env() const;
    [[nodiscard]] const map_inja_tpl&    templates() const;
    // --- read only - throws std::out_of_rangee if provided indeks does not exist ---
    const inja_tpl& operator[](tpl_types tpl_type) const
    {
      log()->debug("Returning filename template for '{}'", ME::enum_name(tpl_type));
      return templates_.at(tpl_type);
    }
  private:
    [[nodiscard]] spdlog::logger* log() const { return log::get(); }
    /// members
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const cmd_line_params& cmd_;       ///< reference to parameters
    inja::Environment      env_;       ///< inja environment
    map_inja_tpl           templates_; ///< array of templates
  };
} // namespace dbgen4