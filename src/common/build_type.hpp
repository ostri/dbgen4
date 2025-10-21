#ifndef BUILD_TYPE_HPP
#define BUILD_TYPE_HPP

enum class build_type_enum : int
{
  debug,
  release
};
#ifndef NDEBUG // debug
constexpr static const build_type_enum build_type = build_type_enum::debug;
#else // release
constexpr static const build_type_enum build_type = build_type_enum::release;
#endif

/// @brief build type is debug
/// @return true if the build type is debug, otherwise false
consteval bool        is_debug_build() { return build_type == build_type_enum::debug; }
/// @brief build type as a string
/// @return returns string denoting the build type (debug or release)
consteval const char* build_type_name() { return ::is_debug_build() ? "debug" : "release"; }
#endif // BUILD_TYPE_HPP
