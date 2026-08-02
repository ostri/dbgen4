#!/bin/bash
#
# Create every table the psql tests need, owned by dbgen4.
#
# Run after create_database.sh (point it at the actual test server through
# the usual PG* environment variables, e.g. PGHOST=postgres.lan):
#
#     ./db/psql/create_tables.sh
set -euo pipefail

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=dbgen4

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for sql in create_table_crud.sql create_table_perf.sql create_table_test.sql create_table_types.sql; do
    echo "--- $sql ---"
    PGPASSWORD="$TEST_PASS" psql -U "$TEST_USER" -d "$TEST_DB" -f "$SCRIPT_DIR/$sql"
done

echo "--- what $TEST_USER now owns ---"
PGPASSWORD="$TEST_PASS" psql -U "$TEST_USER" -d "$TEST_DB" -c "\dt"
