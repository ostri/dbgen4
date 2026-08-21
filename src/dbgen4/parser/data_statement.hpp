#pragma once

#include "common.hpp"
#include "rtl.hpp"
#include <logger/logger.hpp>
#include <map>
// #include <map>

namespace dbgen4::gen
{
  // /**
  //  * @brief array of sql's where each db type flavour has its own possible empty version
  //  *
  //  * sql is for generic sql statement. Others are for RDBMs specifics
  //  */
  // using map_db_type_t = std::map<db_type_enum, str_t>;
  using meta_vec = std::vector<rtl::schema::meta_dscr>;
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
    [[nodiscard]] str_t sql() const;
    /**
     * @brief sql to run once, before this statement's own sql is validated
     * against the database (see parser::load_file_meta_data())
     *
     * Empty when the yaml file gives no "before" block for this statement -
     * the common case. Exists so a statement whose own sql depends on
     * something not yet in the schema (e.g. a staging table another
     * statement's INSERT reads from) can bring it into existence right
     * before validation needs it, rather than requiring the caller to set
     * the database up out of band first.
     *
     * @return str_t sql to run before validation, or empty
     */
    [[nodiscard]] str_t before_sql() const;
    /**
     * @brief sql to run once, after this statement has been validated
     *
     * The counterpart to before_sql() - tears down whatever it set up (e.g.
     * drops a staging table), so a generator run leaves the database no
     * different than it found it. Run whether or not this statement's own
     * sql validated successfully, so a failure partway through generation
     * does not leave before_sql()'s side effects behind.
     *
     * @return str_t sql to run after validation, or empty
     */
    [[nodiscard]] str_t    after_sql() const;
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
    void set_before_sql(const str_t& sql);  ///< set sql to run once before validation - see before_sql()
    void set_after_sql(const str_t& sql);   ///< set sql to run once after validation - see after_sql()
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
    void push_column_names(logger::Logger& log);

    void apply_field_len(size_t fallback, logger::Logger& log);
  protected:
  private:
    str_t    id_;              ///< unique id of the data statement
    str_t    sql_;             ///< sql statement (generic or specific for RDBMS)
    str_t    before_sql_;      ///< sql to run once before validation - see before_sql()
    str_t    after_sql_;       ///< sql to run once after validation - see after_sql()
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
  inline void data_statement::push_column_names(logger::Logger& log)
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
    log.trace("{}", dump(2));
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
   * @param log Logger to report ignored/defaulted widths through
   */
  inline void data_statement::apply_field_len(size_t fallback, logger::Logger& log)
  {
    auto settle = [this, fallback, &log](meta_vec& v)
    {
      for (auto& col : v)
      {
        const auto cat = rtl::schema::get_sql_mapping(col.type)->category;
        const bool is_string =
          (cat == rtl::schema::sql_cat::c_string || cat == rtl::schema::sql_cat::w_string || cat == rtl::schema::sql_cat::b_string);

        /// an explicit field-len wins even when the database did state a
        /// width - a column may be declared varchar(65535) and never hold
        /// more than a few bytes
        if (auto it = field_len_.find(col.name); it != field_len_.end())
        {
          if (! is_string) log.warn("field-len for '{}' ignored: the column is not a string, its width follows from its type.", col.name);
          else col.size = static_cast<uint32_t>(it->second);
          continue;
        }
        if (is_string && col.size == 0)
        {
          log.debug("Column '{}' has no declared width, using {}.", col.name, fallback);
          col.size = static_cast<uint32_t>(fallback);
        }
      }
    };
    settle(results_);
    settle(params_);
  }

} // namespace dbgen4::gen
