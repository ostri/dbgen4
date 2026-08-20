// db_error.hpp
#pragma once
/**
 * @file
 * @brief one description of a failure, whichever backend produced it
 *
 * The two runtimes each have an error type of their own - odbc_error carries
 * what SQLGetDiagRec reported, psql_error what libpq did - and both stay
 * exactly as they are. Neither can be shared: odbc_error is written in terms
 * of SQLRETURN and SQLHANDLE, so a psql build would have to include the DB2
 * CLI headers just to describe an error.
 *
 * This is what they both convert into, for callers that must not care which
 * backend is linked in. The async facade is the first of those; nothing in
 * the existing synchronous path uses it, and the generated code never sees it.
 *
 * On the two fields that do not mean the same thing on both sides:
 *
 * `driver_status` is the raw value the driver returned - SQLRETURN on db2,
 * ExecStatusType on psql - and is deliberately not translated. The two sets
 * neither line up in count (7 against 13) nor in value: SQL_ERROR is -1 while
 * PGRES_FATAL_ERROR is 7, and PGRES_TUPLES_OK is 2, which is also
 * SQL_STILL_EXECUTING. Mapping one onto the other would quietly turn a
 * successful select into "still executing". Worse, libpq has states ODBC has
 * no word for at all - PGRES_PIPELINE_ABORTED says a row never ran because an
 * earlier one in the pipeline failed, which is precisely the distinction
 * psql's batch path depends on. So the field keeps the backend's own number,
 * for the log and for a debugger, and decisions are made on `sts` instead.
 *
 * `native_error` is db2's SQLINTEGER. PostgreSQL has no numeric code beyond
 * the SQLSTATE, so it stays 0 there.
 */

#include "rtl.hpp"
#include <logger/logger.hpp>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rtl
{
  /**
   * @brief a failure, described in terms no backend owns
   */
  struct db_error
  {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    db_sts      sts = db_sts::unknown; ///< the classification decisions are made on
    std::string message;               ///< human readable, UTF-8
    std::string sql_state;             ///< five character SQLSTATE, empty for client side errors
    int16_t     driver_status = 0;     ///< raw driver return code - see the file comment
    int32_t     native_error  = 0;     ///< db2's SQLINTEGER; 0 on psql, which has none
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    [[nodiscard]] bool        is_error() const noexcept;
    [[nodiscard]] bool        is_duplicate() const noexcept;
    [[nodiscard]] std::string str() const;
  };

  /**
   * @brief classify a failure by its SQLSTATE
   *
   * SQLSTATE is the one vocabulary both backends genuinely share - it is in
   * the SQL standard, and DB2 and PostgreSQL use the same five character codes
   * for the same conditions. That makes it a sounder basis for classification
   * than either driver's return code, which is why this exists rather than a
   * translation between SQLRETURN and ExecStatusType.
   *
   * Only the classes worth acting on are mapped; anything unrecognised comes
   * back as db_sts::error, which is still an error, just an unclassified one.
   *
   * @param sql_state five character SQLSTATE, or empty for a client side fault
   * @return the matching db_sts, db_sts::error when nothing matches
   */
  [[nodiscard]] db_sts sqlstate_to_db_sts(std::string_view sql_state) noexcept;

  /**
   * @brief turn whatever the linked backend reports into a db_error
   *
   * Not declared here: the overload lives with the backend error type it
   * takes, in odbc_error.hpp and in query.hpp respectively, because only
   * there is that type known. Both are called `rtl::to_db_error`, so a
   * caller writes the same line either way and unqualified lookup finds
   * whichever one is linked in - the same arrangement as make_db(), and for
   * the same reason: no #ifdef at the call site, and rtl stays free of any
   * knowledge of odbc_error or psql_error.
   */

  /**
   * @brief log-and-collapse-to-bool for a query<>::prepare()/execute()/fetch() result
   *
   * The pattern this replaces - `if (! x) { log().error(fmt::format(...)); return false; }` -
   * repeats at every call site that runs a generated statement, differing only in which context
   * string it logs. This does the same job in one line: true straight through on success, false
   * with an error already logged (via to_db_error(result.error()).str() - found by ADL, same
   * arrangement to_db_error() itself already documents above) on failure.
   *
   * Deliberately does NOT roll back or return anything from the failed operation itself - that
   * decision (roll back once, at the caller's own single exit point, rather than after every failed
   * step) stays with the caller, since only the caller knows whether more than one statement is
   * sharing that transaction.
   *
   * @tparam E whatever error type the backend's own query<> reports (psql_error, odbc_error, ...) -
   *           unconstrained here; to_db_error(e) itself only compiles for backends actually linked in.
   * @param result  what prepare()/execute()/fetch() just returned.
   * @param log     logs to, only on failure.
   * @param context prefixed to the logged line, e.g. "on_doc_close: prepare(update docs)".
   * @return true if result held a value, false otherwise.
   */
  template <typename E>
  [[nodiscard]] bool chk(const std::expected<void, E>& result, const logger::Logger& log, std::string_view context)
  {
    if (result) return true;
    log.error("{}: {}", context, to_db_error(result.error()).str());
    return false;
  }

  /**
   * @brief chk()'s own overload for db::commit()/db::rollback() - see the std::expected<void, E>
   * overload's own doc comment for the rest.
   * @param sts     what commit()/rollback() just returned.
   */
  [[nodiscard]] inline bool chk(db_sts sts, const logger::Logger& log, std::string_view context)
  {
    if (is_success(sts)) return true;
    log.error("{}: {}", context, db_status_to_string(sts));
    return false;
  }

  inline bool db_error::is_error() const noexcept { return ! is_success(sts); }

  inline bool db_error::is_duplicate() const noexcept { return sql_state == "23505"; }

} // namespace rtl
