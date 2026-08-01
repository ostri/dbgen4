# dbgen4

database access layer generator

## build

   cmake --preset ninja-debug
   cmake --build --preset debug -j8
   ctest --test-dir build/debug -L unit --output-on-failure

Presets: `ninja-debug`, `ninja-release`, `ninja-profile`. The last one is a
placeholder for profiling the runtime and the generated code - it configures
`RelWithDebInfo` without sanitizers, but no profiler flags are wired up yet.

The presets do not pin a compiler. The project has to build under both gcc and
clang, so pick one through the environment rather than through a preset:

   cmake --preset ninja-debug                 # system default (gcc)
   CXX=clang++ cmake -S . -B build/clang -G Ninja -DCMAKE_BUILD_TYPE=Debug

A build tree remembers its compiler once configured, so switching means a fresh
directory, not a reconfigure of the old one.

## testing

All test sources live in [src/test/](src/test/). They build into three
binaries:

| binary | source | needs a database |
| --- | --- | --- |
| `unit_tests` | `main.cpp`, `parser_test.cpp`, `log_test.cpp` | no |
| `test_t1` | `test_t1.cpp` | no |
| `test_crud` | `test_crud.cpp`, `test_types.cpp`, `test_perf.cpp`, `test_bench.cpp` | yes |

`test_db.hpp` holds the connection fixture the database tests share.

### running

    ctest --test-dir build/debug                       # everything
    ctest --test-dir build/debug --output-on-failure   # with output when one fails
    ctest --test-dir build/debug -j8                   # in parallel

By label:

    ctest --test-dir build/debug -L unit      # parser and log, no database
    ctest --test-dir build/debug -L smoke     # generated buffers, no database
    ctest --test-dir build/debug -L crud      # round trips against a live database
    ctest --test-dir build/debug -L live-db   # same set, by what they need

One test by name (a regular expression):

    ctest --test-dir build/debug -R "batch of a hundred" --output-on-failure
    ctest --test-dir build/debug -N            # list without running

### running a test binary directly

ctest passes the connection details in the environment. Running a binary by
hand means supplying them, otherwise the built in defaults apply
(`localhost:50000`, database `test`, user `dbgen4`, password `dbgen4`):

    DBGEN4_TEST_HOST=localhost DBGEN4_TEST_PORT=50000 DBGEN4_TEST_DB=test \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
      ./build/debug/test_crud "crud round trip through the generated buffers"

Useful Catch2 flags: `--list-tests` for the names, `-s` to show successful
assertions as well (this is how the benchmarks print their timings), `-?` for
the rest. Note that a test name containing a comma has to be selected by tag
rather than by name - Catch2 reads the comma as a filter separator.

### the database account the tests use

The DB2 tests connect as a dedicated `dbgen4` account rather than as whoever is
logged in. DB2 authenticates against the operating system, so a database user
here is an OS user, and putting a developer's own login password into a CMake
cache variable would make the build personal to one machine.

One script sets up both the account and the tables it owns:

    ./db/db2/create_test_user.sh

It creates the OS user `dbgen4` (password `dbgen4`), grants it DBADM on the
`test` database, and creates `crud_test`, `types_test`, `perf_test` and `test`
in its own schema. Everything the tests create, fill and truncate therefore
belongs to `dbgen4` - no other schema is touched. Both password and username are
deliberately trivial: the account only ever talks to a local development
database.

Override at configure time if your setup differs:

    cmake --preset ninja-debug -DDB2_TEST_USER=someone -DDB2_TEST_PASS=secret

### the tables the database tests need

`create_test_user.sh` creates all of them. The individual definitions, for
reference or for a database set up by hand:

    db/db2/create_table_crud.sql     # crud_test  - the round trip test
    db/db2/create_table_types.sql    # types_test - one column per sql type
    db/db2/create_table_perf.sql     # perf_test  - the batch and benchmark tests

with PostgreSQL counterparts under `db/psql/`.

### benchmarks

`test_bench.cpp` holds two throughput benchmarks. They are tagged
`[.benchmark]`, which means Catch2 does not run them unless they are asked for
by tag - they move a million rows with the default settings, and `ctest` should
not pay for that.

    ./build/release/test_crud "[.benchmark]" -s

`-s` matters: the timings are reported through `WARN`, which is only printed
when successful assertions are shown.

Three environment variables tune them:

| variable | default | meaning |
| --- | --- | --- |
| `DBGEN4_BUFFER_SIZE` | 4000 | rows per execute, and rows per fetch |
| `DBGEN4_ITERATIONS` | 250 | how many executes the insert performs |
| `DBGEN4_COMMIT_EVERY` | 1 | executes between commits |

Anything unusable - unset, empty, zero, not a number - falls back to the
default rather than failing the run.

The **insert** benchmark writes `DBGEN4_BUFFER_SIZE` rows per execute,
`DBGEN4_ITERATIONS` times (a million rows by default), and reports the total
alongside the time spent filling the buffer, executing, and committing. The
**select** benchmark then reads the whole table back through a buffer of the
same size, fetching until the result set is exhausted, and checks that every
row arrived exactly once in key order.

`DBGEN4_COMMIT_EVERY` defaults to a commit after every block. A single
transaction over the whole run does not fit: a million rows of `perf_test`
needs roughly 270 MB of transaction log, and DB2 rolls the transaction back
with SQL0964 once the log fills. Committing per block is what bulk loading does
in practice, and it bounds how much work a failure discards. Raise it to
measure what a less frequent commit buys - the timing report breaks the commit
out separately.

