#pragma once

#include "logger.hpp"      // IWYU pragma: keep.
#include <source_location> // IWYU pragma: keep.
#include <yaml-cpp/yaml.h>
#include <expected>
#include <string>
#include <vector>
#include <map>
// #include "fmt_structs.hpp"
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
    /**
     * @brief fetch pointer to logger
     *
     * @return spdlog::logger*
     */
    static class rtl::logger* log_() { return rtl::logger::get(); };

    [[nodiscard]] std::string to_string() const;
  } __attribute__((aligned(128))); // NOLINT

  class parse_yaml;
  using Result = std::expected<parse_yaml, Error>;

  class parse_yaml
  {
  public:
    // constructors
    parse_yaml()  = default;
    ~parse_yaml() = default;
    parse_yaml(YAML::Node node, std::string name);

    // copy & move
    parse_yaml(const parse_yaml&)                = default;
    parse_yaml(parse_yaml&&) noexcept            = default;
    parse_yaml& operator=(const parse_yaml&)     = default;
    parse_yaml& operator=(parse_yaml&&) noexcept = default;

    static Result        load(const std::string& filename);
    static Result        load_from_string(const std::string& content, const std::string& name = "<string>");
    [[nodiscard]] Result get_map(const std::string& key) const;

    // getters
    template <typename T>
    [[nodiscard]] std::expected<T, Error> get(const std::string& key, [[maybe_unused]] loc_t loc = std::source_location::current()) const
    {
      try
      {
        if (! root_[key] || root_[key].IsNull()) // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        {
          const Error err{.message  = fmt::format("root: '{}' tag: '{}' not found", root_.Tag(), key),
                          .line     = root_.Mark().line,
                          .column   = root_.Mark().column,
                          .filename = filename_};
          log_()->trace(err.to_string());
          return std::unexpected(make_missing_key_error(key));
        }
        log_()->debug("tag '{}' found.", key);
        return root_[key].as<T>(); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      }
      catch (const YAML::Exception& e)
      {
        auto err = Error{.message  = fmt::format("Failed to convert key '{}' : msg: '{}'", key, e.msg),
                         .line     = e.mark.line,
                         .column   = e.mark.column,
                         .filename = filename_};
        log_()->error(err.to_string());
        return std::unexpected(err);
      }
    }

    template <typename T>
    [[nodiscard]] T get_or(const std::string& key, const T& def) const noexcept
    {
      auto res = get<T>(key);
      if (! res)
      {
        try
        {
          log_()->trace("root: '{0}' key '{1}' not found; using default. default:'{2}'", root_.Tag(), key, def);
        }
        catch (const YAML::Exception&) // NOLINT(bugprone-empty-catch)
        {
        }
        return def;
      }
      return *res;
    }

    /// node existence and type checking
    [[nodiscard]] bool exists(const std::string& key) const noexcept
    {
      try
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        return root_[key] && ! root_[key].IsNull();
      }
      catch (const YAML::Exception&)
      {
        return false;
      }
    }
    [[nodiscard]] bool is_map(const std::string& key) const noexcept
    {
      try
      {
        return exists(key) && root_[key].IsMap(); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      }
      catch (const YAML::Exception&)
      {
        return false;
      }
    }
    [[nodiscard]] bool is_sequence(const std::string& key) const noexcept
    {
      try
      {
        return exists(key) && root_[key].IsSequence(); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      }
      catch (const YAML::Exception&)
      {
        return false;
      }
    }
    [[nodiscard]] bool is_scalar(const std::string& key) const noexcept
    {
      try
      {
        return exists(key) && root_[key].IsScalar(); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      }
      catch (const YAML::Exception&)
      {
        return false;
      }
    }
    /// fetch non atomic structures
    [[nodiscard]] std::expected<std::vector<std::string>, Error> get_seq_of_strings(const std::string& key) const;
    [[nodiscard]] std::expected<std::vector<parse_yaml>, Error>  get_seq_of_maps(const std::string& key) const;

    [[nodiscard]] std::vector<parse_yaml>  get_seq_of_maps_or(const std::string&             key,
                                                              const std::vector<parse_yaml>& def = {}) const noexcept;
    [[nodiscard]] std::vector<std::string> get_seq_of_strings_or(const std::string&              key,
                                                                 const std::vector<std::string>& def = {}) const noexcept;
    /// map of column name to a number, as used by 'field-len'
    [[nodiscard]] std::map<std::string, size_t> get_map_of_sizes_or(const std::string& key) const noexcept;
  private:
    YAML::Node  root_;
    std::string filename_;
    /// private methods
    static class rtl::logger* log_() { return rtl::logger::get(); };
    /// Member variables
    Error make_missing_key_error(const std::string& key) const;
  };
} // namespace dbgen4