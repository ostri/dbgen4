#include "data_statement.hpp"
// #include <magic_enum.hpp>
#include "common.hpp"
#include <fmt/format.h>
namespace dbgen4
{
  /// data_statement::data_statement(const rtl::qry_metadata& o) { assign(o); }

  // data_statement& data_statement::operator=(const rtl::qry_metadata& o)
  // {
  //   assign(o);
  //   return *this;
  // }

  // void data_statement::assign(const rtl::qry_metadata& o)
  // {
  //   // id_      = o.id();
  //   // sql_ = o.sql();
  //   // dscr_    = o.dscr();
  //   columns_ = o.columns();
  //   params_  = o.params();
  // }

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
                           get_sql_mapping(col.type)->c_mnemonic,
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
  {0}id: '{1}'
  {0}  description:    {2}
  {0}  parameter-size: {3}
  {0}  result-size     {4}
  {0}  sql:         {5}

  {0}  columns: cnt:{6}
  {0}
  {0}  params:  cnt:{7}
  {0})",
                           left_padding,    ///
                           id_,             /// unique id
                           dscr_,           /// description
                           par_set_size_,   /// parameter set size
                           res_set_size_,   /// result set size
                           sql_str,         /// sql statement
                           columns_.size(), /// column count
                           col,             /// columns
                           params_.size(),  /// param count
                           par              /// params
    );
    return msg;
  }

  str_t data_statement::dscr() const { return dscr_; }

  void data_statement::set_id(const str_t& id) { id_ = id; }

  size_t data_statement::par_set_size() const { return par_set_size_; }

  void data_statement::set_par_set_size(size_t par_set_size) { par_set_size_ = par_set_size; }

  meta_vec data_statement::columns() const { return columns_; }

  void data_statement::set_sql(const str_t& sql) { sql_ = trim_whitespace_view(sql); }

  meta_vec data_statement::params() const { return params_; }

  void data_statement::set_dscr(const str_t& dscr) { dscr_ = dscr; }

  size_t data_statement::res_set_size() const { return res_set_size_; }

  void data_statement::set_res_set_size(size_t res_set_size) { res_set_size_ = res_set_size; }

  spdlog::logger* data_statement::log() const { return log::get(); }
}; // namespace dbgen4
