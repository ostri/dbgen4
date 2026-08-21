#include "parse_yaml.hpp"
#include <fstream>
namespace dbgen4::gen
{
  Error parse_yaml::make_missing_key_error(const std::string& key) const
  {
    auto m = root_.Mark();
    return Error{.message = fmt::format("Missing required key: '{}'", key), .line = m.line, .column = m.column, .filename = filename_};
  }

  parse_yaml::parse_yaml(YAML::Node node, std::string name, logger::Logger& log) // NOLINT
  : root_(std::move(node))                                                       // NOLINT
  , filename_(std::move(name))
  , log_ptr_(&log)
  {
  }

  // static methods
  Result parse_yaml::load(const std::string& filename, logger::Logger& log)
  {
    try
    {
      return parse_yaml(YAML::LoadFile(filename), filename, log);
    }
    catch (const YAML::Exception& e)
    {
      return std::unexpected(Error{.message = e.msg, .line = e.mark.line, .column = e.mark.column, .filename = filename});
    }
  }

  Result parse_yaml::load_from_string(const std::string& content, logger::Logger& log, const std::string& name)
  {
    try
    {
      return parse_yaml(YAML::Load(content), name, log);
    }
    catch (const YAML::Exception& e)
    {
      return std::unexpected(Error{.message = e.msg, .line = e.mark.line, .column = e.mark.column, .filename = name});
    }
  }

  // sequence of strings
  [[nodiscard]] std::expected<std::vector<std::string>, Error> parse_yaml::get_seq_of_strings(const std::string& key) const
  {
    if (! exists(key)) return std::unexpected(make_missing_key_error(key));
    if (! root_[key].IsSequence()) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    {
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      auto err = Error{.message  = fmt::format("Key '{}' is not a sequence", key),
                       .line     = root_[key].Mark().line,
                       .column   = root_[key].Mark().column,
                       .filename = filename_};
      // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      log_().error(err.to_string());
      return std::unexpected(err);
    }
    std::vector<std::string> result;
    result.reserve(root_[key].size());  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (const auto& item : root_[key]) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    {
      if (! item.IsScalar())
      {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        auto err = Error{.message  = "Key '" + key + "' is not a sequence",
                         .line     = root_[key].Mark().line,
                         .column   = root_[key].Mark().column,
                         .filename = filename_};
        // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        log_().error(err.to_string());
        return std::unexpected(err);
      }
      result.emplace_back(item.as<std::string>());
    }
    return result;
  }

  [[nodiscard]] std::vector<std::string> parse_yaml::get_seq_of_strings_or(const std::string&              key,
                                                                           const std::vector<std::string>& def) const noexcept
  {
    try
    {
      auto res = get_seq_of_strings(key);
      if (! res)
      {
        log_().trace("root: '{0}' key: '{1}' not found. Using default.", root_.Tag(), key);
        return def;
      }
      return *res;
    }
    catch (const YAML::Exception&)
    {
      return def;
    }
  }

  // sequence of maps
  [[nodiscard]] std::expected<std::vector<parse_yaml>, Error> parse_yaml::get_seq_of_maps(const std::string& key) const
  {
    if (! exists(key)) return std::unexpected(make_missing_key_error(key));
    if (! root_[key].IsSequence()) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    {
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      auto err = Error{.message  = fmt::format("Key '{}' is not a sequence of maps", key),
                       .line     = root_[key].Mark().line,
                       .column   = root_[key].Mark().column,
                       .filename = filename_};
      // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      log_().error(err.to_string());
      return std::unexpected(err);
    }

    std::vector<parse_yaml> result;
    result.reserve(root_[key].size());                  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (std::size_t i = 0; i < root_[key].size(); ++i) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    {
      const auto& item = root_[key][i]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      if (! item.IsMap())
      {
        auto err = Error{.message  = "Item " + std::to_string(i) + " in sequence '" + key + "' is not a map",
                         .line     = item.Mark().line,
                         .column   = item.Mark().column,
                         .filename = filename_};
        log_().error(err.to_string());
        return std::unexpected(err);
      }
      result.emplace_back(item, filename_ + "." + key + "[" + std::to_string(i) + "]", log_());
    }
    return result;
  }

  [[nodiscard]] std::vector<parse_yaml> parse_yaml::get_seq_of_maps_or(const std::string&             key,
                                                                       const std::vector<parse_yaml>& def) const noexcept
  {
    try
    {
      auto res = get_seq_of_maps(key);
      if (! res)
      {
        log_().trace("Root: {0} Key: '{1}' does not exists using default sequence of maps.", root_.Tag(), key);
        return def;
      }
      return *res;
    }
    catch (const YAML::Exception&)
    {
      return def;
    }
  }

  /**
   * @brief read a mapping of name to number, e.g. the field-len override
   *
   * A missing key is not an error - the caller falls back to its default. A
   * malformed entry is skipped with a warning rather than failing the whole
   * file, so one bad width does not stop code generation.
   */
  [[nodiscard]] std::map<std::string, size_t> parse_yaml::get_map_of_sizes_or(const std::string& key) const noexcept
  {
    std::map<std::string, size_t> result;
    if (! is_map(key))
    {
      if (exists(key)) log_().error("Key '{}' is not a map of name to length - ignored.", key);
      return result;
    }
    try
    {
      for (const auto& item : root_[key]) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      {
        try
        {
          result[item.first.as<std::string>()] = item.second.as<size_t>();
        }
        catch (const YAML::Exception& e)
        {
          log_().error("Entry of '{}' is not a name to length pair: {}", key, e.msg);
        }
      }
    }
    catch (const YAML::Exception&) // NOLINT(bugprone-empty-catch)
    {
    }
    return result;
  }

  // nested maps
  [[nodiscard]] Result parse_yaml::get_map(const std::string& key) const
  {
    if (! exists(key)) return std::unexpected(make_missing_key_error(key));
    if (! root_[key].IsMap()) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    {
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      auto err = Error{.message  = fmt::format("Key '{}' is not member of map/object", key),
                       .line     = root_[key].Mark().line,
                       .column   = root_[key].Mark().column,
                       .filename = filename_};
      // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      log_().trace(err.to_string());
      return std::unexpected(err);
    }
    return parse_yaml(root_[key], filename_ + "." + key, log_()); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  }

  [[nodiscard]] std::string Error::to_string() const
  {
    if (line <= 0 || filename.empty())
    { /// parsing from string no filename or line/column data
      auto msg = std::string("\033[1;31mYAML error\033[0m");
      if (! filename.empty()) msg += fmt::format(" in {}", filename);
      msg += fmt::format(":\n  {}\n", message);
      return msg;
    }

    std::vector<std::string> lines;
    std::ifstream            file(filename);
    if (file.is_open())
    {
      std::string l;
      while (std::getline(file, l)) lines.push_back(l);
      file.close();
    }

    if (lines.empty())
    {
      auto msg = fmt::format("\033[1;31mYAML error in {0} at {1}:{2} :\033[0m\n{3}\n", filename, line, column, message);
      return msg;
    }

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
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        msg += fmt::format("\033[1;37m{0}{1} \033[1;31m>\033[0m {2}\n{3}\033[1;31m^ here\033[0m\n", pad, num, lines[i], msg_feed);
      }
      else
        msg += fmt::format(
          "\033[2m{0}{1}  \033[0m {2}\n",
          pad,
          num,
          lines[i]); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,readability-inconsistent-ifelse-braces)
    }
    msg += "\n";
    return msg;
  }
}; // namespace dbgen4::gen