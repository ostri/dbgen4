-- PostgreSQL counterpart of db/db2/create_table_perf.sql.
DROP TABLE IF EXISTS perf_test;

CREATE TABLE perf_test (
  id      integer      NOT NULL PRIMARY KEY,
  name    varchar(255),
  created date
);
