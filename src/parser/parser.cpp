#include "parser.hpp"

namespace
{
  // NOLINTNEXTLINE
  const json person_schema = R"(
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "title": "A person",
      "properties": {
          "name": {
              "description": "Name",
              "type": "string"
          },
          "age": {
              "description": "Age of the person",
              "type": "number",
              "minimum": 2,
              "maximum": 200
          }
      },
      "required": [
                  "name",
                  "age"
                  ],
      "type": "object"
    }
    )"_json;
} // namespace

namespace dbgen4
{
  int parser::load_grammar()
  {
    try
    {
      validator_.set_root_schema(person_schema); // insert root-schema
    }
    catch (const std::exception& e)
    {
      l->critical("Validation of schema failed, with '{}'", e.what());
      throw;
    }
    l->debug("grammar loaded.");
    return 0;
  }

  int parser::exec(const str_t& filename)
  {
    file_ = json::parse(filename);

    return exec(file_);
  }
  int parser::exec(const json& o)
  {
    try
    {
      validator_.validate(o);
      l->info("Validation succeeded");
    }
    catch (const std::exception& e)
    {
      auto msg = fmt::format("Validation failed, here is why: '{}'", e.what());
      l->error(msg);
      //      throw std::runtime_error(msg);
      return 1;
    }
    return 0;
  }

  str_t parser::filename() const { return filename_; }

  void parser::set_filename(const str_t& filename) { filename_ = filename; }
}; // namespace dbgen4