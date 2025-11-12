#include "generator.hpp"
#include "cli_constants.hpp"
#include "common.hpp"
#include "context.hpp"
#include "inja.hpp"
#include "parser_errors.hpp"
// #include <climits>
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
  /**
   * @brief fetch reference to command line instance
   *
   * @return const cmd_line_params&
   */
  const cmd_line_params& generator::cmd() const { return ctx().cmd(); }
  /**
   * @brief generates member storage type <atomic type> | <string type>
   *
   * - atomic or structure type : just atomic type (e.g. real)
   * - string type (char, wchar, binary) : <basic-type>[<len>+1] (e.g. char[l_col+1])
   * - binary string type (binary) : <basic-type>[<len>] (e.g. char[l_col])
   * @param sql_type
   * @param name
   * @return str_t
   */
  str_t generator::attr_storage_type(rtl::sql_type sql_type, const str_t& name)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    case rtl::sql_cat::atomic:
    case rtl::sql_cat::structure: return fmt::format("{}", dscr->cpp_type_name);
    case rtl::sql_cat::c_string:
    case rtl::sql_cat::w_string: return fmt::format("{0}[l_{1}+1]", dscr->cpp_type_name, name);
    case rtl::sql_cat::b_string: return fmt::format("{0}[l_{1}]", dscr->cpp_type_name, name);
    default: __builtin_unreachable();
    }
  }
  /**
   * @brief getter implementation code
   * atomic, structure : return <name>.at(row);
   * strings (char, wchar, binary): return { &<name>_.at(row)[0], l_<name>};
   *
   * @param sql_type type of the column/param
   * @param name  name of column/param
   * @return str_t implementation code
   */
  str_t generator::attr_getter_code(rtl::sql_type sql_type, const str_t& name)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    case rtl::sql_cat::atomic:
    case rtl::sql_cat::structure: return fmt::format("{{ return {0}_.at(row);}}", name);
    case rtl::sql_cat::c_string:
    case rtl::sql_cat::w_string:
    case rtl::sql_cat::b_string:
      return fmt::format("{{ return {{&{0}_.at(row)[0], l_{0} }};}}",
                         name); // FIXME actual length not max length
    default: __builtin_unreachable();
    }
  }
  str_t generator::attr_setter_code(rtl::sql_type sql_type, const str_t& name)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    case rtl::sql_cat::atomic:
    case rtl::sql_cat::structure: return fmt::format("{{ {0}_.at(row) = v;}}", name);
    case rtl::sql_cat::c_string:
      return fmt::format( //
        "{{"
        "auto l = std::min(v.size(), l_{0}); "
        "if (l > std::numeric_limits<std::int32_t>::max()) throw std::out_of_range(\"Value size is "
        "too big.\");"
        "len_{0}_.at(row) = static_cast<int32_t>(l);"
        "auto pos = v.copy(&{0}_.at(row)[0], l,0);"
        "{0}_.at(row)[pos] = '\\0'; /*safety*/"
        "}}",
        name);
    case rtl::sql_cat::w_string:
      return fmt::format( //
        "{{"
        "auto l = std::min(v.size(), l_{0}); "
        "if (l > std::numeric_limits<std::int32_t>::max()) throw std::out_of_range(\"Value size is "
        "too big.\");"
        "len_{0}_.at(row) = static_cast<int32_t>(l);"
        "auto pos = v.copy(&{0}_.at(row)[0], l,0);"
        "{0}_.at(row)[pos] = L'\\0'; /*safety*/"
        "}}",
        name);
    case rtl::sql_cat::b_string:
      return fmt::format( // (no final 0 on purpose)
        "{{"
        "auto l = std::min(v.size(), l_{0}); "
        "if (l > std::numeric_limits<std::int32_t>::max()) throw std::out_of_range(\"Value size is "
        "too big.\");"
        "len_{0}_.at(row) = static_cast<int32_t>(l);"
        "v.copy(&{0}_.at(row)[0], l,0);"
        "}}",
        name);
    default: __builtin_unreachable();
    }
  }

  e_json generator::internal_model_to_json(const data_statements& s)
  {
    json j;
    j["cpp-file"] = this->filename(tpl_types::cpp);
    j["hpp-file"] = this->filename(tpl_types::hpp);

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
      jstmt["id"]           = stmt.id();
      jstmt["sql"]          = prefix_text(stmt.sql(), 10); // no offset NOLINT
      jstmt["dscr"]         = stmt.dscr().empty() ? "" : prefix_text(stmt.dscr(), 10); // NOLINT
      jstmt["res-set-size"] = stmt.res_set_size();
      jstmt["par-set-size"] = stmt.par_set_size();

      jstmt["column"] = json::array();
      jstmt["param"]  = json::array();
      for (const auto& col : stmt.columns())
      {
        json tmp_col;
        tmp_col["index"]          = col.index;
        tmp_col["name"]           = col.name;
        tmp_col["type"]           = ME::enum_name(col.type);
        tmp_col["type_name"]      = get_sql_mapping(col.type)->c_mnemonic;
        tmp_col["odbc_name_type"] = col.odbc_type;
        tmp_col["size"]           = col.size;
        tmp_col["digits"]         = col.digits;
        tmp_col["nullable"]       = col.nullable;
        tmp_col["as-param"]       = get_sql_mapping(col.type)->par_type_name;
        tmp_col["as-result"]      = get_sql_mapping(col.type)->ret_type_name;
        tmp_col["storage"]        = attr_storage_type(col.type, col.name);
        tmp_col["getter-code"]    = attr_getter_code(col.type, col.name);
        tmp_col["setter-code"]    = attr_setter_code(col.type, col.name);
        jstmt["column"].push_back(tmp_col);
      }
      for (const auto& par : stmt.params())
      {
        json tmp_col;
        tmp_col["index"]          = par.index;
        tmp_col["name"]           = par.name;
        tmp_col["type"]           = ME::enum_name(par.type);
        tmp_col["type_name"]      = get_sql_mapping(par.type)->c_mnemonic;
        tmp_col["odbc_name_type"] = par.odbc_type;
        tmp_col["size"]           = par.size;
        tmp_col["digits"]         = par.digits;
        tmp_col["nullable"]       = par.nullable;
        tmp_col["as-param"]       = get_sql_mapping(par.type)->par_type_name;
        tmp_col["as-result"]      = get_sql_mapping(par.type)->ret_type_name;
        tmp_col["storage"]        = attr_storage_type(par.type, par.name);
        tmp_col["getter-code"]    = attr_getter_code(par.type, par.name);
        tmp_col["setter-code"]    = attr_setter_code(par.type, par.name);
        jstmt["param"].push_back(tmp_col);
      }

      j["statements"].push_back(jstmt);
    }
    return j;
  };


} // namespace dbgen4
