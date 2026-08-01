-- Test table for the db2 backend.
--
-- Column names and order are kept identical to db/psql/create_table_test.sql:
-- every type both backends support natively shares one name on both sides
-- (col_numeric rather than col_decimal/col_numeric, col_text rather than
-- col_clob/col_text, ...). Where DB2 has no native equivalent for a
-- PostgreSQL-only type, the closest DB2 type stands in - see the comment on
-- each such column below. Counterpart of db/psql/create_table_test.sql.

DROP TABLE test;

CREATE TABLE test (
  col_smallint    smallint,                 -- int2
  col_integer     integer,                  -- int4
  col_bigint      bigint,                   -- int8
  col_numeric     decimal(10,2),            -- numeric
  col_real        real,                     -- float4
  col_double      double,                   -- float8
  col_boolean     boolean,                  -- bool
  col_char        char(2),                  -- bpchar
  col_varchar     varchar(255),             -- varchar
  col_text        clob(1k),                 -- text
  col_binary      varbinary(16),            -- bytea
  col_date        date,                     -- date
  col_time        time,                     -- time
  col_timestamp   timestamp,                -- timestamp
  col_xml         xml,                      -- xml
  col_decfloat    decfloat,                 -- DB2-only: IEEE 754-2008 decimal floating point, no PostgreSQL equivalent
  col_graphic     graphic(2),               -- DB2-only: double-byte character set type, no PostgreSQL equivalent
  col_dbclob      dbclob(1k),               -- DB2-only: double-byte CLOB, no PostgreSQL equivalent
  col_timestamptz timestamp,                -- stand-in: DB2 TIMESTAMP carries no time zone, unlike PostgreSQL timestamptz
  col_interval    varchar(64),              -- stand-in: DB2 CLI does not expose SQL_INTERVAL_* for a plain column here
  col_uuid        char(36),                 -- stand-in: DB2 has no native UUID type, stored as its text form
  col_varbit      varbinary(16),            -- stand-in: DB2 has no bit varying type, stored as raw bytes
  col_jsonb       clob(1k)                  -- stand-in: DB2 has no native JSON type or validation
);

INSERT INTO test (
  col_smallint, col_integer, col_bigint, col_numeric, col_real, col_double,
  col_boolean, col_char, col_varchar, col_text, col_binary,
  col_date, col_time, col_timestamp, col_xml,
  col_decfloat, col_graphic, col_dbclob, col_timestamptz, col_interval, col_uuid, col_varbit, col_jsonb
)
WITH t (i) AS (
  VALUES (1)
  UNION ALL
  SELECT i + 1 FROM t WHERE i < 10
)
SELECT
  i,
  i,
  i,
  i + 0.1 * i,
  i + 0.01 * i,
  i + 0.001 * i,
  CASE WHEN MOD(i, 2) = 0 THEN 1 ELSE 0 END,
  CHR(65 + MOD(i, 26)) || 'X',
  'Record_' || i,
  'TEXT_data_for_record_' || i,
  CAST(HEXTORAW(REPEAT('0', 16 - LENGTH(HEX(i))) || HEX(i)) AS VARBINARY(16)),
  DATE('2025-10-27'),
  TIME('12:00:00') + (MOD(i, 60)) MINUTES,
  TIMESTAMP('2025-10-27 12:00:00') + (MOD(i, 60)) MINUTES,
  XMLPARSE(DOCUMENT '<record id="' || i || '"><data>Sample XML for record ' || i || '</data></record>'),
  CAST(i AS DECFLOAT),
  CHR(71 + MOD(i, 26)),
  'DBCLOB_data_for_record_' || i,
  TIMESTAMP('2025-10-27 12:00:00') + (MOD(i, 60)) MINUTES,
  MOD(i, 60) || ' seconds',
  '00000000-0000-4000-8000-' || LPAD(HEX(i), 12, '0'),
  CAST(HEXTORAW(REPEAT('0', 16 - LENGTH(HEX(MOD(i, 65536)))) || HEX(MOD(i, 65536))) AS VARBINARY(16)),
  '{"id": ' || i || '}'
FROM t;

SELECT count(*) AS inserted FROM test;
