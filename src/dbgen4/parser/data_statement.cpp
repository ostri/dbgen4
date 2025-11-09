#include "data_statement.hpp"
// #include <magic_enum.hpp>
#include "common.hpp"
#include <fmt/format.h>
namespace dbgen4
{
  data_statement::data_statement(const rtl::qry_metadata& o) { assign(o); }

  data_statement& data_statement::operator=(const rtl::qry_metadata& o)
  {
    if ((id_ != o.id()) && ! id_.empty()) log()->warn("The keys are not the same. Possible bug");
    assign(o);
    return *this;
  }

  void data_statement::assign(const rtl::qry_metadata& o)
  {
    id_      = o.id();
    sql_     = o.sql();
    desc_    = o.dscr();
    columns_ = o.columns();
    params_  = o.params();
  }

  str_t data_statement::id() const { return id_; }
  str_t data_statement::sql() const { return sql_; }

  /**
   * @brief dump meta vector
   *
   * It serializes the provided meta vector using the provided format and header. If the vector is
   * empty, it returns an empty string.

   * @param offs offset from left margin
   * @param fmt serialization fomat of the meta data line
   * @param header the header line to be at the begining
   * @param v
   * @return serialized vector of meta data
   */
  std::string data_statement::dump_meta_vector(size_t          offs,
                                               const char*     fmt,
                                               const char*     header,
                                               const meta_vec& v) const
  {
    if (! v.empty())
    {
      std::string msg = header;
      str_t       left_padding(offs, ' ');
      for (auto col : v)
      {
        msg += fmt::format(fmt::runtime(fmt),
                           left_padding,
                           col.index,
                           col.name,
                           ME::enum_name(col.type),
                           get_sql_type_mnemonic(col.type),
                           col.odbc_type,
                           col.size,
                           col.digits,
                           col.nullable != 0 ? "yes" : "no");
      }
      return msg;
    }
    return {};
  }

  std::string data_statement::dump(size_t offs) const
  {
    str_t                 left_padding(offs, ' ');
    str_t                 left_padding_2(offs + 2, ' ');
    constexpr const char* fmt = "{}{:>3} {:<20} {:<18} {:<20} {:>9} {:>4} {:>6} {:^8}\n";

    auto msg_hdr = fmt::format(fmt,
                               left_padding_2,
                               "ndx",
                               "column name",
                               "col type",
                               "cli id",
                               "ODBC type",
                               "size",
                               "digits",
                               "nullable");
    auto col     = dump_meta_vector(offs + 4, fmt, msg_hdr.c_str(), columns_);
    auto par     = dump_meta_vector(offs + 4, fmt, msg_hdr.c_str(), params_);

    /// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    str_t sql_str = offset_text(sql_, offs + 6);

    auto msg = fmt::format(R"(
  {}id: '{}'
  {}  description: {}
  {}  sql:         {}
  
  {}  columns: cnt:{}
  {}
  {}  params:  cnt:{}
  {})",
                           left_padding,    ///
                           id_,             /// unique id
                           left_padding,    /// description
                           desc_,           /// description
                           left_padding,    ///
                           sql_str,         /// sql statement
                           left_padding,    ///
                           columns_.size(), /// column count
                           col,             /// columns
                           left_padding,    ///
                           params_.size(),  /// param count
                           par              /// params
    );
    return msg;
  }

  str_t data_statement::desc() const { return desc_; }

  void data_statement::set_id(const str_t& id) { id_ = id; }

  meta_vec data_statement::columns() const { return columns_; }

  void data_statement::set_sql(const str_t& sql) { sql_ = trim_whitespace_view(sql); }

  meta_vec data_statement::params() const { return params_; }

  void data_statement::set_desc(const str_t& desc) { desc_ = desc; }

  spdlog::logger* data_statement::log() const { return log::get(); }
}; // namespace dbgen4
