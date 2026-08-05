// psql_database.hpp
#pragma once
/**
 * @file
 * @brief what a generated query needs from the database object
 *
 * Kept apart from psql_rtl.hpp so that query.hpp can depend on the interface
 * without dragging in the whole connection implementation, and db_psql can
 * implement it without depending on query.hpp. Mirrors db2_database.hpp.
 */

#include <logger/logger.hpp>
#include <libpq-fe.h> // for PGconn

namespace rtl
{
  struct database // NOLINT
  {
    database()                                    = default;
    virtual ~database()                           = default;
    database(const database&)                     = delete;
    database& operator=(const database&)          = delete;
    database(database&&)                          = delete;
    database& operator=(database&&)               = delete;
    /// the live connection handle a query allocates its statements on
    [[nodiscard]] virtual PGconn* get_conn() const noexcept   = 0;
    /// not owner - borrowed pointer to the shared Logger, never deleted here
    [[nodiscard]] virtual logger::Logger* get_logger() const noexcept = 0;
  };
} // namespace rtl
