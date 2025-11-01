#include "sql_col_def.hpp"

namespace dbgen4
{
  /// getters
  uint16_t sql_col_def::position() const { return position_; }

  str_t sql_col_def::name() const { return name_; }

  rtl::sql_type sql_col_def::type() const { return type_; }

  bool sql_col_def::nullable() const { return nullable_; }

  /// setters
  void sql_col_def::setPosition(const uint16_t& position) { position_ = position; }

  void sql_col_def::set_name(const str_t& name) { name_ = name; }

  void sql_col_def::set_type(const rtl::sql_type& type) { type_ = type; }

  void sql_col_def::is_nullable(bool nullable) { nullable_ = nullable; }

} // namespace dbgen4
