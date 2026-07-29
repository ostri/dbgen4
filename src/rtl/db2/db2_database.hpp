// db2_database.hpp
#pragma once
/**
 * @file
 * @brief what a generated query needs from the database object
 *
 * Kept apart from db2_rtl.hpp so that query.hpp can depend on the interface
 * without dragging in the whole connection implementation, and db_db2 can
 * implement it without depending on query.hpp.
 */

#include "db2_types.hpp" // for SQLHDBC
#include "log.hpp"

namespace rtl
{
  struct database // NOLINT
  {
    database()                                                       = default;
    virtual ~database()                                              = default;
    database(const database&)                                        = delete;
    database& operator=(const database&)                             = delete;
    database(database&&)                                             = delete;
    database& operator=(database&&)                                  = delete;
    /// the live connection handle a query allocates its statements on
    [[nodiscard]] virtual SQLHDBC         get_conn() const noexcept   = 0;
    [[nodiscard]] virtual class log::log* get_logger() const noexcept = 0;
  };
} // namespace rtl
