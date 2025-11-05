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
}; // namespace dbgen4