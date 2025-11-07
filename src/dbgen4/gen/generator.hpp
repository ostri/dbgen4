#pragma once
#include "common.hpp"
#include "data_model.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/logger.h>
#include <sstream>
namespace dbgen4
{
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
    void load_data_model(const str_t& filename)
    {
      filename_   = filename;
      output_dir_ = "output/";                ///< FIXME(ostri) magic string
      cpp_file_   = output_dir_ + "gen.cpp";  ///< FIXME(ostri) magic string
      hpp_file_   = output_dir_ + "gen.hpp";  ///< FIXME(ostri) magic string
      json_file_  = output_dir_ + "gen.json"; ///< FIXME(ostri) magic string
      gen::document doc;
      doc = load_data_model(doc);
      // log()->info("Loaded data model document: {}", doc.name);
      // for (const auto& stmt : doc.statements)
      // {
      //   log()->info("Statement ID: {}, SQL: {}, Description: {}", stmt.id, stmt.sql, stmt.desc);
      // }
      // store data model as json
      nlohmann::json j = cpp_to_json(doc);
      // std::ofstream  o(json_file_);
      std::stringstream o;
      o << std::setw(4) << j << "\n";
      log()->info("Generated JSON data model:\n{}", o.str());
    }
  private:
    gen::document load_data_model(gen::document doc)
    {
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
      doc.name = "krneki";
      return doc;
    }
    nlohmann::json cpp_to_json(const gen::document& doc)
    {
      nlohmann::json j; // FIXME(ostri) implement me
      j["document_name"] = doc.name;
      j["statements"]    = nlohmann::json::array();
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