// db_error.cpp
#include "db_error.hpp"
#include <array>
#include <fmt/format.h>
#include <string_view>

namespace rtl
{
  namespace
  {
    /// the first two characters of a SQLSTATE are its class, the last three
    /// the specific condition within it
    [[nodiscard]] constexpr bool has_class(std::string_view s, std::string_view cls) noexcept
    { return s.size() >= 2 && s.substr(0, 2) == cls; }
  } // namespace

  namespace
  {
    /// one entry of either table below - a code or a class, and what it means
    struct state_entry
    {
      std::string_view key;
      db_sts           sts;
    };

    /**
     * @brief the conditions specific enough to deserve their own status
     *
     * Consulted before by_class, so that 23505 comes back as duplicate_key
     * rather than as the constraint_violation its class would give.
     */
    constexpr std::array by_code = {
      state_entry{.key = "00000", .sts = db_sts::success},
      state_entry{.key = "23505", .sts = db_sts::duplicate_key},         ///< unique violation
      state_entry{.key = "40001", .sts = db_sts::serialization_failure}, ///< could not serialize
      state_entry{.key = "40P01", .sts = db_sts::deadlock},              ///< psql deadlock detected
      state_entry{.key = "40003", .sts = db_sts::transaction_error},     ///< completion unknown
      state_entry{.key = "22001", .sts = db_sts::data_truncated},        ///< string right truncation
      state_entry{.key = "53100", .sts = db_sts::disk_full},             ///< psql disk full
      state_entry{.key = "53200", .sts = db_sts::memory_error},          ///< psql out of memory
      state_entry{.key = "53400", .sts = db_sts::quota_exceeded},        ///< psql configuration limit
      state_entry{.key = "57014", .sts = db_sts::timeout},               ///< query cancelled
      state_entry{.key = "28000", .sts = db_sts::access_denied},
      state_entry{.key = "28P01", .sts = db_sts::access_denied},
    };

    /**
     * @brief what a whole SQLSTATE class means, when no code above matched
     *
     * 25P02 - the aborted transaction psql leaves behind after a failed
     * statement - lands in transaction_error through class 25, which is what
     * the async facade needs to recognise.
     */
    constexpr std::array by_class = {
      state_entry{.key = "01", .sts = db_sts::success_with_info},
      state_entry{.key = "02", .sts = db_sts::no_data},
      state_entry{.key = "08", .sts = db_sts::connection_lost},       ///< connection exception
      state_entry{.key = "0A", .sts = db_sts::not_implemented},       ///< feature not supported
      state_entry{.key = "22", .sts = db_sts::data_conversion_error}, ///< data exception
      state_entry{.key = "23", .sts = db_sts::constraint_violation},  ///< integrity constraint
      state_entry{.key = "24", .sts = db_sts::invalid_cursor},        ///< invalid cursor state
      state_entry{.key = "25", .sts = db_sts::transaction_error},     ///< invalid transaction state
      state_entry{.key = "28", .sts = db_sts::access_denied},         ///< invalid authorization
      state_entry{.key = "40", .sts = db_sts::transaction_error},     ///< transaction rollback
      state_entry{.key = "42", .sts = db_sts::invalid_sql},           ///< syntax error or access rule
      state_entry{.key = "53", .sts = db_sts::resource_error},        ///< insufficient resources
      state_entry{.key = "54", .sts = db_sts::resource_error},        ///< program limit exceeded
      state_entry{.key = "57", .sts = db_sts::admin_error},           ///< operator intervention
      state_entry{.key = "58", .sts = db_sts::os_error},              ///< system error
    };
  } // namespace

  db_sts sqlstate_to_db_sts(std::string_view sql_state) noexcept
  {
    /// A client side fault, with no server diagnostic behind it. Not success -
    /// this function is only ever asked about a failure.
    if (sql_state.empty()) return db_sts::error;

    for (const auto& e : by_code)
      if (sql_state == e.key) return e.sts;

    for (const auto& e : by_class)
      if (has_class(sql_state, e.key)) return e.sts;

    /// A real SQLSTATE this does not know about. Still a failure, just an
    /// unclassified one - the message and the state itself carry the detail.
    return db_sts::error;
  }

  std::string db_error::str() const
  {
    if (sql_state.empty()) return fmt::format("{} ({})", message, db_status_to_string(sts));
    return fmt::format("sqlstate '{}': {} ({})", sql_state, message, db_status_to_string(sts));
  }

} // namespace rtl
