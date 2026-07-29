-- Table for the crud round trip test.
--
-- Deliberately small: one column per storage category the test covers so far
-- (atomic, c_string, structure). It grows as the test covers more types.
DROP TABLE crud_test;

CREATE TABLE crud_test (
  id      INTEGER      NOT NULL PRIMARY KEY,  -- atomic
  name    VARCHAR(64),                        -- c_string
  created DATE                                -- structure
);
