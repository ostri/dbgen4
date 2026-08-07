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
#include <cstdint>
#include <fmt/format.h>
#include "magic_enum_config.hpp" // IWYU pragma: keep.
#include <magic_enum.hpp>
namespace ME = magic_enum; // NOLINT(misc-unused-alias-decls)
#include "rtl.hpp"

namespace rtl
{
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
   * @brief default implementation - a backend that cannot describe statements
   *
   * The generator cannot do anything useful without metadata, so this is an
   * error rather than an empty result.
   */
  e_qry_metadata db::get_sql_metadata(const std::string& sql)
  {
    log_().error("get_sql_metadata is not implemented by this backend. sql: '{}'", sql);
    return std::unexpected(db_sts::not_implemented);
  }

  /**
   * @brief default implementation - a backend that cannot run a bare statement
   */
  db_sts db::exec(const std::string& sql)
  {
    log_().error("exec is not implemented by this backend. sql: '{}'", sql);
    return db_sts::not_implemented;
  }

  const db_data_root* db::data() const { return data_.get(); }

  // ------------------------------------------------------------------------
  // qry_metadata
  // ------------------------------------------------------------------------
  meta_vec qry_metadata::columns() const { return columns_; }
  meta_vec qry_metadata::params() const { return params_; }

  void qry_metadata::add_col_dscr(const meta_dscr& dscr) { columns_.push_back(dscr); }
  void qry_metadata::add_par_dscr(const meta_dscr& dscr) { params_.push_back(dscr); }

  std::string qry_metadata::dump_meta_vector(const char* fmt, const char* header, const meta_vec& v) const
  {
    if (v.empty()) return {};
    std::string msg = header;
    for (const auto& col : v)
    {
      msg += fmt::format(fmt::runtime(fmt),
                         col.index,
                         col.name,
                         ME::enum_name(col.type),
                         get_sql_mapping(col.type)->mnemonic,
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
