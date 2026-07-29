-- Test table for the psql backend.
--
-- Deliberately covers every type OID that rtl::psql::from_oid() claims to
-- know, so that a describe of "select * from test" exercises the whole
-- mapping in one go. Counterpart of db/db2/create_table_test.sh.

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
  col_bytea       bytea,                       -- bytea     17
  col_varbit      bit varying(16),             -- varbit  1562
  col_date        date,                        -- date    1082
  col_time        time,                        -- time    1083
  col_timestamp   timestamp,                   -- timestamp   1114
  col_timestamptz timestamp with time zone,    -- timestamptz 1184
  col_interval    interval,                    -- interval    1186
  col_uuid        uuid,                        -- uuid    2950
  col_json        json,                        -- json     114
  col_jsonb       jsonb,                       -- jsonb   3802
  col_xml         xml                          -- xml      142
);

INSERT INTO test (
  col_smallint, col_integer, col_bigint, col_numeric, col_real, col_double,
  col_boolean, col_char, col_varchar, col_text, col_bytea, col_varbit,
  col_date, col_time, col_timestamp, col_timestamptz, col_interval,
  col_uuid, col_json, col_jsonb, col_xml
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
  (i % 65536)::bit(16)::bit varying,
  DATE '2025-10-27',
  TIME '12:00:00' + (i % 60) * INTERVAL '1 minute',
  TIMESTAMP '2025-10-27 12:00:00' + (i % 60) * INTERVAL '1 minute',
  TIMESTAMPTZ '2025-10-27 12:00:00+02' + (i % 60) * INTERVAL '1 minute',
  (i % 60) * INTERVAL '1 second',
  ('00000000-0000-4000-8000-' || lpad(to_hex(i), 12, '0'))::uuid,
  ('{"id": ' || i || '}')::json,
  ('{"id": ' || i || '}')::jsonb,
  ('<record id="' || i || '"><data>Sample XML for record ' || i || '</data></record>')::xml
FROM generate_series(1, 10) AS i;

SELECT count(*) AS inserted FROM test;
