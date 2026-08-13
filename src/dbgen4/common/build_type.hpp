#pragma once

#include <cstdint>

/**
 * @brief Enum representing build configuration type
 */
enum class build_type_enum : std::uint8_t
{
  debug,
  release
};

#ifndef NDEBUG
/// @brief Current build type (compile-time constant)
constexpr static const build_type_enum build_type = build_type_enum::debug;
#else
/// @brief Current build type (compile-time constant)
constexpr static const build_type_enum build_type = build_type_enum::release;
#endif

/**
 * @brief Check if current build is Debug
 * @return true if Debug build, false if Release
 */
consteval bool is_debug_build() { return build_type == build_type_enum::debug; }
consteval bool is_release_build() { return ! is_debug_build(); }
consteval bool is_release() { return ! is_debug_build(); }
consteval bool is_debug() { return is_debug_build(); }

/**
 * @brief Get build type as string
 * @return "debug" or "release"
 */
consteval const char* build_type_name() { return is_debug_build() ? "debug" : "release"; }
