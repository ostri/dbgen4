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
