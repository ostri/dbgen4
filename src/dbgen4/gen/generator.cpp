#include "generator.hpp"
#include "sql_types.hpp"
#include "common.hpp"
#include "context.hpp"
#include "inja.hpp"
#include "parser_errors.hpp"
#include <expected>
#include <filesystem>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include <stdexcept>
#include <system_error>
#include <utility>
#include <fmt/chrono.h>

namespace
{
  std::string to_str(const inja::json* arg, const std::string& fallback = "")
  {
    if (arg == nullptr || arg->is_null()) return fallback;
    if (arg->is_string()) return arg->get<std::string>();
    if (arg->is_number()) return std::to_string(arg->get<int64_t>());
    if (arg->is_boolean()) return arg->get<bool>() ? "true" : "false";
    if (arg->is_array() || arg->is_object()) return arg->dump(); // JSON as string
    return arg->dump();                                          // fallback
  }

  std::string pad_impl(inja::Arguments& args, logger::Logger& log)
  {
    if (args.size() < 2) throw std::runtime_error("pad needs at least 2 args");
    // required arguments
    std::string  str = to_str(args.at(0));
    const size_t len = args.at(1)->get<size_t>();
    // optional arguments
    const std::string leading   = (args.size() > 2) ? args.at(2)->get<std::string>() : "";
    const std::string trailing  = (args.size() > 3) ? args.at(3)->get<std::string>() : leading;
    std::string       fill_char = (args.size() > 4) ? args.at(4)->get<std::string>() : " ";
    // pad char is single character, blank is default;
    char pad_char = ' ';
    if (! fill_char.empty()) pad_char = fill_char[0]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    // combine leading string and trailing
    str = leading + str + trailing;
    if (str.size() >= len) return str;
    const std::string pad(len - str.size(), pad_char);
    auto              tmp = str + pad;
    log.trace("Padded value: '{}' len: {} pad_char: '{}' pad '{}'", tmp, len, pad_char, pad);
    return tmp;
  }
  /**
   * @brief left pad the string with the given fill character or blank
   *
   */
  std::string lpad_impl(inja::Arguments& args, logger::Logger& log)
  {
    if (args.size() < 2) throw std::runtime_error("pad needs at least 2 args");
    // required arguments
    std::string  str = to_str(args.at(0));
    const size_t len = args.at(1)->get<size_t>();

    // optional arguments
    const std::string leading   = (args.size() > 2) ? args.at(2)->get<std::string>() : "";
    const std::string trailing  = (args.size() > 3) ? args.at(3)->get<std::string>() : leading;
    std::string       fill_char = (args.size() > 4) ? args.at(4)->get<std::string>() : " ";

    // pad char is single character, blank is default;
    char pad_char = '-';
    if (! fill_char.empty()) pad_char = fill_char[0]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    // combine leading string and trailing
    str = leading + str + trailing;
    if (str.size() >= len) return str;
    const std::string pad(len - str.size(), pad_char);
    auto              tmp = pad + str;
    log.trace("L Padded value: '{}' len: {} pad_char: '{}' pad '{}'", tmp, len, pad_char, pad);
    return tmp;
  }
}; // anonymous namespace
namespace dbgen4
{
  namespace fs = std::filesystem;
  /**
   * @brief generate hpp and cpp file
   *
   * @param s statements - vector of sql statements that we need to generate the c++ code
   * @return e_string or error - if successful - empty string error code otherwise
   */
  e_string generator::generate(const data_statements& s)
  {
    auto json = internal_model_to_json(s);
    if (! json) return std::unexpected(json.error());

    /// does output folder exist
    const fs::path path(cmd().out_folder());
    if (! fs::exists(path))
    {
      log_().info("Provided output folder '{}' does not exist. Starting to create.");
      std::error_code ec;
      auto            ret = fs::create_directories(path, ec);
      if (! ret)
      {
        auto msg = fmt::format("Program was not able to create folder '{}' msg: {} code: {}", path.string(), ec.message(), ec.value());
        log_().critical(msg);
        throw std::runtime_error(msg);
      }
      log_().info("Output folder '{}' successfully created.", path.string());
    };

    /// only for top level templates hpp and cpp
    for (auto tpl_id : {inja_tpl_enum::main_hpp, inja_tpl_enum::main_cpp})
    {
      // auto fn_tpl = tpl_2_fn_enum(tpl_type);
      auto fn  = ((tpl_id == inja_tpl_enum::main_hpp) ? filename(gen_fn_tpl_names::hpp) : filename(gen_fn_tpl_names::cpp));
      auto res = generate_file_through_template(json.value(), tpl_id);
      if (! res) return std::unexpected(res.error());
      auto r = write_file(fn, res.value());
      if (! r) return std::unexpected(exit_status_enum::ok);
      log_().trace("File: {} written\n '{}'", fn, res.value());
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
  /**
   * @brief Construct a new generator::generator object
   *
   * @param ctx  context
   * @param log  logger
   */
  generator::generator(const context& ctx, logger::Logger& log)
  : ctx_(ctx)
  , log_ref_(log) { };
  data_statements* generator::s() const { return s_; }


  str_t generator::yaml_fn() const { return yaml_fn_; }

  str_t generator::out_folder() const { return ctx().cmd().out_folder(); }

  db_type_enum generator::db_type() const { return cmd().db_type(); }

  str_t generator::hpp_fn() const { return filename(gen_fn_tpl_names::hpp); }


  str_t generator::cpp_fn() const { return filename(gen_fn_tpl_names::cpp); }

  str_t generator::filename(gen_fn_tpl_names tpl_type) const
  {
    auto fmt = fn_tpl_.at(tpl_type);
    return fmt::format(fmt::runtime_format_string<char>(fmt), cmd().out_folder(), barename_);
  }

  void generator::set_s(data_statements* s) { s_ = s; }

  void generator::set_yaml_fn_and_barename(cstr_t yaml_fn)
  {
    yaml_fn_ = yaml_fn;
    const fs::path path(yaml_fn_);
    barename_ = path.stem().string();
  }

  e_template generator::load_template(inja_tpl_enum tpl_id)
  {
    {
      auto template_fn = template_filename(tpl_id);
      log_().trace("preparing template '{}'", template_fn);
      auto res = read_file(template_fn);
      if (! res)
      {
        const auto* fmt = get_exit_code_str(exit_status_enum::error_reading_file);
        log_().error(fmt::runtime_format_string<char>(fmt), template_fn, res.error());
        return std::unexpected(exit_status_enum::error_reading_file);
      }

      log_().trace("template read:\n'{}'", res.value());
      inja::Template templ = env_.parse(res.value());
      log_().debug("template '{}' successfully read and evaluated.", template_fn);
      return templ;
    } // namespace dbgen4
  }
  /**
   * @brief log error and return with provided error code
   *
   * @param filename_tpl name of the template
   * @param template_str contents of the template (can be empty)
   * @param e exception thrown by inja
   * @param code exit code
   * @return exit_status_enum the same as code
   */
  exit_status_enum generator::error(const str_t& filename_tpl, const str_t& template_str, const inja::InjaError& e, exit_status_enum code)
  {
    log_().critical("Render error file: '{}' error: '{}' line: {} col: {} template: \n{}.",
                    filename_tpl,
                    e.what(),
                    e.location.line,
                    e.location.column,
                    template_str);
    return code;
  }

  /**
   * @brief load all inja templates to templates_ map
   *
   * @return e_templates (templates_, error code)
   */
  e_void generator::prepare_templates()
  {
    str_t       fn; /// must be outside due to catch
    const str_t template_str = "Prepare templates";
    try
    {
      /// prepare templates only for hpp and cpp files
      for (auto tpl_id : ME::enum_values<inja_tpl_enum>())
      {
        fn       = template_filename(tpl_id);
        auto res = load_template(tpl_id);
        if (! res) return std::unexpected(exit_status_enum::error_reading_file);
        templates_.emplace(tpl_id, res.value());
      }
    } // clang-format off
    catch (const inja::RenderError& e) {return std::unexpected(error(fn, template_str, e, exit_status_enum::inja_render_error));}
    catch (const inja::ParserError& e) {return std::unexpected(error(fn, template_str, e, exit_status_enum::inja_parser_error));}
    catch (const inja::InjaError& e)   {return std::unexpected(error(fn, template_str, e, exit_status_enum::inja_general_error));}
    catch (const std::exception& e)    {throw;}
    // clang-format on
    return {};
  }
  /**
   * @brief generate the file through the template
   *
   * @param data data to fill in into template
   * @param tpl_type type of the template to generate
   * @return e_string file contents (template+data) or error
   */
  e_string generator::generate_file_through_template(const json& data, inja_tpl_enum tpl_type)
  {
    auto fn = template_filename(tpl_type);
    try
    {
      return env_.render(templates_[tpl_type], data);
    } // clang-format off
    catch (const inja::RenderError& e){return std::unexpected(error(fn, "generate", e, exit_status_enum::inja_render_error));}
    catch (const inja::ParserError& e){return std::unexpected(error(fn, "generate", e, exit_status_enum::inja_parser_error));}
    catch (const inja::FileError& e)  {return std::unexpected(error(fn, "generate", e, exit_status_enum::inja_file_error));  }
    catch (const inja::DataError& e)  {return std::unexpected(error(fn, "generate", e, exit_status_enum::inja_data_error));  }
    catch (const std::exception& e)   {throw; } // clang-format on
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
  /**
   * @brief drop the rtl:: qualification from a type name
   *
   * Generated code lives in namespace dbx, which aliases the handful of rtl
   * types that appear in signatures (cstr, wcstr, bcstr, date, time,
   * timestamp - see main_hpp.jinja). Emitting the short name keeps the
   * generated declarations narrow enough to read; the alias makes it resolve
   * to exactly the same type. The mapping table stays the single place where
   * a sql type is tied to a C++ one.
   *
   * @param type_name fully qualified name from the sql mapping table
   * @return str_t the same name without a leading rtl::
   */
  str_t generator::unqualified(rtl::cstr_t type_name)
  {
    constexpr rtl::cstr_t prefix = "rtl::";
    if (type_name.starts_with(prefix)) type_name.remove_prefix(prefix.size());
    return str_t(type_name);
  }

  str_t generator::attr_storage_type(rtl::sql_type sql_type, const str_t& name)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    case rtl::sql_cat::atomic:
      /// Everything else stores as its own C++ type, but bool cannot: the
      /// buffer is a std::vector, and std::vector<bool> is the bit packed
      /// specialisation with no data(). The driver wants one addressable byte
      /// per row anyway - SQL_C_BIT - so uint8_t is what it should have been.
      /// The getter and setter still speak bool, so callers see no difference.
      if (dscr->cpp_type_name == "bool") return "uint8_t";
      return unqualified(dscr->cpp_type_name);
    case rtl::sql_cat::structure: return unqualified(dscr->cpp_type_name);
    case rtl::sql_cat::c_string:
    case rtl::sql_cat::w_string: return fmt::format("std::array<{0}, l_{1}+1>", unqualified(dscr->cpp_type_name), name);
    case rtl::sql_cat::b_string: return fmt::format("std::array<{0}, l_{1}>", unqualified(dscr->cpp_type_name), name);
    default: std::unreachable();
    }
  }
  /**
   * @brief generate the raw type for the buffer
   *
   * @param sql_type
   * @param len column length
   * @return str_t
   */
  str_t generator::attr_storage_raw_type(rtl::sql_type sql_type, size_t len)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    case rtl::sql_cat::atomic:
    case rtl::sql_cat::structure: return fmt::format("{}", dscr->cpp_type_name);
    case rtl::sql_cat::c_string:
    case rtl::sql_cat::w_string: return fmt::format("{0}[{1:3}+1]", dscr->cpp_type_name, len);
    case rtl::sql_cat::b_string: return fmt::format("{0}[{1:3}]", dscr->cpp_type_name, len);
    default: __builtin_unreachable();
    }
  }
  /**
   * @brief fetch the base type of the attribute as string
   *
   * THe value is determined by the category database column type
   * atomic: c type
   * structure: c type
   * string: char
   * wstring: wchar_t
   * bstring: uint8_t
   *
   * @param sql_type
   * @return str_t
   */
  str_t generator::attr_base_type(rtl::sql_type sql_type)
  {
    const auto* dscr = rtl::get_sql_mapping(sql_type);
    return std::string(dscr->cpp_type_name);
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
      /// bool is stored as uint8_t (see attr_storage_type) - an explicit cast
      /// back to bool here, rather than the implicit narrowing conversion the
      /// bare return would otherwise do, is what readability-implicit-bool-conversion
      /// wants to see spelled out.
      if (dscr->cpp_type_name == "bool") return fmt::format("{{ return static_cast<bool>({0}_.at(row));}}", name);
      [[fallthrough]];
    case rtl::sql_cat::structure: return fmt::format("{{ return {0}_.at(row);}}", name);
    case rtl::sql_cat::c_string:
    case rtl::sql_cat::w_string:
    case rtl::sql_cat::b_string:
      /// the view has to span the value, not the whole array: the driver
      /// reports how much it actually wrote in the length indicator, and a
      /// view of l_<name> would carry whatever trailing bytes happen to be there
      return fmt::format("{{ return {{{0}_.at(row).data(), {0}_length(row) }};}}", name);
    default: __builtin_unreachable();
    }
  }
  /**
   * @brief
   *
   * @param sql_type
   * @param name
   * @return str_t
   */
  str_t generator::attr_setter_code(rtl::sql_type sql_type, const str_t& name)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    /// The indicator is written along with the value, exactly as the string
    /// categories do through rtl::set_value. Without it a buffer that has been
    /// through reset_all_null() keeps the indicator at rtl::null_data, and the
    /// driver sends NULL no matter what the value slot holds - a setter whose
    /// meaning depends on the column's storage category, which is not
    /// something a caller can see.
    ///
    /// sizeof over the element rather than a literal or a spelled out type:
    /// all three compile to identical machine code (checked), and this one
    /// cannot drift if the storage type changes - bool, for one, is stored as
    /// uint8_t.
    case rtl::sql_cat::atomic:
      /// same cast back the other way as attr_getter_code(): v is bool, the
      /// slot is uint8_t, and an explicit cast is what
      /// readability-implicit-bool-conversion wants in place of the narrowing
      /// conversion a bare assignment would do.
      if (dscr->cpp_type_name == "bool")
        return fmt::format("{{ {0}_.at(row) = static_cast<uint8_t>(v); len_{0}_.at(row) = static_cast<int32_t>(sizeof({0}_.at(0)));}}",
                           name);
      [[fallthrough]];
    case rtl::sql_cat::structure:
      return fmt::format("{{ {0}_.at(row) = v; len_{0}_.at(row) = static_cast<int32_t>(sizeof({0}_.at(0)));}}", name);
    case rtl::sql_cat::c_string:
      return fmt::format( //
        "{{"
        "rtl::set_value<{0},l_{1}+1>(v, row, len_{1}_, {1}_);" // net capacity +1
        "}}",
        "char",
        name);
    case rtl::sql_cat::w_string:
      return fmt::format( //
        "{{"
        "rtl::set_value<{0},l_{1}+1>(v, row, len_{1}_, {1}_);" // net capacity +1
        "}}",
        "wchar_t",
        name);
    case rtl::sql_cat::b_string:
      return fmt::format( //
        "{{"
        "rtl::set_value<{0},l_{1}>(v, row, len_{1}_, {1}_);" // net capacity +0 (to simplify, not needed)
        "}}",
        "uint8_t",
        name);
    default: __builtin_unreachable();
    }
  }
  /**
   * @brief fragment of attribute for dump method
   *
   * @param sql_type
   * @param name
   * @return str_t
   */
  str_t generator::attr_dump_value_to_string(rtl::sql_type sql_type, const str_t& name)
  {
    const auto* dscr    = rtl::get_sql_mapping(sql_type);
    auto        col_cat = dscr->category;
    switch (col_cat)
    {
    case rtl::sql_cat::structure: return fmt::format("fmt::format(\"{{}}\", {0}(n))", name);
    case rtl::sql_cat::atomic: // return fmt::format("{0}(n))", name);
    case rtl::sql_cat::c_string: return fmt::format("{0}(n)", name);
    case rtl::sql_cat::w_string: return fmt::format("dbgen4::to_utf8({0}(n))", name);
    case rtl::sql_cat::b_string: return fmt::format("dbgen4::to_hex({0}(n))", name);
    default: __builtin_unreachable();
    }
  }

  /**
   * @brief register callback that can be called from inja template
   *
   * @return e_void
   */
  e_void generator::register_callbacks()
  {
    env_.set_expression("<<", ">>");
    log_().info("Expression delimiter set to << and >>.");
    log_().debug("register callbacks");
    /**
     * @brief hpp part of buffer definition
     *
     */
    {
      const auto* cb_name = "buffer-definition";

      env_.add_callback(cb_name,
                        3,
                        [this](inja::Arguments& args) -> std::string
                        {
                          // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                          const json&       buf        = *args[0];                    // data
                          const std::string class_name = args[1]->get<std::string>(); // name
                          auto              buf_size   = args[2]->get<int>();         // buffer size

                          /// a parameter buffer and a result buffer derive from
                          /// different roots and carry different members
                          const bool is_param = (class_name == "p");

                          json data          = j_data_; //  main json
                          data["buf"]        = buf;
                          data["class-name"] = class_name;
                          data["buf-size"]   = buf_size;
                          data["is-param"]   = is_param;
                          data["root-class"] = is_param ? "rtl::parameter_root" : "rtl::result_root";
                          // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

                          // only rendering of the preloaded template
                          //
                          // The rendered template ends in a newline, and the
                          // call site is itself on its own line - together that
                          // leaves a blank line after every buffer class. Trim
                          // the trailing whitespace so the generated file has
                          // none.
                          auto out = env_.render(templates_.at(inja_tpl_enum::buf_hpp), data);
                          return str_t(trim_trailing_whitespace_view(out));
                        });
      log_().debug("callback {} - registered.", cb_name);
    }

    /**
     * @brief cpp part of the buffer definition
     *
     */
    {
      const auto* cb_name = "buffer-implementation";
      env_.add_callback(cb_name,
                        2,
                        [this](inja::Arguments& args) -> std::string
                        {
                          // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                          const json& buf        = *args[0];                    // data
                          auto        class_name = args[1]->get<std::string>(); // name

                          const bool is_param = (class_name == "p");

                          json data          = j_data_; // main json
                          data["buf"]        = buf;
                          data["class-name"] = class_name;
                          data["is-param"]   = is_param;
                          data["root-class"] = is_param ? "rtl::parameter_root" : "rtl::result_root";
                          // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

                          // only rendering of the preloaded template - trailing
                          // whitespace trimmed for the same reason as in the
                          // hpp callback above
                          auto out = env_.render(templates_.at(inja_tpl_enum::buf_cpp), data);
                          return str_t(trim_trailing_whitespace_view(out));
                        });
      log_().debug("callback {} - registered.", cb_name);
    }
    {
      env_.add_callback("pad", [this](inja::Arguments& args) -> std::string { return pad_impl(args, log_()); });
      env_.add_callback("lpad", [this](inja::Arguments& args) -> std::string { return lpad_impl(args, log_()); });
    }
    return {};
  };
  /**
   * @brief fetch the template filename for the given template type
   *
   * @param tpl_id
   * @return str_t
   */
  str_t generator::template_filename(inja_tpl_enum tpl_id) const
  {
    auto frag = ME::enum_name(tpl_id);
    switch (tpl_id)
    {
    case inja_tpl_enum::main_hpp:
    case inja_tpl_enum::main_cpp:
    case inja_tpl_enum::buf_hpp:
    case inja_tpl_enum::buf_cpp: return fmt::format("template/{}.jinja", frag);
    default: __builtin_unreachable();
    }
  }
  /**
   * @brief (part of json) result/param attr data
   * @param el element meta description
   */
  json generator::attr_mappings(rtl::meta_dscr const& el)
  {
    const auto* dscr = rtl::get_sql_mapping(el.type);
    json        tmp_col;
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    tmp_col["index"]     = el.index;
    tmp_col["name"]      = el.name;
    tmp_col["col-name"]  = fmt::format("\"{}\"", el.name);
    tmp_col["base-type"] = dscr->cpp_type_name;
    /// enumerator name of the neutral type - the template emits it as
    /// rtl::sql_type::<<type>> and each backend translates it when binding
    tmp_col["type"]        = ME::enum_name(el.type);
    tmp_col["mnemonic"]    = dscr->mnemonic;
    tmp_col["size"]        = el.size;
    tmp_col["digits"]      = el.digits;
    tmp_col["nullable"]    = el.nullable;
    tmp_col["category"]    = ME::enum_name(dscr->category);
    tmp_col["as-param"]    = unqualified(dscr->par_type_name);
    tmp_col["as-result"]   = unqualified(dscr->ret_type_name);
    tmp_col["storage"]     = attr_storage_type(el.type, el.name);
    tmp_col["storage-raw"] = attr_storage_raw_type(el.type, el.size);
    tmp_col["getter-code"] = attr_getter_code(el.type, el.name);
    tmp_col["setter-code"] = attr_setter_code(el.type, el.name);
    tmp_col["dump-value"]  = attr_dump_value_to_string(el.type, el.name);
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return tmp_col;
  }

  /**
   * @brief prepare json structure from internal data.
   *
   * @param s internal structure
   * @return e_json json structure to be used by inja template
   */
  e_json generator::internal_model_to_json(const data_statements& s)
  {
    json j;
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    j["cpp-file"] = this->filename(gen_fn_tpl_names::cpp);
    j["hpp-file"] = this->filename(gen_fn_tpl_names::hpp);
    const fs::path path(this->filename(gen_fn_tpl_names::hpp));
    auto           hpp_include = path.filename().string();
    j["hpp-include-file"]      = hpp_include;

    j["summary"]     = s.summary();
    j["description"] = join(prefix_split(trim_whitespace_view(s.description()), '\n', " *   "), "\n");
    j["version"]     = "0.1.0"; // FIXME(ostri) magic string
    j["schema"]      = barename_;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    j["timestamp"] = fmt::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));

    /// Which helper headers the emitted code will actually reach for. Only
    /// dump() needs any of them, and which one depends on the storage
    /// categories this yaml happens to use - see attr_dump_value_to_string().
    /// Worked out here so that the template does not have to walk every
    /// statement to find out, and so that a buffer of plain integers does not
    /// drag in a utf8 converter it never calls.
    bool needs_rtl_fmt = false;
    bool needs_utf8    = false;
    bool needs_hex     = false;

    const auto note_helpers = [&](const rtl::meta_dscr& el) noexcept
    {
      switch (rtl::get_sql_mapping(el.type)->category)
      {
      case rtl::sql_cat::structure: needs_rtl_fmt = true; break; // fmt::format("{}", date)
      case rtl::sql_cat::w_string: needs_utf8 = true; break;     // dbgen4::to_utf8
      case rtl::sql_cat::b_string:
        needs_hex = true;
        break; // dbgen4::to_hex
      /// printed as they are, so no helper header - and a category nobody
      /// added yet needs none either, which is why they share the branch
      case rtl::sql_cat::atomic:
      case rtl::sql_cat::c_string:
      default: break;
      }
    };

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    j["statements"] = json::array();
    for (const auto& stmt : s.map_statements() | std::views::values)
    {
      json s;
      s["id"] = stmt.id();
      /// Both of these land inside a /** */ block, so every line needs the
      /// leading star of a doxygen comment - a bare indent leaves the
      /// continuation lines dangling outside the block's left edge.
      s["sql"]     = join(prefix_split(trim_whitespace_view(stmt.sql()), '\n', "     *   "), "\n");
      s["summary"] = stmt.summary();
      s["dscr"]    = stmt.dscr().empty() ? "" : join(prefix_split(trim_whitespace_view(stmt.dscr()), '\n', "   * "), "\n");
      /// the statement verbatim, for the generated string_view - the pretty
      /// printed one above is for the doc comment and cannot be executed
      s["sql-literal"]  = str_t(trim_whitespace_view(stmt.sql()));
      s["res-set-size"] = stmt.res_buf_size();
      s["par-set-size"] = stmt.par_buf_size();

      s["result"] = json::array();
      s["param"]  = json::array();

      for (const auto& el : stmt.results())
      {
        note_helpers(el);
        s["result"].push_back(attr_mappings(el));
      }
      for (const auto& el : stmt.params())
      {
        note_helpers(el);
        s["param"].push_back(attr_mappings(el));
      }
      j["statements"].push_back(s);
    }
    j["needs-rtl-fmt"] = needs_rtl_fmt;
    j["needs-utf8"]    = needs_utf8;
    j["needs-hex"]     = needs_hex;
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return j;
  };
} // namespace dbgen4
