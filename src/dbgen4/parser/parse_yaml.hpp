#pragma once

#include "log.hpp" // TODO log to singelton
#include <source_location>
#include <spdlog/logger.h>
#include <yaml-cpp/yaml.h>
#include <expected>
#include <string>
#include <vector>
// #include <iostream>
// #include <sstream>
#include <fstream>
#include "fmt_structs.hpp"
namespace dbgen4
{
  using loc_t = const std::source_location;
  struct Error
  { // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    std::string message;
    int         line   = -1;
    int         column = -1;
    std::string filename;
    // NOLINTEND(misc-non-private-member-variables-in-classes)
    [[nodiscard]] std::string to_string() const
    {
      if (line <= 0 || filename.empty())
      {
        auto msg = std::string("\033[1;31mYAML error\033[0m");
        if (! filename.empty()) msg += fmt::format(" in {}", filename);
        msg += fmt::format(":\n  {}\n", message);
        return fmt::format("{}", msg);
      }

      std::vector<std::string> lines;
      std::ifstream            file(filename);
      if (file.is_open())
      {
        std::string l;
        while (std::getline(file, l)) lines.push_back(l);
        file.close();
      }

      if (lines.empty()) return fmt::format("\033[1;31mYAML error in {0} at {1}:{2} :\033[0m\n{3}\n", filename, line, column, message);

      const int  cur_idx = line - 1;
      const int  start   = std::max(0, cur_idx - 3);
      const int  end     = std::min(static_cast<int>(lines.size()), cur_idx + 4);
      const auto width   = std::to_string(end).size();

      auto msg = fmt::format("\033[1;31mYAML error in {0} at {1}:{2}:\033[0m\n {3}\n\n", filename, line, column, message);

      for (int i = start; i < end; ++i)
      {
        const std::string num = std::to_string(i + 1);
        const std::string pad(width - num.size(), ' ');

        if (i == cur_idx)
        {
          std::string msg_feed(column > 0 ? column - 1 : 0, ' ');
          msg += fmt::format("\033[1;37m{0}{1} \033[1;31m>\033[0m {2}\n{3}\033[1;31m^ here\033[0m\n", pad, num, lines[i], msg_feed);
        }
        else msg += fmt::format("\033[2m{0}{1}  \033[0m {2}\n", pad, num, lines[i]);
      }
      msg += "\n";
      return msg;
    }
  } __attribute__((aligned(128))); // NOLINT

  class parse_yaml
  {
  public:
    using Result = std::expected<parse_yaml, Error>;

    // constructors
    parse_yaml()  = default;
    ~parse_yaml() = default;
    parse_yaml(YAML::Node node, std::string name) // NOLINT
    : root_(std::move(node))                      // NOLINT
    , filename_(std::move(name))
    {
    }

    // copy & move
    parse_yaml(const parse_yaml&)                = default;
    parse_yaml(parse_yaml&&) noexcept            = default;
    parse_yaml& operator=(const parse_yaml&)     = default;
    parse_yaml& operator=(parse_yaml&&) noexcept = default;

    // static methods
    static Result load(const std::string& filename)
    {
      try
      {
        return parse_yaml(YAML::LoadFile(filename), filename);
      }
      catch (const YAML::Exception& e)
      {
        return std::unexpected(Error{.message = e.msg, .line = e.mark.line, .column = e.mark.column, .filename = filename});
      }
    }

    static Result load_from_string(const std::string& content, const std::string& name = "<string>")
    {
      try
      {
        return parse_yaml(YAML::Load(content), name);
      }
      catch (const YAML::Exception& e)
      {
        return std::unexpected(Error{.message = e.msg, .line = e.mark.line, .column = e.mark.column, .filename = name});
      }
    }

    // getters
    template <typename T>
    [[nodiscard]] std::expected<T, Error> get(const std::string& key, [[maybe_unused]] loc_t loc = std::source_location::current()) const
    {
      try
      {
        if (! root_[key] || root_[key].IsNull())
        {
          // auto  msg = fmt::format("root: '{}' tag: '{}' not found", root_.Tag(), key);
          // Error err{.message = msg, .line = -1, .column = -1, .filename = filename_};
          // log()->error("YAML error '{}'", err.to_string());
          return std::unexpected(make_missing_key_error(key));
        }
        log()->debug("tag '{}' found.", key);
        return root_[key].as<T>();
      }
      catch (const YAML::Exception& e)
      {
        return std::unexpected(Error{
          .message = "Failed to convert key '" + key + "': " + e.msg, .line = e.mark.line, .column = e.mark.column, .filename = filename_});
      }
    }

