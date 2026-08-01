-- Table for the batch insert and buffered select tests.
--
-- Three columns, one per storage category: the point of these tests is how
-- many rows move per execute, not how many types a row carries - the type
-- coverage is types_test's job.
--
-- name is VARCHAR(255) and the test fills it to its full declared width, so
-- that a row bleeding into its neighbour in the parameter buffer shows up as
-- a wrong value rather than as nothing at all.
DROP TABLE perf_test;

CREATE TABLE perf_test (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE
);

-- Two more of the same shape, for the multi-table benchmark: one block goes
-- into each of the three before a commit, which is what a real generator run
-- does - several related tables filled together inside one transaction.
DROP TABLE perf_test2;

CREATE TABLE perf_test2 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE
);

DROP TABLE perf_test3;

CREATE TABLE perf_test3 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE
);
