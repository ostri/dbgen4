/**
 * @file rtl.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-11-04
 *
 * @copyright Copyright (c) 2025
 *
 */
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <thread>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "rtl.hpp"

namespace rtl
{
  namespace
  {
    constexpr unsigned thread_bits   = 10;
    constexpr unsigned sequence_bits = 12;
    constexpr uint64_t sequence_mask = (uint64_t{1} << sequence_bits) - 1;
    constexpr uint16_t thread_mask   = (uint16_t{1} << thread_bits) - 1;

    /// 2025-01-01T00:00:00Z in milliseconds since the Unix epoch - shifts the
    /// 41-bit timestamp field's range forward so it does not run out until
    /// well past this project's lifetime, same reasoning real snowflake ids
    /// use their own custom epoch for
    constexpr uint64_t custom_epoch_ms = 1735689600000;

    uint64_t now_ms() noexcept
    {
      return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    /// packed (ms_since_epoch << sequence_bits) | sequence - one atomic
    /// instead of two so a single compare_exchange keeps both in step
    /// across threads racing this function at once. Deliberately mutable
    /// and file-scope: unique_id() needs state shared by every caller, in
    /// every thread, for the lifetime of the process.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::atomic<uint64_t> last_state{0}; // NOLINT(cert-err58-cpp) - trivial constant init, cannot throw
  } // namespace

  uint64_t unique_id(uint16_t thread) noexcept
  {
    const auto t = static_cast<uint64_t>(thread & thread_mask);
    for (;;)
    {
      const uint64_t wall_ms = now_ms() - custom_epoch_ms;
      uint64_t       prev    = last_state.load(std::memory_order_relaxed);
      const uint64_t prev_ms = prev >> sequence_bits;

      // std::chrono::system_clock is wall-clock time, not monotonic - an NTP
      // adjustment (or just scheduling/TSC jitter across cores) can make a
      // later call observe an EARLIER wall_ms than an already-published
      // prev_ms. Clamping ms to max(wall_ms, prev_ms) keeps last_state's own
      // timestamp field monotonically non-decreasing, so the (ms, seq) pair
      // handed out by any single compare_exchange_weak() below can never
      // collide with one already handed out by an earlier call - without
      // this clamp, the clock stepping backward and then forward again could
      // reproduce an exact (ms, seq) pair a second time (each occurrence
      // individually race-free, since compare_exchange_weak() itself never
      // spuriously succeeds - the duplicate comes from ms itself repeating,
      // not from a broken CAS). Confirmed by direct repro: unique_id_test.cpp's
      // own "never repeats" TEST_CASE reproducibly hands out a duplicate id
      // roughly 1 run in 25 without this clamp, table-stakes proof this was
      // never mere test flakiness.
      const uint64_t ms = wall_ms > prev_ms ? wall_ms : prev_ms;

      uint64_t seq = 0;
      if (ms == prev_ms)
      {
        seq = (prev & sequence_mask) + 1;
        if (seq > sequence_mask)
        {
          // this thread's 4096 ids for this millisecond are used up - wait
          // for the clock to move on rather than hand out a colliding id
          std::this_thread::sleep_for(std::chrono::microseconds(100)); // NOLINT(readability-magic-numbers)
          continue;
        }
      }

      const uint64_t next = (ms << sequence_bits) | seq;
      if (last_state.compare_exchange_weak(prev, next, std::memory_order_relaxed))
        return (ms << (thread_bits + sequence_bits)) | (t << sequence_bits) | seq;
      // another thread won the race - retry with a fresh timestamp/sequence
    }
  }

  db::~db() { }; // NOLINT

  db_sts db::connect(const std::string& /*conn_str*/) { return db_sts::driver_not_found; }

  db_sts db::connect(const std::string& host,
                     uint16_t           port,
                     const std::string& database_name,
                     const std::string& user,
                     const std::string& /*password*/)
  {
    log_().error(
      "Connection error - db2 method not implemented host: {} port {} db {} user {} pass {}", host, port, database_name, user, "*****");
    return db_sts::connection_error;
  }

  bool db::is_connected() const { return false; }

  /**
   * @brief Commits the current transaction.
   * @return db_sts Status code indicating the result of the commit operation.
   */
  db_sts db::commit() { return db_sts::success; }

  /**
   * @brief Roll back the current transaction
   * @return db_sts Status code indicating the result of the rollback operation
   *
   * This method should be overridden by derived classes to implement
   * database-specific rollback logic.
   */
  db_sts db::rollback() { return db_sts::success; }

  /**
   * @brief default implementation - a backend that cannot run a bare statement
   */
  db_sts db::exec(const std::string& sql)
  {
    log_().error("exec is not implemented by this backend. sql: '{}'", sql);
    return db_sts::not_implemented;
  }

  /**
   * @brief default implementation - a backend that never overrides this is a caller bug to
   * surface immediately, not something to silently no-op past (see refresh_statistics()'s own doc
   * comment in rtl.hpp).
   */
  db_sts db::refresh_statistics(const std::string& table_name)
  {
    log_().error("refresh_statistics is not implemented by this backend. table_name: '{}'", table_name);
    return db_sts::not_implemented;
  }

  /**
   * @brief default implementation - a backend that never overrides this is a caller bug to surface
   * immediately, not something to silently no-op past (see on_change()'s own doc comment in rtl.hpp).
   */
  db_sts db::on_change(const std::string& table_name, const change_handler& /*handler*/)
  {
    log_().error("on_change is not implemented by this backend. table_name: '{}'", table_name);
    return db_sts::not_implemented;
  }

  const db_data_root* db::data() const { return data_.get(); }

  // ------------------------------------------------------------------------
  // qry_metadata
  // ------------------------------------------------------------------------
  schema::meta_vec qry_metadata::columns() const { return columns_; }
  schema::meta_vec qry_metadata::params() const { return params_; }

  void qry_metadata::add_col_dscr(const schema::meta_dscr& dscr) { columns_.push_back(dscr); }
  void qry_metadata::add_par_dscr(const schema::meta_dscr& dscr) { params_.push_back(dscr); }

  std::string qry_metadata::dump_meta_vector(const char* fmt, const char* header, const schema::meta_vec& v) const
  {
    if (v.empty()) return {};
    std::string msg = header;
    for (const auto& col : v)
    {
      msg += fmt::format(fmt::runtime(fmt),
                         col.index,
                         col.name,
                         ME::enum_name(col.type),
                         schema::get_sql_mapping(col.type)->mnemonic,
                         col.native_type,
                         col.size,
                         col.digits,
                         col.nullable != 0 ? "yes" : "no");
    }
    return msg;
  }

  std::string qry_metadata::dump() const
  {
    constexpr const char* fmt     = "      {:>3} {:<20} {:<18} {:<20} {:>9} {:>4} {:>6} {:^8}\n";
    auto                  msg_hdr = fmt::format(fmt, "ndx", "column name", "col type", "mnemonic", "native", "size", "digits", "nullable");
    auto                  col     = dump_meta_vector(fmt, msg_hdr.c_str(), columns_);
    auto                  par     = dump_meta_vector(fmt, msg_hdr.c_str(), params_);
    return fmt::format(R"(
     columns: {}
{}
    parameters: {}
{}
  )",
                       columns_.size(),
                       col,
                       params_.size(),
                       par);
  }

} // namespace rtl
