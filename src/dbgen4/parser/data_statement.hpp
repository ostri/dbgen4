#pragma once

#include "common.hpp"
#include "rtl.hpp"
#include <map>
// #include <map>

namespace dbgen4
{
  // /**
  //  * @brief array of sql's where each db type flavour has its own possible empty version
  //  *
  //  * sql is for generic sql statement. Others are for RDBMs specifics
  //  */
  // using map_db_type_t = std::map<db_type_enum, str_t>;
  using meta_vec = std::vector<rtl::meta_dscr>;
  using str_vec  = std::vector<str_t>;
  /**
   * @brief data about specific sql statement
   *
   */
  class data_statement
  {
  public:
    data_statement()                                 = default;
    virtual ~data_statement()                        = default;
    data_statement(const data_statement& o)          = default;
    data_statement(data_statement&&) noexcept        = default;
    data_statement& operator=(const data_statement&) = default;
    // data_statement& operator=(const rtl::qry_metadata& o);
    /**
     * @brief assign from rtl::qry_metadata
     *
     * @param o
     */
    void            assign(const rtl::qry_metadata& o);
    data_statement& operator=(data_statement&&) = delete;
    /*! getters */
    [[nodiscard]] str_t id() const; ///< fetch unique id of the statement
    /**
     * @brief fetch sql statement
     * @return str_t sql statement
     */
    [[nodiscard]] str_t    sql() const;
    [[nodiscard]] str_t    summary() const;
    [[nodiscard]] str_t    dscr() const;
    std::string            dump_meta_vector(size_t offs, const char* fmt, const char* header, const meta_vec& v) const;
    [[nodiscard]] meta_vec results() const;
    [[nodiscard]] meta_vec params() const;
    [[nodiscard]] size_t   par_buf_size() const;
    [[nodiscard]] size_t   res_buf_size() const;
    [[nodiscard]] str_vec  param_names() const;
    [[nodiscard]] str_vec  result_names() const;
    /**
     * @brief dump data statement info
     *
     * @param offs
     * @return std::string serialized data statement info
     */
    [[nodiscard]] std::string dump(size_t offs) const;
    /*! setters */
    void set_id(const str_t& id);           ///< set unique id of the statement
    void set_sql(const str_t& sql);         ///< set sql for specific database type
    void set_summary(const str_t& summary); ///< set one line summary of the statement
    void set_dscr(const str_t& dscr);       ///< set description of the statement
    void set_par_buf_size(size_t par_set_size);
    void set_res_buf_size(size_t res_set_size);
    void set_results(meta_vec v);
    void set_params(meta_vec v);
    void set_result_names(str_vec v);
    void set_param_names(str_vec v);
    void set_field_len(std::map<str_t, size_t> v) { field_len_ = std::move(v); }
    /// utility methods
    void push_column_names();

    void apply_field_len(size_t fallback);
  protected:
  private:
    class rtl::logger* log_() { return rtl::logger::get(); }; /// Member variables

    str_t    id_;              ///< unique id of the data statement
    str_t    sql_;             ///< sql statement (generic or specific for RDBMS)
    str_t    summary_;         ///< one line summary of the statement
    str_t    dscr_;            ///< statement description
    size_t   par_buf_size_{1}; ///< how many parameter records we have in parameter buffer
    size_t   res_buf_size_{1}; ///< how many result records we have in result buffer
    meta_vec results_;         ///< Result-set column metadata
    meta_vec params_;          ///< Input parameter metadata
    str_vec  result_names_;    ///< alternative result column names
    str_vec  param_names_;     ///< alternative parameter column names
    /// per column width overrides from the yaml file, keyed by column name
    std::map<str_t, size_t> field_len_;
  };

  /**
   * Overwrite database column names with user defined
   */
  inline void data_statement::push_column_names()
  {
    {
      auto len = std::min(param_names_.size(), params_.size());
      for (auto cnt = 0UL; cnt < len; cnt++)
        params_[cnt].name = param_names_[cnt]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }
    {
      auto len = std::min(result_names_.size(), results_.size());
      for (auto cnt = 0UL; cnt < len; cnt++)
        results_[cnt].name = result_names_[cnt]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }
    log_()->trace("{}", dump(2));
  }

  /**
   * @brief settle the width of every column that has none
   *
   * A database cannot always say how wide a column is: PostgreSQL text,
   * json and bytea, MariaDB TEXT and BLOB carry no declared length, and the
   * backend reports 0 for those. A width of 0 would generate a zero sized
   * array, so one has to come from somewhere - the field-len entry for that
   * column if the yaml file gives one, otherwise --max-field-len.
   *
   * Only the string categories are affected. An atomic or structure column
   * is sized by its own C++ type, and its reported size is informational.
   *
   * @param fallback width to use when the yaml file says nothing
   */
  inline void data_statement::apply_field_len(size_t fallback)
  {
    auto settle = [this, fallback](meta_vec& v)
    {
      for (auto& col : v)
      {
        const auto cat       = rtl::get_sql_mapping(col.type)->category;
        const bool is_string = (cat == rtl::sql_cat::c_string || cat == rtl::sql_cat::w_string || cat == rtl::sql_cat::b_string);

        /// an explicit field-len wins even when the database did state a
        /// width - a column may be declared varchar(65535) and never hold
        /// more than a few bytes
        if (auto it = field_len_.find(col.name); it != field_len_.end())
        {
          if (! is_string)
            log_()->warn("field-len for '{}' ignored: the column is not a string, its width follows from its type.", col.name);
          else col.size = static_cast<uint32_t>(it->second);
          continue;
        }
        if (is_string && col.size == 0)
        {
          log_()->debug("Column '{}' has no declared width, using {}.", col.name, fallback);
          col.size = static_cast<uint32_t>(fallback);
        }
      }
    };
    settle(results_);
    settle(params_);
  }

} // namespace dbgen4
