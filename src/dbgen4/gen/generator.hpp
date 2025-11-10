#pragma once
#include "cmd_line_params.hpp"
#include "common.hpp"
// #include "data_model.hpp"
#include "data_statements.hpp"
#include "hpp_template.hpp"
#include "inja.hpp"
#include "parser_errors.hpp"
#include <expected>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <spdlog/logger.h>
#include <sstream>
#include "log.hpp"

namespace dbgen4
{
  using json = nlohmann::ordered_json;
  class generator
  {
  public:
    generator()                            = default;
    ~generator()                           = default;
    generator(const generator&)            = default;
    generator(generator&&)                 = delete;
    generator& operator=(const generator&) = default;
    generator& operator=(generator&&)      = delete;
    /// method loads data model from parsed statements
    std::expected<json, exit_status_enum> internal_model_to_json(
      const data_statements&                  s,
      [[maybe_unused]] const cmd_line_params& cmd,
      const str_t&                            filename)
    {
      filename_   = filename;
      output_dir_ = "output/";                ///< FIXME(ostri) magic string
      cpp_file_   = output_dir_ + "gen.cpp";  ///< FIXME(ostri) magic string
      hpp_file_   = output_dir_ + "gen.hpp";  ///< FIXME(ostri) magic string
      json_file_  = output_dir_ + "gen.json"; ///< FIXME(ostri) magic string
      // gen::document doc;
      // doc = load_data_model(doc, s); /// load data model from statements

      // store data model as json
      json              j = internal_model_to_json(s, cmd); /// from cpp model to json
      std::stringstream o;                                  /// from json to string
      o << std::setw(2) << j << "\n";                       /// pretty print with 2 spaces
      log()->info("Generated JSON data model:\n{}", o.str());
      return j;
    }
    std::expected<std::string, exit_status_enum> generate_hpp_file(
      [[maybe_unused]] const std::string& hpp_file,
      const json&                         data)
    {
      //      return std::unexpected(exit_status_enum::not_implemented);
      try
      {
        inja::Environment env;
        return env.render(gen::hpp_template, data); // <-- TO JE VSE!
      }
      catch (const inja::RenderError& e)
      {
        log()->error("RENDER ERROR: {}", e.what());
        return std::unexpected(exit_status_enum::inja_render_error);
      }
      catch (const inja::ParserError& e)
      {
        log()->error("PARSER ERROR: {}", e.what());
        return std::unexpected(exit_status_enum::inja_parser_error);
      }
      catch (const inja::FileError& e)
      {
        log()->error("FILE ERROR: {}", e.what());
        return std::unexpected(exit_status_enum::inja_file_error);
      }
      catch (const inja::DataError& e)
      {
        log()->error("Data error: {}", e.what());
        return std::unexpected(exit_status_enum::inja_data_error);
      }
      // catch (const inja::inja_exception& e)
      // {
      //   log()->error("INJA EXCEPTION: {}", e.what());
      //   return std::unexpected(exit_status_enum::inja_unknown_error);
      // }
      catch (const std::exception& e)
      {
        log()->error("STD EXCEPTION: {}", e.what());
        return std::unexpected(exit_status_enum::unhandled_exception);
      }
    }
  private:
    // gen::document load_data_model(gen::document doc, const data_statements& s)
    // {
    //   doc.summary     = s.summary();
    //   doc.description = s.description();
    //   for (const auto& stmt : s.map())
    //   {
    //     gen::statement gstmt;
    //     gstmt.id   = stmt.second.id();
    //     gstmt.sql  = stmt.second.sql();
    //     gstmt.desc = stmt.second.desc();
    //     for (const auto& col : stmt.second.columns())
    //     {
    //       gen::meta_dscr md;
    //       md.index     = col.index;
    //       md.name      = col.name;
    //       md.type      = col.type;
    //       md.type_name = get_sql_type_mnemonic(col.type);
    //       md.odbc_type = col.odbc_type;
    //       md.size      = col.size;
    //       md.digits    = col.digits;
    //       md.nullable  = col.nullable != 0;
    //       gstmt.columns.push_back(md);
    //     };
    //     for (const auto& par : stmt.second.params())
    //     {
    //       gen::meta_dscr md;
    //       md.index     = par.index;
    //       md.name      = par.name;
    //       md.type      = par.type;
    //       md.type_name = get_sql_type_mnemonic(par.type);
    //       ;
    //       md.odbc_type = par.odbc_type;
    //       md.size      = par.size;
    //       md.digits    = par.digits;
    //       md.nullable  = par.nullable != 0;
    //       gstmt.params.push_back(md);
    //     }
    //     doc.statements.push_back(gstmt);
    //   };

    //   return doc;
    // }
    json internal_model_to_json([[maybe_unused]] const data_statements& s,
                                [[maybe_unused]] const cmd_line_params& cmd = {})
    {
      json j; // FIXME(ostri) implement me
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
        jstmt["sql"]    = prefix_text(stmt.sql(), 0); // no offset
        jstmt["desc"]   = stmt.desc().empty() ? "" : stmt.desc();
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
    spdlog::logger* log() { return log::get(); }; /// Member variables
    str_t           filename_;                    ///< yaml file name
    str_t           output_dir_;                  ///< output directory
    str_t           cpp_file_;                    ///< generated cpp file name
    str_t           hpp_file_;                    ///< generated hpp file name
    str_t           json_file_;                   ///< generated json file name
    // gen::document doc_;        ///< data model document
  };
}; // namespace dbgen4