#!/bin/bash
#
# Create the OS and DB2 account the db2 tests run as, and the tables they need.
#
# DB2 authenticates against the operating system, so a "database user" here is
# an OS user. The tests use a dedicated one rather than a developer's own login:
# a personal account would mean the build carries that person's login password
# in a CMake cache variable, and every checkout would need a different one.
#
# dbgen4 owns its own schema, so the tables the tests create, fill and truncate
# are its own - nothing in anybody else's schema is touched. It holds DBADM on
# the test database so that the tests can manage those tables themselves.
#
# Run once per machine, as a user with sudo and with DB2 admin rights:
#
#     ./db/db2/create_test_user.sh
#
# The password matches DB2_TEST_PASS in CMakeLists.txt. Both are deliberately
# trivial: this account exists only to talk to a local development database.
set -euo pipefail

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=test
# an account that may already administer the database, used to issue the GRANT
ADMIN_USER=${DB2_ADMIN_USER:-$(id -un)}

echo "--- operating system account ---"
if id "$TEST_USER" >/dev/null 2>&1; then
    echo "user $TEST_USER already exists"
else
    sudo useradd -m -c "dbgen4 test account" -s /bin/bash "$TEST_USER"
    echo "$TEST_USER:$TEST_PASS" | sudo chpasswd
    echo "user $TEST_USER created"
fi

echo "--- database authorities ---"
# shellcheck disable=SC1091
source ~/sqllib/db2profile 2>/dev/null || source /home/db2inst1/sqllib/db2profile

db2 "connect to $TEST_DB"
db2 "GRANT DBADM, CREATETAB, BINDADD, CONNECT, IMPLICIT_SCHEMA, LOAD ON DATABASE TO USER ${TEST_USER^^}"
db2 "commit"
db2 terminate

echo "--- tables, in the ${TEST_USER^^} schema ---"
db2 "connect to $TEST_DB user $TEST_USER using $TEST_PASS"

db2 "CREATE TABLE crud_test (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(64),
  created DATE)"

db2 "CREATE TABLE types_test (
  id            INTEGER      NOT NULL PRIMARY KEY,
  col_smallint  SMALLINT,
  col_bigint    BIGINT,
  col_real      REAL,
  col_double    DOUBLE,
  col_boolean   BOOLEAN,
  col_decimal   DECIMAL(10,2),
  col_char      CHAR(2),
  col_varchar   VARCHAR(255),
  col_clob      CLOB(1K),
  col_binary    BINARY(8),
  col_varbinary VARBINARY(16),
  col_blob      BLOB(1K),
  col_date      DATE,
  col_time      TIME,
  col_timestamp TIMESTAMP)"

db2 "CREATE TABLE perf_test1 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE)"

db2 "CREATE TABLE perf_test2 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE)"

db2 "CREATE TABLE perf_test3 (
  id      INTEGER      NOT NULL PRIMARY KEY,
  name    VARCHAR(255),
  created DATE)"

# the wide table yaml/t1.yaml describes - the generator reads its layout at
# build time, so it has to exist before the first build
db2 "CREATE TABLE test (
  col_SMALLINT   SMALLINT,
  col_INTEGER    INTEGER,
  col_BIGINT     BIGINT,
  col_DECIMAL    DECIMAL(10,2),
  col_REAL       REAL,
  col_DOUBLE     DOUBLE,
  col_DECFLOAT   DECFLOAT,
  col_CHAR       CHAR(2),
  col_VARCHAR    VARCHAR(255),
  col_CLOB       CLOB(1K),
  col_GRAPHIC    GRAPHIC(2),
  col_VARGRAPHIC VARGRAPHIC(255),
  col_DBCLOB     DBCLOB(1K),
  col_BLOB       BLOB(1K),
  col_BINARY     BINARY(8),
  col_VARBINARY  VARBINARY(16),
  col_DATE       DATE,
  col_TIME       TIME,
  col_TIMESTAMP  TIMESTAMP,
  col_BOOLEAN    BOOLEAN,
  col_XML        XML)"

db2 "commit"

echo "--- what ${TEST_USER^^} now owns ---"
db2 -x "select tabname from syscat.tables where tabschema='${TEST_USER^^}' and type='T'"
db2 terminate

echo
echo "done. The transaction log may also need raising for the benchmarks:"
echo "  db2 UPDATE DB CFG FOR $TEST_DB USING LOGFILSIZ 16384 LOGPRIMARY 10 LOGSECOND 6"
echo "  (deactivate and reactivate the database afterwards)"
