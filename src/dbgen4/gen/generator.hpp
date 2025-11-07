#pragma once
#include "cmd_line_params.hpp"
#include "common.hpp"
#include "data_model.hpp"
#include "data_statements.hpp"
#include "parser_errors.hpp"
#include <expected>
#include <nlohmann/json.hpp>
#include <spdlog/logger.h>
#include <sstream>
namespace dbgen4
{
  using json = nlohmann::json;
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
      gen::document doc;
      doc = load_data_model(doc, s); /// load data model from statements

      // store data model as json
      json              j = cpp_to_json(doc); /// from cpp model to json
      std::stringstream o;                    /// from json to string
      o << std::setw(2) << j << "\n";         /// pretty print with 2 spaces
      log()->info("Generated JSON data model:\n{}", o.str());
      return j;
    }
  private:
    gen::document load_data_model(gen::document doc, const data_statements& s)
    {
      for (const auto& stmt : s.map())
      {
        gen::statement gstmt;
        gstmt.id   = stmt.second.id();
        gstmt.sql  = stmt.second.sql();
        gstmt.desc = stmt.second.desc();
        doc.statements.push_back(gstmt);
      }
      gen::statement stmt1;
      stmt1.id   = "stmt_1";
      stmt1.sql  = "SELECT * FROM SYSIBM.SYSDUMMY1";
      stmt1.desc = "Sample SQL statement 1";
      doc.statements.push_back(stmt1);

      gen::statement stmt2;
      stmt2.id   = "stmt_2";
      stmt2.sql  = "SELECT CURRENT DATE FROM SYSIBM.SYSDUMMY1";
      stmt2.desc = "Sample SQL statement 2";
      doc.statements.push_back(stmt2);

      gen::statement stmt3;
      stmt2.id   = "stmt_3";
      stmt2.sql  = "SELECT CURRENT DATE FROM SYSIBM.SYSDUMMY1";
      stmt2.desc = "Sample SQL statement 3";

      doc.statements.push_back(stmt2);
      doc.summary     = s.summary();
      doc.description = s.description();
      return doc;
    }
    nlohmann::json cpp_to_json(const gen::document& doc)
    {
      nlohmann::json j; // FIXME(ostri) implement me
      j["summary"]     = doc.summary;
      j["description"] = doc.description;
      j["statements"]  = nlohmann::json::array();
      for (const auto& stmt : doc.statements)
      {
        nlohmann::json jstmt;
        jstmt["id"]   = stmt.id;
        jstmt["sql"]  = stmt.sql;
        jstmt["desc"] = stmt.desc;
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