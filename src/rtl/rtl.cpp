#include "rtl.hpp"
namespace rtl
{
  bool db::is_connected() const { return false; }

  const db_data_root* db::data() const { return data_.get(); }


  constexpr const char* db_status_to_string(db_sts status) noexcept
  {
    switch (status)
    {
    case db_sts::success: return "Success";
    case db_sts::success_with_info: return "Success with info";
    case db_sts::no_data: return "No data";
    case db_sts::error: return "Error";
    case db_sts::invalid_handle: return "Invalid handle";
    case db_sts::need_data: return "Need data";
    case db_sts::still_executing: return "Still executing";
    case db_sts::connection_error: return "Connection error";
    case db_sts::connection_lost: return "Connection lost";
    case db_sts::server_gone: return "Server gone";
    case db_sts::timeout: return "Timeout";
    case db_sts::busy: return "Database busy";
    case db_sts::access_denied: return "Access denied";
    case db_sts::invalid_sql: return "Invalid SQL";
    case db_sts::syntax_error: return "Syntax error";
    case db_sts::constraint_violation: return "Constraint violation";
    case db_sts::duplicate_key: return "Duplicate key";
    case db_sts::truncated: return "Data truncated";
    case db_sts::invalid_cursor: return "Invalid cursor state";
    case db_sts::transaction_error: return "Transaction error";
    case db_sts::deadlock: return "Deadlock detected";
    case db_sts::serialization_failure: return "Serialization failure";
    case db_sts::memory_error: return "Memory error";
    case db_sts::resource_error: return "Resource error";
    case db_sts::disk_full: return "Disk full";
    case db_sts::quota_exceeded: return "Quota exceeded";
    case db_sts::driver_not_found: return "Driver not found";
    case db_sts::env_error: return "Environment error";
    case db_sts::not_implemented: return "Not implemented";
    case db_sts::os_error: return "OS error";
    case db_sts::data_conversion_error: return "Data conversion error";
    case db_sts::data_truncated: return "Data truncated";
    case db_sts::invalid_parameter: return "Invalid parameter";
    case db_sts::admin_error: return "Administrative error";
    case db_sts::config_error: return "Configuration error";
    case db_sts::license_error: return "License error";
    case db_sts::custom_error: return "Custom error";
    case db_sts::unknown: return "Unknown error";
    default: return "Undefined error";
    }
  }
} // namespace rtl
