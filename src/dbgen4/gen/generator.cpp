#include "generator.hpp"
#include "common.hpp"
#include "context.hpp"
#include "inja.hpp"
#include "parser_errors.hpp"
#include <expected>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace dbgen4
{
  namespace fs = std::filesystem;
  /**
   * @brief generate hpp and cpp file
   *
   * @param s statements - liat of sql statements that we need to generate the c++ code
   * @return e_string or error - if successful - empty string error code otherwise
   */
  e_string generator::generate(const data_statements& s)
  {
    auto json = internal_model_to_json(s);
    if (! json) return std::unexpected(json.error());

    /// does output folder exist
    fs::path path(cmd().out_folder());
    if (! fs::exists(path))
    {
      log()->info("Provided output folder '{}' does not exist. Starting to create.");
      std::error_code ec;
      auto            ret = fs::create_directories(path, ec);
      if (! ret)
      {
        auto msg = fmt::format("Program was not able to create folder '{}' msg: {} code: {}",
                               path.string(),
                               ec.message(),
                               ec.value());
        log()->critical(msg);
        throw std::runtime_error(msg);
      }
      log()->info("Output folder '{}' successfully created.", path.string());
    };

    /// only for hpp and cpp
    for (auto tpl_type : {tpl_types::hpp, tpl_types::cpp})
    {
      auto fn  = filename(tpl_type);
      auto res = generate_file_through_template(json.value(), tpl_type);
      if (! res) return std::unexpected(res.error());
      auto r = write_file(fn, res.value());
      if (! r) return std::unexpected(exit_status_enum::ok);
      log()->trace("File: {} written\n '{}'", fn, res.value());
    }
    return "";
  }
  /**
   * @brief return map of filename templates
   *
   * @return map_fn
   */
  map_fn         generator::get_fn_tpl() const { return fn_tpl_; }
  const context& generator::ctx() const { return ctx_; }
  generator::generator(const context& ctx)
  : ctx_(ctx) { };
  data_statements* generator::s() const { return s_; }


  str_t generator::yaml_fn() const { return yaml_fn_; }

  str_t generator::out_folder() const { return ctx().cmd().out_folder(); }

  db_type_enum generator::db_type() const { return cmd().db_type(); }

  str_t generator::filename(tpl_types tpl_type) const
  {
    auto fmt = fn_tpl_.at(tpl_type);
    return fmt::format(fmt::runtime_format_string<char>(fmt), cmd().out_folder(), barename_);
  }

  void generator::set_s(data_statements* s) { s_ = s; }

  void generator::set_yaml_fn_and_barename(cstr_t yaml_fn)
  {
    yaml_fn_ = yaml_fn;
    fs::path path(yaml_fn_);
    barename_ = path.stem().string();
  }

  e_string generator::generate_file_through_template(const json& data, tpl_types tpl_type)
  {
    try
    {
      auto res = ctx().env().render(ctx_[tpl_type], data);
      return res;
    }
    catch (const inja::RenderError& e)
    {
      log()->error("RENDER ERROR: {} file: {}", e.what(), filename(tpl_type));
      return std::unexpected(exit_status_enum::inja_render_error);
    }
    catch (const inja::ParserError& e)
    {
      log()->error("PARSER ERROR: {} file: {}", e.what(), filename(tpl_type));
      return std::unexpected(exit_status_enum::inja_parser_error);
    }
    catch (const inja::FileError& e)
    {
      log()->error("FILE ERROR: {} file: {}", e.what(), filename(tpl_type));
      return std::unexpected(exit_status_enum::inja_file_error);
    }
    catch (const inja::DataError& e)
    {
      log()->error("Data error: {} file: {}", e.what(), filename(tpl_type));
      return std::unexpected(exit_status_enum::inja_data_error);
    }
    // catch (const inja::inja_exception& e)
    // {
    //   log()->error("INJA EXCEPTION: {}", e.what());
    //   return std::unexpected(exit_status_enum::inja_unknown_error);
    // }
    catch (const std::exception& e)
    {
      log()->error("STD EXCEPTION: {} file: ", e.what(), filename(tpl_type));
      return std::unexpected(exit_status_enum::unhandled_exception);
    }
  }

  const cmd_line_params& generator::cmd() const { return ctx().cmd(); }

  // void generator::set_cmd(cmd_line_params* cmd) { cmd_ = cmd; }

  e_json generator::internal_model_to_json(const data_statements& s)
  {
    json j;
    j["summary"]     = s.summary();
    j["description"] = join(prefix_split(s.description(), '\n', " *  "), "\n");
    j["version"]     = "0.1.0"; // FIXME(ostri) magic string
    j["timestamp"] =
      fmt::format("{:%Y-%m-%d %H:%M:%Z %Z}",
                  std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));
    j["statements"] = json::array();
    for (const auto& stmt : s.map() | std::views::values)
    {
      json jstmt;
      jstmt["id"]     = stmt.id();
      jstmt["sql"]    = prefix_text(stmt.sql(), 10); // no offset
      jstmt["dscr"]   = stmt.dscr().empty() ? "" : prefix_text(stmt.dscr(), 10);
      jstmt["column"] = json::array();
      jstmt["param"]  = json::array();
      // jstmt["column"] = stmt.columns;
      for (const auto& col : stmt.columns())
      {
        json jcol;
        jcol["index"]          = col.index;
        jcol["name"]           = col.name;
        jcol["type"]           = ME::enum_name(col.type);
        jcol["type_name"]      = get_sql_type_mnemonic(col.type);
        jcol["odbc_name_type"] = col.odbc_type;
        jcol["size"]           = col.size;
        jcol["digits"]         = col.digits;
        jcol["nullable"]       = col.nullable;
        jstmt["column"].push_back(jcol);
      }
      for (const auto& par : stmt.params())
      {
        json jpar;
        jpar["index"]          = par.index;
        jpar["name"]           = par.name;
        jpar["type"]           = ME::enum_name(par.type);
        jpar["type_name"]      = get_sql_type_mnemonic(par.type);
        jpar["odbc_name_type"] = par.odbc_type;
        jpar["size"]           = par.size;
        jpar["digits"]         = par.digits;
        jpar["nullable"]       = par.nullable;
        jstmt["param"].push_back(jpar);
      }

      j["statements"].push_back(jstmt);
    }
    return j;
  };


} // namespace dbgen4
