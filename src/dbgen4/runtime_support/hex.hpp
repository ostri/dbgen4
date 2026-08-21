// hex.hpp
#pragma once
#include <span>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstddef>

namespace dbgen4
{

  [[nodiscard]] inline std::string to_hex(const std::string_view data, const char separator = '-')
  {
    if (data.empty()) [[unlikely]] { return {}; }

    // Vsak bajt → 2 hex znaka + 1 separator (razen zadnji)
    const std::size_t result_size = (data.size() * 2) + (data.size() - 1U);

    std::string result;
    result.reserve(result_size);

    // LUT za hex pretvorbo – najhitrejša možna pot (brez brancha)
    // NOLINTBEGIN(readability-magic-numbers)
    static constexpr char // NOLINT
      hex_lut[513] = "000102030405060708090A0B0C0D0E0F"
                     "101112131415161718191A1B1C1D1E1F"
                     "202122232425262728292A2B2C2D2E2F"
                     "303132333435363738393A3B3C3D3E3F"
                     "404142434445464748494A4B4C4D4E4F"
                     "505152535455565758595A5B5C5D5E5F"
                     "606162636465666768696A6B6C6D6E6F"
                     "707172737475767778797A7B7C7D7E7F"
                     "808182838485868788898A8B8C8D8E8F"
                     "909192939495969798999A9B9C9D9E9F"
                     "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
                     "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
                     "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
                     "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
                     "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
                     "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";
    // NOLINTEND(readability-magic-numbers)

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data.data()); // NOLINT

    // Prvi bajt brez separatorja
    const std::size_t first = static_cast<std::size_t>(bytes[0]) * 2U; // NOLINT
    result.push_back(hex_lut[first]);                                  // NOLINT
    result.push_back(hex_lut[1U + first]);                             // NOLINT

    // Preostali bajti s separatorjem
    for (std::size_t i = 1; i < data.size(); ++i)
    {
      result.push_back(separator);
      const std::size_t idx = static_cast<std::size_t>(bytes[i]) * 2U; // NOLINT
      result.push_back(hex_lut[idx]);                                  // NOLINT
      result.push_back(hex_lut[1U + idx]);                             // NOLINT
    }

    return result;
  }

  // Preobremenitev za std::span<const std::uint8_t> ali std::vector<uint8_t>
  [[nodiscard]] inline std::string to_hex(const std::span<const std::uint8_t> data, const char separator = '-')
  { // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return to_hex(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()), separator);
  }

} // namespace dbgen4