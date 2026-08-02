-- Table for the all types round trip test.
--
-- One column per sql type the generator maps today, so that a wrong binding
-- for any of them shows up as a failing test rather than as silence. The
-- widths match what yaml/crud.yaml expects the generated buffers to be.
--
-- XML is deliberately absent: it round trips as text but DB2 will not accept a
-- plain string parameter for an XML column without XMLPARSE, which is a
-- statement level concern rather than a binding one.
DROP TABLE types_test;

CREATE TABLE types_test (
  id           INTEGER      NOT NULL PRIMARY KEY, -- atomic, the key
  col_smallint SMALLINT,                          -- atomic
  col_bigint   BIGINT,                            -- atomic
  col_real     REAL,                              -- atomic
  col_double   DOUBLE,                            -- atomic
  col_boolean  BOOLEAN,                           -- atomic (bound as SQL_C_BIT)
  col_decimal  DECIMAL(10,2),                     -- c_string, travels as text
  col_char     CHAR(2),                           -- c_string
  col_varchar  VARCHAR(255),                       -- c_string
  col_clob     CLOB(1K),                          -- c_string
  col_binary   BINARY(8),                         -- b_string
  col_varbinary VARBINARY(16),                    -- b_string
  col_blob     BLOB(1K),                          -- b_string
  col_date     DATE,                              -- structure
  col_time     TIME,                              -- structure
  col_timestamp TIMESTAMP                         -- structure
)
IN TS_TBL_32K
INDEX IN TS_NDX_32K
;
