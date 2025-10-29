#include "rtl.hpp"

namespace rtl
{
  db::~db() { }; // NOLINT

  db_sts db::connect(const std::string& name) { return connect("", "", name, "", ""); }

  db_sts db::connect(const std::string& host,
                     const std::string& port,
                     const std::string& database_name,
                     const std::string& user,
                     const std::string& password)
  {
    l->error("Connection error - db2 method not implemented host: {} port {} db {} user {} pass {}",
             host,
             port,
             database_name,
             user,
             password);
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

  const db_data_root* db::data() const { return data_.get(); }


} // namespace rtl
