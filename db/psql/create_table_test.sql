-- Test table for the psql backend.
--
-- Column names and order are kept identical to db/db2/create_table_test.sql:
-- every type both backends support natively shares one name on both sides
-- (col_numeric rather than col_decimal/col_numeric, col_text rather than
-- col_clob/col_text, ...). Where PostgreSQL has no native equivalent for a
-- DB2-only type, the closest PostgreSQL type stands in - see the comment on
-- each such column below. Counterpart of db/db2/create_table_test.sql.

DROP TABLE IF EXISTS test;

CREATE TABLE test (
  col_smallint    smallint,                    -- int2      21
  col_integer     integer,                     -- int4      23
  col_bigint      bigint,                      -- int8      20
  col_numeric     numeric(10,2),               -- numeric 1700
  col_real        real,                        -- float4   700
  col_double      double precision,            -- float8   701
  col_boolean     boolean,                     -- bool      16
  col_char        char(2),                     -- bpchar  1042
  col_varchar     varchar(255),                -- varchar 1043
  col_text        text,                        -- text      25
  col_binary      bytea,                       -- bytea     17
  col_date        date,                        -- date    1082
  col_time        time,                        -- time    1083
  col_timestamp   timestamp,                   -- timestamp   1114
  col_xml         xml,                         -- xml      142
  col_decfloat    numeric(31),                 -- stand-in: PostgreSQL has no IEEE 754-2008 decimal floating point type
  col_graphic     char(2),                     -- stand-in: PostgreSQL has no double-byte character set type, uses UTF-8 throughout
  col_dbclob      text,                        -- stand-in: PostgreSQL has no double-byte CLOB type
  col_timestamptz timestamp with time zone,    -- timestamptz 1184
  col_interval    interval,                    -- interval    1186
  col_uuid        uuid,                        -- uuid    2950
  col_varbit      bit varying(16),             -- varbit  1562
  col_jsonb       jsonb                        -- jsonb   3802
);

INSERT INTO test (
  col_smallint, col_integer, col_bigint, col_numeric, col_real, col_double,
  col_boolean, col_char, col_varchar, col_text, col_binary,
  col_date, col_time, col_timestamp, col_xml,
  col_decfloat, col_graphic, col_dbclob, col_timestamptz, col_interval, col_uuid, col_varbit, col_jsonb
)
SELECT
  i::smallint,
  i,
  i::bigint,
  (i + 0.1 * i)::numeric(10,2),
  (i + 0.01 * i)::real,
  (i + 0.001 * i)::double precision,
  (i % 2 = 0),
  chr(65 + (i % 26)) || 'X',
  'Record_' || i,
  'TEXT_data_for_record_' || i,
  decode(lpad(to_hex(i), 16, '0'), 'hex'),
  DATE '2025-10-27',
  TIME '12:00:00' + (i % 60) * INTERVAL '1 minute',
  TIMESTAMP '2025-10-27 12:00:00' + (i % 60) * INTERVAL '1 minute',
  ('<record id="' || i || '"><data>Sample XML for record ' || i || '</data></record>')::xml,
  i::numeric(31),
  chr(71 + (i % 26)),
  'DBCLOB_data_for_record_' || i,
  TIMESTAMPTZ '2025-10-27 12:00:00+02' + (i % 60) * INTERVAL '1 minute',
  (i % 60) * INTERVAL '1 second',
  ('00000000-0000-4000-8000-' || lpad(to_hex(i), 12, '0'))::uuid,
  (i % 65536)::bit(16)::bit varying,
  ('{"id": ' || i || '}')::jsonb
FROM generate_series(1, 10) AS i;

SELECT count(*) AS inserted FROM test;
