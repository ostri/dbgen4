-- PostgreSQL counterpart of db/db2/create_table_types.sql.
--
-- Types that PostgreSQL spells differently: DOUBLE is double precision,
-- BINARY/VARBINARY/BLOB all collapse to bytea, and CLOB is text.
DROP TABLE IF EXISTS types_test;

CREATE TABLE types_test (
  id            integer     NOT NULL PRIMARY KEY, -- atomic, the key
  col_smallint  smallint,                         -- atomic
  col_bigint    bigint,                           -- atomic
  col_real      real,                             -- atomic
  col_double    double precision,                 -- atomic
  col_boolean   boolean,                          -- atomic
  col_decimal   numeric(10,2),                    -- c_string, travels as text
  col_char      char(2),                          -- c_string
  col_varchar   varchar(255),                     -- c_string
  col_clob      text,                             -- c_string
  col_binary    bytea,                            -- b_string
  col_varbinary bytea,                            -- b_string
  col_blob      bytea,                            -- b_string
  col_date      date,                             -- structure
  col_time      time,                             -- structure
  col_timestamp timestamp                         -- structure
);
