#include "common.hpp"
#include <algorithm>
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
   * @brief Obreže beli prostor na začetku in koncu niza in vrne std::string_view.
   * * KLJUČNO: Ta funkcija ne kopira podatkov; vrne pogled (view) na izvorni niz.
   * To je izjemno hitro in učinkovito.
   * * @param s Vhodni niz (lahko std::string, std::string_view ali const char*).
   * @return std::string_view Pogled na obrezani niz.
   */
  std::string_view trim_whitespace_view(std::string_view s)
  {
    // 1. Določitev predikata za preverjanje whitespace znakov
    auto is_space = [](char ch)
    {
      // Pomembno: static_cast za varnost pri isspace
      return std::isspace(static_cast<unsigned char>(ch));
    };

    // 2. Iskanje začetka (Levo obrezovanje)
    // std::find_if_not poišče prvi znak, ki NI whitespace.
    const auto* first = find_if_not(s.begin(), s.end(), is_space); // NOLINT

    // 3. Iskanje konca (Desno obrezovanje)
    // Uporabimo reverzne iteratorje za učinkovito iskanje od zadaj.
    const auto* last = find_if_not(s.rbegin(), s.rend(), is_space).base(); // NOLINT

    // 4. Izračun in vrnitev string_view

    // Preverimo, če je niz prazen ali samo whitespace.
    if (first >= last)
    {
      return ""; // Vrni prazen string_view
    }

    // Ustvarimo string_view s pravim začetkom in dolžino
    // Začetek: razdalja od začetka niza do prvega ne-whitespace znaka (first)
    size_t pos = first - s.begin();

    // Dolžina: razdalja med najdenima iteratorjema (last - first)
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

    // 1. Copying to std::stringstream:
    // This step is required to use std::getline with an in-memory string.
    std::stringstream ss{std::string(input_sv)};

    std::string segment;

    // 2. Using std::getline to read segments delimited by 'delimiter'
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
   * multiliner texts are splitted to lines and each line is prefixed with proper number of spaces
   *
   * @param text text to be offseted
   * @param offs amount of offset from the left margin
   * @return str_t offseted text
   */
  str_t offset_text(const str_t& text, size_t offs)
  {
    auto sql_view = trim_whitespace_view(text); /// trim leading and trailing whitespaces
    if (sql_view.contains("\n"))                /// multiliner
      return str_t("\n") + join(prefix_split(sql_view, '\n', str_t(offs, ' ')), "\n");
    return str_t(sql_view); /// single liner
  }
}; // namespace dbgen4