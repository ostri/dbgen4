#!/bin/bash
#
# Create the PostgreSQL role and database the psql tests run as.
#
# Run once per server, against the dedicated test server:
#
#     ./db/psql/create_database.sh
#
# Follow with create_tables.sh to create the tables the tests need.
#
# Host, admin and test account are hardcoded rather than left to PG*
# environment variables: this only ever runs against the one disposable test
# server, and admin and test passwords are deliberately trivial to match -
# there is nothing here worth parameterizing or protecting.
set -euo pipefail

PGHOST=postgres.lan
PGPORT=5432

ADMIN_USER=postgres
ADMIN_PASS=postgres1

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=dbgen4

export PGHOST PGPORT

echo "--- role ---"
if PGPASSWORD="$ADMIN_PASS" psql -U "$ADMIN_USER" -d postgres -tAc "select 1 from pg_roles where rolname='$TEST_USER'" | grep -q 1; then
    echo "role $TEST_USER already exists"
else
    PGPASSWORD="$ADMIN_PASS" psql -U "$ADMIN_USER" -d postgres -c "CREATE ROLE $TEST_USER LOGIN PASSWORD '$TEST_PASS'"
    echo "role $TEST_USER created"
fi

echo "--- database ---"
if PGPASSWORD="$ADMIN_PASS" psql -U "$ADMIN_USER" -d postgres -tAc "select 1 from pg_database where datname='$TEST_DB'" | grep -q 1; then
    echo "database $TEST_DB already exists"
else
    PGPASSWORD="$ADMIN_PASS" createdb -U "$ADMIN_USER" -O "$TEST_USER" "$TEST_DB"
    echo "database $TEST_DB created"
fi

echo
echo "done. Run create_tables.sh next to create the tables the tests need."
