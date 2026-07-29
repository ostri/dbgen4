#pragma once
/**
 * @file
 * @brief fmt formatters used by the generator itself
 *
 * Formatters for the rtl structure types live in rtl/rtl_fmt.hpp - those are
 * needed by generated code, these are not.
 */
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <yaml-cpp/node/node.h>
#include <stacktrace>

// clang-format off
template <> struct fmt::formatter<YAML::Node> : fmt::ostream_formatter {}; //NOLINT
template <> struct fmt::formatter<std::stacktrace> : fmt::ostream_formatter{}; // NOLINT
// clang-format on