The development database was configured for this with

    db2 UPDATE DB CFG FOR test USING LOGFILSIZ 16384 LOGPRIMARY 10 LOGSECOND 6

which is 16 files of 64 MB, about 1 GB in all (it was 100 MB). The change needs
the database deactivated and reactivated before it takes effect.

A smaller run, for a quick check:

    DBGEN4_BUFFER_SIZE=100 DBGEN4_ITERATIONS=3 ./build/debug/test_crud "[.benchmark]" -s

Timings are reported, never asserted on: a wall clock threshold would fail on a
loaded machine or a slow link without saying anything about whether the code is
correct. The row counts and the ordering *are* asserted.

## usage

The generator is built once per available backend, as `dbgen4-db2` and
`dbgen4-psql`. Each takes the same command line:

    dbgen4-psql -t psql -n mydb -u dbuser -p secret --host localhost --port 5432 \
      -o ./generated schema.yaml

| option | short | required | default | meaning |
| --- | --- | --- | --- | --- |
| `--db-type` | `-t` | no | `sql` | target RDBMS: `sql`, `mariadb`, `psql`, `db2` |
| `--host` | | no | `localhost` | database host |
| `--port` | | no | backend's default (e.g. 5432 for psql) | database port |
| `--db-name` | `-n` | yes | | database name |
| `--username` | `-u` | yes | | database user |
| `--password` | `-p` | yes* | | database user password |
| `--max-field-len` | `-l` | no | 4096 | fallback width for columns with no declared length (text, json, bytea, ...); override per column with `field-len` in the YAML file |
| `--out-folder` | `-o` | no | `./` | output folder for generated files |
| `--parallel` | `-j` | no | `1` | number of worker threads used to process the YAML files, see below |
| `--verbose` | `-v` | no | off | verbose output |
| `files` (positional) | | yes | | one or more YAML files to process; each must exist |

\* `--password` can be omitted from the command line if the `DBGEN4_PASSWORD`
environment variable is set instead:

    DBGEN4_PASSWORD=secret dbgen4-psql -n mydb -u dbuser --host localhost schema.yaml

An explicit `-p/--password` on the command line takes precedence over
`DBGEN4_PASSWORD` when both are present. Preferring the environment variable
keeps the password out of the shell history and process listing.

### parallel processing (`-j`/`--parallel`)

By default (`-j1`) the YAML files are processed one after another. Passing
`-j<n>` or `--parallel=<n>` processes them using `n` worker threads instead:

    dbgen4-psql -n mydb -u dbuser -j4 -o ./generated a.yaml b.yaml c.yaml d.yaml e.yaml

If `n` is omitted - a bare `-j` or `--parallel` - the number of threads
available on the machine is used instead (capped at the number of files, so
five files never start more than five workers).

Each worker opens its own database connection and works through the shared
list of files: whenever a worker finishes a file, it picks up the next
not-yet-processed one, until none are left. This means the work is balanced
across workers even when files take different amounts of time to process,
without any file being handled by more than one worker.

At `info` log level, the run reports:

- when each worker starts,
- for every file a worker processes: the file name, how long it took, and the
  names of the generated `.hpp`/`.cpp` files,
- when a worker finishes: how many files it processed and its total time,
- a final summary once every worker is done: number of worker threads used,
  total wall clock time, number of YAML files processed, and the average time
  per file.

If any file fails, the other workers keep draining the queue rather than
stopping early; the program's exit code reflects the first error encountered.

**Command line quirk:** a *bare* `-j`/`--parallel` (no number) placed
*before* the file list is parsed as if the first filename were its value,
and fails with a clear "not in range" error rather than silently doing the
wrong thing. Giving an explicit count (`-j4`, `-j 4`, `--parallel=4`) never
has this problem, in any position. To auto-detect the thread count, either
put the bare `-j`/`--parallel` *after* the file list, or just give an
explicit count.

Running with no arguments prints the help text.

## build dependencies

### C++ libraries

fmt, spdlog, nlohmann_json, yaml-cpp, valijson and Catch2 are fetched and built
by [CPM.cmake](cmake/CPM.cmake), pinned in [CMakeLists.txt](CMakeLists.txt). No
system packages needed, and no `find_package` that silently picks up whatever
the distribution ships.

Export a source cache before the first configure, otherwise every build
directory (`build/debug`, `build/release`, `build/profile`) downloads its own copy:

   export CPM_SOURCE_CACHE=$HOME/.cache/CPM

Worth putting in `~/.bashrc`.

### toolchain

   sudo dnf install cmake ninja-build gcc-c++

CMake 3.25 or newer (presets need it). Built with gcc 16 and C++23.

### sanitizer runtime

   sudo dnf install libasan libubsan

On by default in Debug and RelWithDebInfo; `-DENABLE_SANITIZERS=OFF` to skip.

### database backends

Each is optional, but at least one has to be present - the generator is built
once per available backend, as `dbgen4-db2` and `dbgen4-psql`.

   sudo dnf install unixODBC-devel     # DB2 backend, plus the IBM CLI driver
   sudo dnf install libpq-devel        # PostgreSQL backend

## database installations

### mariadb

   sudo dnf install mariadb-server mariadb-client
   sudo mysql_secure_installation
   sudo dnf install unixODBC
   sudo dnf install mariadb-connector-odbc
   <https://mariadb.com/kb/en/mariadb-connector-odbc/>

   start mariadb

   sudo service mariadb start

## Doxygen

   sudo dnf install doxygen mscgen dia
