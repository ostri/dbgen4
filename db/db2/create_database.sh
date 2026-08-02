#!/bin/bash
#
# Create the OS account, DB2 database and DB2 user the db2 tests run as.
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
#     ./db/db2/create_database.sh
#
# Follow with create_tables.sh to create the tables the tests need.
#
# The password matches DB2_TEST_PASS in CMakeLists.txt. Both are deliberately
# trivial: this account exists only to talk to a local development database.
set -euo pipefail

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=test

echo "--- operating system account ---"
if id "$TEST_USER" >/dev/null 2>&1; then
    echo "user $TEST_USER already exists"
else
    sudo useradd -m -c "dbgen4 test account" -s /bin/bash "$TEST_USER"
    echo "$TEST_USER:$TEST_PASS" | sudo chpasswd
    echo "user $TEST_USER created"
fi

echo "--- database ---"
# shellcheck disable=SC1091
source ~/sqllib/db2profile 2>/dev/null || source /home/db2inst1/sqllib/db2profile

if db2 "LIST DATABASE DIRECTORY" | grep -qi "$TEST_DB"; then
    echo "database $TEST_DB already exists"
else
    db2 "CREATE DATABASE $TEST_DB AUTOMATIC STORAGE YES USING CODESET UTF-8 TERRITORY SI"
    echo "database $TEST_DB created"
fi

echo "--- database authorities ---"
db2 "connect to $TEST_DB"
db2 "GRANT DBADM, CREATETAB, BINDADD, CONNECT, IMPLICIT_SCHEMA, LOAD ON DATABASE TO USER ${TEST_USER^^}"
db2 "commit"
db2 terminate

echo
echo "done. Run create_tables.sh next to create the tables the tests need."
echo "The transaction log may also need raising for the benchmarks:"
echo "  db2 UPDATE DB CFG FOR $TEST_DB USING LOGFILSIZ 16384 LOGPRIMARY 10 LOGSECOND 6"
echo "  (deactivate and reactivate the database afterwards)"
