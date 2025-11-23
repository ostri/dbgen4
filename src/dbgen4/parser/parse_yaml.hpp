// yaml_expected.hpp
#pragma once

#include <yaml-cpp/yaml.h>
#include <expected> // C++23
#include <string>
// #include <vector>
// #include <fstream>
#include <iostream>
// #include <iomanip>
// #include <optional>

class YamlConfig
{
public:
  struct Error
  { // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    std::string message;
    int         line   = -1;
    int         column = -1;
    std::string filename;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    [[nodiscard]] std::string to_string() const
    {
      std::ostringstream oss;
      oss << "\033[1;31mYAML error";
      if (line > 0) oss << " in " << filename << " at " << line << ":" << column;
      oss << ":\033[0m\n  " << message << "\n";
      return oss.str();
    }
  } __attribute__((
    aligned(128))); // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  using Result = std::expected<YamlConfig, Error>;

  /**
   * @brief read from yaml file
   *
   * @param filename  yaml filename (path)
   * @return Result yaml or error
   */
  static Result load(const std::string& filename)
  {
    try
    {
      return YamlConfig(YAML::LoadFile(filename), filename);
    }
    catch (const YAML::Exception& e)
    {
      return std::unexpected(Error{
        .message = e.msg, .line = e.mark.line, .column = e.mark.column, .filename = filename});
    }
  }
  /**
   * @brief parse yaml string
   *
   * @param content string with yaml contents
   * @param name anme of toplevel node
   * @return Result
   */
  static Result from_string(const std::string& content, const std::string& name = "<string>")
  {
    try
    {
      return YamlConfig(YAML::Load(content), name);
    }
    catch (const YAML::Exception& e)
    {
      return std::unexpected(
        Error{.message = e.msg, .line = e.mark.line, .column = e.mark.column, .filename = name});
    }
  }

  // === Safe getters z expected ===

  template <typename T>
  [[nodiscard]] std::expected<T, Error> get(const std::string& key) const
  {
    try
    {
      if (! root_[key]) { return std::unexpected(make_missing_key_error(key)); }
      return root_[key].as<T>();
    }
    catch (const YAML::Exception& e)
    {
      return std::unexpected(
        Error{.message  = std::string("Failed to convert key '") + key + "': " + e.msg,
              .line     = e.mark.line,
              .column   = e.mark.column,
              .filename = filename_});
    }
  }

  /**
   * @brief getter with the default value
   *
   * @tparam T
   * @param key name of the node
   * @param default_value default value of the node if one is not found
   * @return T
   */
  template <typename T>
  [[nodiscard]] T get_or(const std::string& key, const T& default_value) const noexcept
  {
    auto result = get<T>(key);
    if (result) return *result;
    if (result.error().line > 0)
    {
      std::cerr << result.error().to_string() << "\n   Using default value.\n";
    }
    else { std::cerr << "Key '" << key << "' not found, using default.\n"; }
    return default_value;
  }

  /**
   * @brief direct access no safety net
   *
   * @param key
   * @return const YAML::Node&
   */
  [[nodiscard]] YAML::Node        get_node(const std::string& key) const { return root_[key]; }
  [[nodiscard]] const YAML::Node& root() const { return root_; }
private:
  YAML::Node  root_;
  std::string filename_;

  explicit YamlConfig(YAML::Node  node, // NOLINT(performance-unnecessary-value-param)
                      std::string name)
  : root_(std::move(node)) // NOLINT(hicpp-move-const-arg, performance-move-const-arg)
  , filename_(std::move(name))
  {
  }

  Error make_missing_key_error(const std::string& key) const
  {
    return Error{.message  = "Missing required key: '" + key + "'",
                 .line     = -1,
                 .column   = -1,
                 .filename = filename_};
  }
};
// uporaba
// #include "yaml_expected.hpp"
// #include <iostream>

// int main() {
//     auto config_res = YamlConfig::load("config.yaml");

//     if (!config_res) {
//         std::cerr << config_res.error().to_string() << "\n";
//         return 1;
//     }

//     const auto& cfg = *config_res;

//     auto name = cfg.get<std::string>("name");
//     auto port = cfg.get<int>("port");
//     auto tags = cfg.get<std::vector<std::string>>("tags");

//     if (!name || !port || !tags) {
//         std::cerr << "Required field missing or invalid:\n";
//         if (!name) std::cerr << name.error().to_string() << "\n";
//         if (!port) std::cerr << port.error().to_string() << "\n";
//         if (!tags) std::cerr << tags.error().to_string() << "\n";
//         return 1;
//     }

//     std::cout << "Hello " << *name << ", listening on port " << *port << "\n";
//     std::cout << "Tags: ";
//     for (const auto& t : *tags) std::cout << t << " ";
//     std::cout << "\n";

//     // Z default vrednostmi (nikoli ne pade)
//     bool debug = cfg.get_or("debug", false);
//     int threads = cfg.get_or("threads", 4);

//     std::cout << "Debug: " << std::boolalpha << debug
//               << ", threads: " << threads << "\n";

//     return 0;
// }