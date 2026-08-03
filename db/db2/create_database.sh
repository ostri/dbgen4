#!/bin/bash
#
# Create the OS account, DB2 database, DB2 user, tablespace, bufferpool and
# log/memory tuning (via tuning.sh) for the db2 tests.
#
# DB2 authenticates against the operating system, so a "database user" here is
# an OS user. The tests use a dedicated one rather than a developer's own login.
#
# dbgen4 owns its own schema, so the tables the tests create, fill and truncate
# are its own - nothing in anybody else's schema is touched. It holds DBADM on
# the test database so that the tests can manage those tables themselves.
#
# Run once per machine, as a user with sudo and with DB2 admin rights:
#
#     ./db/db2/create_database.sh
#
# Restarts the DB2 instance (via tuning.sh) before it returns - anything else
# connected to it is disconnected. Follow with create_tables.sh to create the
# tables the tests need.
#
set -euo pipefail

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=dbgen4
BP_TBL_NAME=BP_TBL_32K
BP_NDX_NAME=BP_NDX_32K
TS_TBL_NAME=TS_TBL_32K
TS_NDX_NAME=TS_NDX_32K

echo "--- operating system account ---"
if id "${TEST_USER}" >/dev/null 2>&1; then
    echo "user ${TEST_USER} already exists"
else
    sudo useradd -m -c "dbgen4 test account" -s /bin/bash "${TEST_USER}"
    echo "${TEST_USER}:${TEST_PASS}" | sudo chpasswd
    echo "user ${TEST_USER} created"
fi
echo "--- database ---"
# shellcheck disable=SC1091
source ~/sqllib/db2profile 2>/dev/null || source /home/db2inst1/sqllib/db2profile

if db2 "LIST DATABASE DIRECTORY" | grep -qi "${TEST_DB}"; then
    echo "database ${TEST_DB} already exists"
else
    db2 "CREATE DATABASE ${TEST_DB} AUTOMATIC STORAGE YES USING CODESET UTF-8 TERRITORY SI"
    echo "database ${TEST_DB} created"
fi

echo "--- database authorities ---"
# Grant privileges in an isolated block
db2 -t <<-EOF >/dev/null
    CONNECT TO ${TEST_DB};
    GRANT DBADM, CREATETAB, BINDADD, CONNECT, IMPLICIT_SCHEMA, LOAD ON DATABASE TO USER ${TEST_USER^^};
    COMMIT;
EOF
echo "--- database authorities granted to ${TEST_USER} ---"

# Establish a persistent CLP connection for the following single-line commands
db2 -t "CONNECT TO ${TEST_DB};" >/dev/null

echo "--- drop existing objects (if any) & create bufferpool + tablespace ---"
# Temporarily allow the DROP to fail if the object does not exist
set +e
db2 -tv <<-EOF >/dev/null
    CONNECT TO ${TEST_DB};
    DROP TABLESPACE ${TS_TBL_NAME};
    COMMIT;
EOF
db2 -tv <<-EOF >/dev/null
    CONNECT TO ${TEST_DB};
    DROP BUFFERPOOL ${BP_TBL_NAME};
    COMMIT;
EOF
#
db2 -tv <<-EOF >/dev/null
    CONNECT TO ${TEST_DB};
    DROP TABLESPACE ${TS_NDX_NAME};
    COMMIT;
EOF
db2 -tv <<-EOF >/dev/null
    CONNECT TO ${TEST_DB};
    DROP BUFFERPOOL ${BP_NDX_NAME};
    COMMIT;
EOF
set -e # Re-enable exit-on-error for the rest of the script

# create the objects
#
# TS_TBL_NAME's INITIALSIZE is sized for a full-width benchmark run - three
# perf tables filled with 200000+ rows each of a full 5120 byte tran is on
# the order of 3 GB. A tablespace that starts near its working size avoids
# the small, frequent AUTORESIZE extensions a low INITIALSIZE forces once it
# fills up - each one is a synchronous allocation that stalls writes without
# using CPU or showing up as disk throughput, which reads as the whole
# benchmark mysteriously stalling.
db2 -t <<-EOF >/dev/null
    CONNECT TO ${TEST_DB};
    CREATE BUFFERPOOL ${BP_TBL_NAME} PAGESIZE 32K;
    COMMIT;
    CREATE BUFFERPOOL ${BP_NDX_NAME} PAGESIZE 32K;
    COMMIT;
    CREATE TABLESPACE ${TS_TBL_NAME}
        PAGESIZE 32K
        MANAGED BY AUTOMATIC STORAGE
        BUFFERPOOL ${BP_TBL_NAME}
        AUTORESIZE YES
        INITIALSIZE 4000M
        INCREASESIZE 20 PERCENT
        MAXSIZE NONE;
    COMMIT;
    CREATE TABLESPACE ${TS_NDX_NAME}
        PAGESIZE 32K
        MANAGED BY AUTOMATIC STORAGE
        BUFFERPOOL ${BP_NDX_NAME}
        AUTORESIZE YES
        INITIALSIZE 100M
        INCREASESIZE 10 PERCENT
        MAXSIZE NONE;
    COMMIT;
EOF
echo "--- buffer pool: ${BP_TBL_NAME} table space: ${TS_TBL_NAME} created ---"
db2 terminate >/dev/null

# CREATE DATABASE resets the transaction log and bufferpool sizing to DB2's
# defaults (~100 MB of log total) regardless of what a previous run of this
# script configured - a benchmark writing a full block of tran-sized rows
# overruns that in one commit (SQL0964). tuning.sh raises both and restarts
# the instance (db2stop force / db2start) for the change to take effect.
echo "--- applying log/bufferpool tuning ---"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bash "${SCRIPT_DIR}/tuning.sh"