    template <typename T>
    [[nodiscard]] T get_or(const std::string& key, const T& def) const noexcept
    {
      auto res = get<T>(key);
      if (! res)
      {
        log()->trace("root: '{0}' key '{1}' not found; using default. default:'{2}'", root_.Tag(), key, def);
        // if (res.error().line > 0) log()->trace("{}", res.error().to_string());
        return def;
      }
      return *res;
    }

    /// node existence and type checking
    [[nodiscard]] bool exists(const std::string& key) const noexcept { return root_[key] && ! root_[key].IsNull(); }
    [[nodiscard]] bool is_map(const std::string& key) const noexcept { return exists(key) && root_[key].IsMap(); }
    [[nodiscard]] bool is_sequence(const std::string& key) const noexcept { return exists(key) && root_[key].IsSequence(); }
    [[nodiscard]] bool is_scalar(const std::string& key) const noexcept { return exists(key) && root_[key].IsScalar(); }

    // sequence of strings
    [[nodiscard]] std::expected<std::vector<std::string>, Error> get_sequence_of_strings(const std::string& key) const
    {
      if (! exists(key)) return std::unexpected(make_missing_key_error(key));
      if (! root_[key].IsSequence())
      {
        return std::unexpected(Error{.message  = "Key '" + key + "' is not a sequence",
                                     .line     = root_[key].Mark().line,
                                     .column   = root_[key].Mark().column,
                                     .filename = filename_});
      }
      std::vector<std::string> result;
      result.reserve(root_[key].size());
      for (const auto& item : root_[key])
      {
        if (! item.IsScalar())
        {
          return std::unexpected(Error{.message  = "Item in sequence '" + key + "' is not a string",
                                       .line     = item.Mark().line,
                                       .column   = item.Mark().column,
                                       .filename = filename_});
        }
        result.emplace_back(item.as<std::string>());
      }
      return result;
    }

    [[nodiscard]] std::vector<std::string> get_sequence_of_strings_or(const std::string&              key,
                                                                      const std::vector<std::string>& def = {}) const noexcept
    {
      auto res = get_sequence_of_strings(key);
      if (! res)
      {
        log()->trace("root: '{0}' key: '{1}' not found. Using default.", root_.Tag(), key);
        // if (res.error().line > 0) log()->debug("{}", res.error().to_string());
        return def;
      }
      return *res;
    }

    // sequence of maps
    [[nodiscard]] std::expected<std::vector<parse_yaml>, Error> get_sequence_of_maps(const std::string& key) const
    {
      if (! exists(key)) return std::unexpected(make_missing_key_error(key));
      if (! root_[key].IsSequence())
      {
        return std::unexpected(Error{.message  = "Key '" + key + "' is not a sequence of maps",
                                     .line     = root_[key].Mark().line,
                                     .column   = root_[key].Mark().column,
                                     .filename = filename_});
      }

      std::vector<parse_yaml> result;
      result.reserve(root_[key].size());
      for (std::size_t i = 0; i < root_[key].size(); ++i)
      {
        const auto& item = root_[key][i];
        if (! item.IsMap())
        {
          return std::unexpected(Error{.message  = "Item " + std::to_string(i) + " in sequence '" + key + "' is not a map",
                                       .line     = item.Mark().line,
                                       .column   = item.Mark().column,
                                       .filename = filename_});
        }
        result.emplace_back(item, filename_ + "." + key + "[" + std::to_string(i) + "]");
      }
      return result;
    }

    [[nodiscard]] std::vector<parse_yaml> get_sequence_of_maps_or(const std::string&             key,
                                                                  const std::vector<parse_yaml>& def = {}) const noexcept
    {
      auto res = get_sequence_of_maps(key);
      if (! res)
      {
        log()->trace("Root: {0} Key: '{1}' does not exists using default sequence of maps.", root_.Tag(), key);
        // if (res.error().line > 0) log()->debug("{}", res.error().to_string());
        return def;
      }
      return *res;
    }

    // nested maps
    [[nodiscard]] Result get_map(const std::string& key) const
    {
      if (! exists(key)) return std::unexpected(make_missing_key_error(key));
      if (! root_[key].IsMap())
      {
        return std::unexpected(Error{.message  = "Key '" + key + "' is not a map/object",
                                     .line     = root_[key].Mark().line,
                                     .column   = root_[key].Mark().column,
                                     .filename = filename_});
      }
      return parse_yaml(root_[key], filename_ + "." + key);
    }


    // [[nodiscard]] const YAML::Node& root() const noexcept { return root_; }
  private:
    YAML::Node      root_;
    std::string     filename_;
    spdlog::logger* log() const { return log::get(); }
    Error           make_missing_key_error(const std::string& key) const
    {
      return Error{.message = "Missing required key: '" + key + "'", .filename = filename_};
    }
  };
} // namespace dbgen4