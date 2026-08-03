#!/usr/bin/env bash
# asyn_db2_fast.sh - fast, fixed-size run of the sync-vs-async_db insert
# throughput benchmark (test_bench_async.cpp) against the DB2 test database.
#
# For correctness regression checking, not throughput measurement: buffer,
# iteration and delay knobs are pinned rather than left at asyn_db2.sh's
# heavier defaults, so a run finishes in a few seconds. A run of this script
# taking longer than before is itself a regression signal - something is
# slower even at this fixed, tiny workload.
#
# Run from wherever the binary lives: build/release, build/debug or
# build/profile (e.g. `../../shell/asyn_db2_fast.sh` from build/release).
#
# The test case is tagged [.benchmark] so an ordinary ctest/catch2 run
# skips it; it must be requested explicitly by tag, as done below.
set -euo pipefail

BIN="./test_crud_db2"

if [[ ! -x "${BIN}" ]]; then
    echo "error: ${BIN} not found in $(pwd) - run this from build/release, build/debug or build/profile" >&2
    exit 1
fi

# --- connection to the DB2 test database (CMakeLists.txt DB2_TEST_* defaults) ---
export DBGEN4_TEST_HOST="${DBGEN4_TEST_HOST:-localhost}"
export DBGEN4_TEST_PORT="${DBGEN4_TEST_PORT:-50000}"
export DBGEN4_TEST_DB="${DBGEN4_TEST_DB:-dbgen4}"
export DBGEN4_TEST_USER="${DBGEN4_TEST_USER:-dbgen4}"
export DBGEN4_TEST_PASS="${DBGEN4_TEST_PASS:-dbgen4}"

# --- benchmark workload knobs, pinned small and fixed for a fast run ---
export DBGEN4_BUFFER_SIZE=10    # rows per block per table
export DBGEN4_ITERATIONS=2      # iterations (blocks)
export DBGEN4_COMMIT_EVERY=2    # commit after this many blocks
export DBGEN4_FILL_DELAY_MS=10  # simulated work per block
export DBGEN4_REPORT_EVERY=2    # commits between progress reports

# --- logging config override -----------------------------------------------
# The shipped config/log.debug.conf and config/log.release.conf pin
# console_level to "warn", which swallows the benchmark's log->info()
# progress/result lines (log->set_level(info) only raises the logger's own
# level, not the console sink's independent level - see logger_impl.cpp).
# LOG_CONFIG points the logger at an alternate config instead of patching
# the repo's own files. console_level=info here is what makes the
# benchmark's report lines show up on the terminal.
#
# Both the config and the log file live under /tmp: log_folder points there
# too, so nothing is written under build/<variant>/logs - that directory
# gets wiped periodically anyway, and this way there's nothing to clean up
# by hand. The log file itself is left behind in /tmp on purpose, to look
# at after the run; only the temporary config file is removed on exit.
LOG_DIR="/tmp/dbgen4-bench-db2-fast"
mkdir -p "${LOG_DIR}"
LOG_CONFIG_FILE="$(mktemp /tmp/dbgen4_log_bench_db2_fast.XXXXXX.conf)"
trap 'rm -f "${LOG_CONFIG_FILE}"' EXIT

cat > "${LOG_CONFIG_FILE}" <<EOF
{
  "app_name": "dbgen4-bench-db2",
  "mode": "sync",
  "console_level": "info",
  "file_level": "info",
  "rotation_hour": 2,
  "rotation_minute": 30,
  "keep_days": 14,
  "pattern": "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v",
  "log_folder": "${LOG_DIR}",
  "flush_on": "info"
}
EOF

export LOG_CONFIG="${LOG_CONFIG_FILE}"

echo "log file: ${LOG_DIR}/dbgen4-bench-db2.log" >&2

# --reporter compact keeps Catch2's own per-assertion PASSED/section noise
# out of the run; only the logger's lines (via console_level=info above)
# and Catch2's final pass/fail summary are printed.
exec "${BIN}" "[.benchmark][async]" --reporter compact
