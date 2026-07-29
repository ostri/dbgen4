-- Table for the crud round trip test - PostgreSQL counterpart of
-- db/db2/create_table_crud.sql. Same shape, so the same test can run
-- against either backend.
DROP TABLE IF EXISTS crud_test;

CREATE TABLE crud_test (
  id      integer     NOT NULL PRIMARY KEY,  -- atomic
  name    varchar(64),                       -- c_string
  created date                               -- structure
);
