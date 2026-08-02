#!/bin/bash
#
# Create every table the db2 tests need, in the dbgen4 schema.
#
# Run after create_database.sh, as the dbgen4 account:
#
#     ./db/db2/create_tables.sh
#
# Each table's DROP TABLE fails harmlessly on a fresh database where the
# table does not exist yet - db2 -tvf keeps going past it.
set -euo pipefail

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=dbgen4

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck disable=SC1091
source ~/sqllib/db2profile 2>/dev/null || source /home/db2inst1/sqllib/db2profile

db2 "connect to $TEST_DB user $TEST_USER using $TEST_PASS"

for sql in create_table_crud.sql create_table_perf.sql create_table_test.sql create_table_types.sql; do
    echo "--- $sql ---"
    db2 -tvf "$SCRIPT_DIR/$sql" || true
done

db2 "commit"

echo "--- what ${TEST_USER^^} now owns ---"
db2 -x "select tabname from syscat.tables where tabschema='${TEST_USER^^}' and type='T'"
db2 terminate
