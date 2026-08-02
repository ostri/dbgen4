-- PostgreSQL counterpart of db/db2/create_table_perf.sql.
DROP TABLE IF EXISTS perf_test1;

CREATE TABLE perf_test1 (
  id      integer      NOT NULL PRIMARY KEY,
  name    varchar(255),
  created date
);

-- Two more of the same shape, for the multi-table benchmark: one block goes
-- into each of the three before a commit, which is what a real generator run
-- does - several related tables filled together inside one transaction.
DROP TABLE IF EXISTS perf_test2;

CREATE TABLE perf_test2 (
  id      integer      NOT NULL PRIMARY KEY,
  name    varchar(255),
  created date
);

DROP TABLE IF EXISTS perf_test3;

CREATE TABLE perf_test3 (
  id      integer      NOT NULL PRIMARY KEY,
  name    varchar(255),
  created date
);
