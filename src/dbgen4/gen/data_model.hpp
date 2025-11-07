#pragma once
#include "common.hpp"
#include "db2_rtl.hpp"

namespace dbgen4
{
  namespace gen
  {
    const auto block_align_128 = 128; ///< 128 bytes alignment for code generation blocks
    const auto block_align_64  = 64;  ///< 64 bytes alignment for code generation blocks
    struct meta_dscr
    {
      str_t name;              ///< Column name as returned by the db
                               ///<  or the parameter name provided to the database
      rtl::sql_type type;      ///< Mapped type from dbgen4::sql_type
      int16_t       odbc_type; ///< Raw ODBC SQL type code (e.g., SQL_INTEGER)
      uint32_t      size;      ///< Maximum column size in characters/bytes
      int16_t       digits;    ///< Number of digits after decimal point (for numeric)
      int16_t       nullable;  ///< SQL_NO_NULLS, SQL_NULLABLE, or SQL_NULLABLE_UNKNOWN
    } __attribute__((aligned(block_align_64)));
    using vec_meta_t = std::vector<meta_dscr>;
    struct statement
    {
      str_t      id;      ///< statement unique id
      str_t      sql;     ///< sql statement
      str_t      desc;    ///< statement description
      vec_meta_t columns; ///< result-set column metadata
      vec_meta_t params;  ///< input parameter metadata
    } __attribute__((aligned(block_align_128)));
    using vec_stmts_t = std::vector<statement>;
    struct document
    {
      str_t       summary;     ///< short description of the document
      str_t       description; ///< long description of the document
      vec_stmts_t statements;  ///< statements in the document
    } __attribute__((aligned(block_align_128)));
  }; // namespace gen
}; // namespace dbgen4