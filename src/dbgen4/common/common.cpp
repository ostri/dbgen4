#include "common.hpp"
// #include "parser_errors.hpp"
#include <algorithm>
#include <fstream>
// #include <ranges>
namespace dbgen4
{
  using std::find_if_not;

  str_t join(const vec_str_t& o, const str_t& delim)
  {
    str_t s{};
    for (const str_t& el : o) s += el + delim;
    if (! s.empty()) s.resize(s.size() - delim.size());
    return s;
  };

  /**
   * @brief trim leading and trailing whitespace, it returns references (no copying)
   * * @param s string to be trimmed
   * @return view to trimmed string
   */
  std::string_view trim_whitespace_view(std::string_view s)
  {
    auto is_space = [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); };

    const auto* first = find_if_not(s.begin(), s.end(), is_space);          // NOLINT
    const auto* last  = find_if_not(s.rbegin(), s.rend(), is_space).base(); // NOLINT
    if (first >= last)
    {
      return ""; // return empty string
    }
    size_t pos = first - s.begin();
    size_t len = last - first;
    return s.substr(pos, len);
  }

  /**
   * @brief Splits the input string by a single character delimiter using std::stringstream and
   * getline, and prefixes each resulting token with the provided string.
   * * This approach is commonly used to simulate reading from a stream, but it involves copying
   * the input string into a stringstream, making it less performant than direct string manipulation
   * for in-memory splitting.
   *
   * @param input_sv The input string to be split (as std::string_view).
   * @param delimiter The single character used to split the input string (as char).
   * Note: std::getline with a stream only accepts a single character delimiter.
   * @param prefix The string to be prepended to each resulting token.
   * @return std::vector<std::string> A vector of the split and prefixed strings.
   */
  vec_str_t prefix_split(std::string_view input_sv, char delimiter, const std::string& prefix)
  {
    std::vector<std::string> result;
    std::stringstream        ss{std::string(input_sv)};
    std::string              segment;
    while (std::getline(ss, segment, delimiter))
    {
      // Process the segment: adding the prefix
      std::string prefixed_token;
      // Optimization: reserve memory upfront
      prefixed_token.reserve(prefix.size() + segment.size());

      prefixed_token += prefix;
      prefixed_token += segment;

      result.push_back(std::move(prefixed_token));
    }

    return result;
  }
  /**
   * @brief offset_text offsets multiline text with proper indentation
   *
   * single liner texts are returned as is
   * multiline texts are splitted to lines and each line is prefixed with proper number of spaces
   *
   * @param text text to be prepended
   * @param offs amount of offset from the left margin
   * @return str_t prepended text
   */
  str_t offset_text(const str_t& text, size_t offs)
  {
    auto sql_view = trim_whitespace_view(text); /// trim leading and trailing whitespaces
    if (sql_view.contains("\n"))                /// multiline
      return str_t("\n") + join(prefix_split(sql_view, '\n', str_t(offs, ' ')), "\n");
    return str_t(sql_view); /// single liner
  }
  str_t prefix_text(const str_t& text, size_t offs)
  {
    auto sql_view = trim_whitespace_view(text); /// trim leading and trailing whitespaces
    return join(prefix_split(sql_view, '\n', str_t(offs, ' ')), "\n");
  }
  e_string_ read_file(const str_t& filename)
  {
    std::ifstream file(filename, std::ios::in);
    if (! file.is_open()) return std::unexpected(fmt::format("Cant read file: '{}'.", filename));
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
  }

  e_string_ write_file(const str_t& filename, const str_t& contents)
  {
    std::ofstream file(filename.data());
    if (! file.good()) { std::unexpected(fmt::format("Cant write file:{}.", filename)); }
    file.write(contents.data(), contents.size()); // NOLINT
    return "";
  }

  std::string lowercase(std::string_view input_view)
  {
    std::string exit_string(input_view);
    /// NOLINTNEXTLINE(modernize-use-ranges, boost-use-ranges)
    std::transform(exit_string.begin(), exit_string.end(), exit_string.begin(), [](unsigned char c) { return std::tolower(c); });
    return exit_string;
  }
}; // namespace dbgen4