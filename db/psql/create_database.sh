#!/bin/bash
#
# Create the PostgreSQL role and database the psql tests run as.
#
# Run once per server, as a user with superuser or CREATEROLE/CREATEDB rights
# (defaults to connecting as the postgres superuser on localhost; point it at
# the actual test server through the usual PG* environment variables, e.g.
# PGHOST=postgres.lan):
#
#     ./db/psql/create_database.sh
#
# Follow with create_tables.sh to create the tables the tests need.
#
# The password matches PSQL_TEST_PASS in CMakeLists.txt. Both are deliberately
# trivial: this account exists only to talk to a local development database.
set -euo pipefail

TEST_USER=dbgen4
TEST_PASS=dbgen4
TEST_DB=dbgen4
ADMIN_USER=${PGUSER:-postgres}

echo "--- role ---"
if psql -U "$ADMIN_USER" -d postgres -tAc "select 1 from pg_roles where rolname='$TEST_USER'" | grep -q 1; then
    echo "role $TEST_USER already exists"
else
    psql -U "$ADMIN_USER" -d postgres -c "CREATE ROLE $TEST_USER LOGIN PASSWORD '$TEST_PASS'"
    echo "role $TEST_USER created"
fi

echo "--- database ---"
if psql -U "$ADMIN_USER" -d postgres -tAc "select 1 from pg_database where datname='$TEST_DB'" | grep -q 1; then
    echo "database $TEST_DB already exists"
else
    createdb -U "$ADMIN_USER" -O "$TEST_USER" "$TEST_DB"
    echo "database $TEST_DB created"
fi

echo
echo "done. Run create_tables.sh next to create the tables the tests need."
