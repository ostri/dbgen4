-- Table for the batch insert and buffered select tests.
--
-- Four columns: id/name/created are one per storage category - the point of
-- these tests is how many rows move per execute, not how many types a row
-- carries, which is types_test's job. tran is the odd one out, see below.
--
-- name is VARCHAR(255) and the test fills it to its full declared width, so
-- that a row bleeding into its neighbour in the parameter buffer shows up as
-- a wrong value rather than as nothing at all.
--
-- tran is VARCHAR(5120), filled with 5120 random printable characters per
-- row. Random rather than repeated: a benchmark table's rows are typically
-- near-identical apart from name/id, which compresses extremely well and
-- understates what a page of real, incompressible data costs to move. tran
-- exists to keep that compression from flattering the numbers.
DROP TABLE perf_test1;

CREATE TABLE perf_test1 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE,
  tran    VARCHAR(5120)
)
IN TS_TBL_32K
INDEX IN TS_NDX_32K
;

-- Two more of the same shape, for the multi-table benchmark: one block goes
-- into each of the three before a commit, which is what a real generator run
-- does - several related tables filled together inside one transaction.
DROP TABLE perf_test2;

CREATE TABLE perf_test2 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE,
  tran    VARCHAR(5120)
)
IN TS_TBL_32K
INDEX IN TS_NDX_32K
;

DROP TABLE perf_test3;

CREATE TABLE perf_test3 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE,
  tran    VARCHAR(5120)
)
IN TS_TBL_32K
INDEX IN TS_NDX_32K
;
