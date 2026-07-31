// query.cpp
/**
 * @file
 * @brief translation unit for the db2 query runtime
 *
 * rtl::query is a template, so its definitions live in query.hpp and there is
 * nothing here to compile yet. The file stays in the build on purpose: it is
 * where anything that does not need to be a template belongs once the runtime
 * grows one, and having it already carried by CMake means adding that code is
 * an edit rather than a build change.
 *
 * odbc_error used to live here - it moved to odbc_error.cpp when it was split
 * out of query.hpp.
 */
#include "query.hpp"

namespace rtl
{
} // namespace rtl
