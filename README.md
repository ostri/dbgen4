# dbgen4x

database access layer generator

## build

   cmake --preset ninja-debug
   cmake --build --preset debug -j8
   ctest --test-dir build/debug -L unit --output-on-failure

Presets: `ninja-debug`, `ninja-release`, `ninja-profile`. `ninja-release` and
`ninja-profile` both build with link time optimization
(`CMAKE_INTERPROCEDURAL_OPTIMIZATION`) and native code generation
(`-march=native`) - both assume the binary runs on the machine that built it,
not one it ships to.

`ninja-profile` is for profiling the runtime and the generated code with
Linux `perf`: same optimization as Release (`-O3 -DNDEBUG` plus LTO),
sanitizers off (their overhead would distort the profile), plus `-g` for
DWARF debug info so `perf report`/`perf script` resolve readable function
names and line numbers. `-fno-omit-frame-pointer` is already on for every
build (see `dbgen4_warnings` in CMakeLists.txt), which `perf`'s default
frame-pointer based unwinding needs.

    cmake --preset ninja-profile
    cmake --build --preset profile -j8
    perf record -g --call-graph=fp -- ./build/profile/dbgen4-psql ...
    perf report

The presets do not pin a compiler. The project has to build under both gcc and
clang, so pick one through the environment rather than through a preset:

   cmake --preset ninja-debug                 # system default (gcc)
   CXX=clang++ cmake -S . -B build/clang -G Ninja -DCMAKE_BUILD_TYPE=Debug

A build tree remembers its compiler once configured, so switching means a fresh
directory, not a reconfigure of the old one.

## testing

All test sources live in [src/test/](src/test/). They build into these
binaries:

| binary | source | needs a database |
| --- | --- | --- |
| `unit_tests` | `main.cpp`, `parser_test.cpp` | no |
| `test_t1` | `test_t1.cpp` | at build time only (db2, to generate code from `yaml/t1.yaml`) - the test binary itself never connects |
| `test_crud_db2` | `test_crud.cpp`, `test_types.cpp`, `test_perf.cpp`, `test_bench.cpp`, `test_crud_batch.cpp`, `test_crud_batch_db2.cpp`, `test_parallel.cpp` | yes (db2) |
| `test_crud_psql` | `test_crud.cpp`, `test_types.cpp`, `test_perf.cpp`, `test_bench.cpp`, `test_crud_batch.cpp`, `test_crud_batch_psql.cpp`, `test_parallel.cpp` | yes (psql) |

`test_crud.cpp`, `test_types.cpp`, `test_perf.cpp`, `test_bench.cpp`,
`test_crud_batch.cpp` and `test_parallel.cpp` are shared verbatim between the
two `test_crud_*` binaries - each is compiled once per backend against that
backend's generated `crud.hpp`/`crud.cpp` and its own `test_db.hpp` (under
`src/test/db2/` or `src/test/psql/`). Only where the backends genuinely
disagree does a test get its own file per backend (see
`test_crud_batch_db2.cpp`/`test_crud_batch_psql.cpp` below).

### what each test file covers

| file | what it tests |
| --- | --- |
| `parser_test.cpp` | the yaml parser directly (no database): an empty document yields no statements, a simple SELECT parses into the expected structure |
| `test_t1.cpp` | a generated buffer's own plumbing: self-description, default and explicit dimensions, row isolation, null handling, the row-status array, `dump()`; one case is `rtl::date`'s comparison operators |
| `test_crud.cpp` | one full crud round trip (insert, read back and compare, update, read back and compare, delete, confirm gone) through generated code against a live database |
| `test_types.cpp` | every supported sql type survives a round trip through the generated buffers, and a null in every nullable column reads back as null |
| `test_crud_batch.cpp` | the batch behavior both backends share: ten rows go in on one execute and come back three at a time, and sizing the buffer before prepare (rather than after) still works |
| `test_crud_batch_db2.cpp` | db2-only batch behavior: partial success (nine rows land, one is refused, reported via `row_status()`), and `rtl::odbc_error`'s fields directly |
| `test_crud_batch_psql.cpp` | psql-only batch behavior: one bad row fails the whole batch (libpq pipeline mode is all-or-nothing), and `rtl::psql_error`'s fields directly |
| `test_perf.cpp` | buffer-to-table size relations: a hundred-row batch on one execute, a fetch buffer larger than the table, an exact divisor of it, a non-divisor (short last fetch), and that an update touches only the rows it names |
| `test_bench.cpp` | throughput benchmarks, tagged `[.benchmark]` so an ordinary `ctest` run skips them: batched insert (rows/s, with the fill/execute/commit split out) and buffered select (rows/s), both parametrised from the environment - see `DBGEN4_BUFFER_SIZE`/`DBGEN4_ITERATIONS`/`DBGEN4_COMMIT_EVERY` further down |
| `test_parallel.cpp` | the generator's own `-j/--parallel` option: runs `dbgen4-<backend>` as a subprocess with `-j2` against five yaml files and checks it exits 0 with all ten `.hpp`/`.cpp` files produced |
| `log_test.cpp` | the older `log` facade; not part of any build target (linking it alongside `rtl_logger` breaks async logging - see the comment in CMakeLists.txt) |

`test_db.hpp` (one copy per backend) holds the connection fixture the
database tests share.

### running

    ctest --test-dir build/debug                       # everything
    ctest --test-dir build/debug --output-on-failure   # with output when one fails
    ctest --test-dir build/debug -j8                   # in parallel

By label:

    ctest --test-dir build/debug -L unit      # parser, no database
    ctest --test-dir build/debug -L smoke     # generated buffers, no database
    ctest --test-dir build/debug -L crud      # round trips against a live database
    ctest --test-dir build/debug -L live-db   # same set, by what they need

One test by name (a regular expression):

    ctest --test-dir build/debug -R "batch of a hundred" --output-on-failure
    ctest --test-dir build/debug -N            # list without running

### running a test binary directly

ctest passes the connection details in the environment. Running a binary by
hand means supplying them, otherwise the built in defaults apply
(`localhost:50000`, database `dbgen4`, user `dbgen4`, password `dbgen4`):

    DBGEN4_TEST_HOST=localhost DBGEN4_TEST_PORT=50000 DBGEN4_TEST_DB=dbgen4 \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
      ./build/debug/test_crud_db2 "crud round trip through the generated buffers"

Useful Catch2 flags: `--list-tests` for the names, `-s` to show successful
assertions as well (this is how the benchmarks print their timings), `-?` for
the rest. Note that a test name containing a comma has to be selected by tag
rather than by name - Catch2 reads the comma as a filter separator.

### the database account the tests use

Both backends connect as a dedicated `dbgen4` account (database and password
both `dbgen4`) rather than as whoever is logged in. DB2 authenticates against
the operating system, so a database user there is also an OS user; putting a
developer's own login password into a CMake cache variable would make the
build personal to one machine. PostgreSQL needs no OS account, just the role.

Two scripts per backend set this up, run in order:

    ./db/db2/create_database.sh    # OS account, DB2 database, DBADM grant
    ./db/db2/create_tables.sh      # crud_test, types_test, perf_test1/2/3, test

    ./db/psql/create_database.sh   # role and database
    ./db/psql/create_tables.sh     # crud_test, types_test, perf_test1/2/3, test

`create_database.sh` is a one-off, run as an account with admin rights on the
server (DB2 admin / sudo for db2, PostgreSQL superuser for psql - point it at
the actual server through `PGHOST` etc. for psql). `create_tables.sh` connects
as `dbgen4` itself and is safe to re-run any time the tables need to be reset;
everything it creates, fills and truncates belongs to `dbgen4` alone - no
other schema is touched.

Override the DB2 side at configure time if your setup differs:

    cmake --preset ninja-debug -DDB2_TEST_USER=someone -DDB2_TEST_PASS=secret

### the tables the database tests need

`create_tables.sh` creates all of them, for either backend. The individual
definitions, for reference or for a database set up by hand:

    db/db2/create_table_crud.sql     # crud_test      - the round trip test
    db/db2/create_table_types.sql    # types_test     - one column per sql type
    db/db2/create_table_perf.sql     # perf_test1/2/3 - the batch and benchmark tests
    db/db2/create_table_test.sql     # test           - the wide table yaml/t1.yaml describes

with PostgreSQL counterparts under `db/psql/`. Column names, order and types
are kept aligned between the two backends wherever both support the type
natively, so the same test logic applies to either.

### benchmarks

`test_bench.cpp` holds two throughput benchmarks, shared between both
backends (see "what each test file covers" above). They are tagged
`[.benchmark]`, which means Catch2 does not run them unless they are asked for
by tag - `ctest` should not pay for moving rows this size.

DB2 and PostgreSQL are two separate servers, not necessarily on the same
machine or port, so each binary needs its own `DBGEN4_TEST_*` connection
details (see "the database account the tests use" / `PSQL_TEST_*` above for
what each defaults to):

    DBGEN4_TEST_HOST=localhost DBGEN4_TEST_PORT=50000 DBGEN4_TEST_DB=dbgen4 \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
      ./build/release/test_crud_db2 "[.benchmark]" -s

    DBGEN4_TEST_HOST=postgres.lan DBGEN4_TEST_PORT=5432 DBGEN4_TEST_DB=dbgen4 \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
      ./build/release/test_crud_psql "[.benchmark]" -s

`-s` matters: the row counts are checked with `CHECK`, and Catch2 only prints
passing assertions when `-s` is given.

The actual timings ("... rows/s") go through `rtl::logger`, not Catch2, and
`test_db.hpp`'s fixture drops the console level to `warn` for every test to
keep ordinary output quiet - the benchmarks raise the *logger*'s level back to
`debug`, but the console sink's own level is set separately, from
`config/log.debug.conf`/`config/log.release.conf`, and stays at `warn`
regardless. To actually see the timings, point `LOG_CONFIG` at a copy of that
file with `console_level` raised to `info` (for the "rows/s" summary lines) or
`debug` (to also see the per-commit/per-fetch breakdown - release builds compile
`debug()`/`trace()` calls out entirely, so `debug` only adds anything in a debug
build):

    cp config/log.release.conf /tmp/log.bench.conf
    sed -i 's/"console_level": "warn"/"console_level": "info"/' /tmp/log.bench.conf

    LOG_CONFIG=/tmp/log.bench.conf \
    DBGEN4_TEST_HOST=postgres.lan DBGEN4_TEST_PORT=5432 DBGEN4_TEST_DB=dbgen4 \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
    DBGEN4_BUFFER_SIZE=100 DBGEN4_ITERATIONS=3 \
      ./build/release/test_crud_psql "[.benchmark]" -s

Three environment variables tune them:

| variable | default | meaning |
| --- | --- | --- |
| `DBGEN4_BUFFER_SIZE` | 4000 | rows per execute, and rows per fetch |
| `DBGEN4_ITERATIONS` | 3 | how many executes the insert performs |
| `DBGEN4_COMMIT_EVERY` | 1 | executes between commits |

Anything unusable - unset, empty, zero, not a number - falls back to the
default rather than failing the run.

The **insert** benchmark writes `DBGEN4_BUFFER_SIZE` rows per execute,
`DBGEN4_ITERATIONS` times (12000 rows by default), and reports the total
alongside the time spent filling the buffer, executing, and committing. The
**select** benchmark then reads the whole table back through a buffer of the
same size, fetching until the result set is exhausted, and checks that every
row arrived exactly once in key order.

Every row also carries a full `tran` - 5120 random printable characters - so
the default run is around 60 MB rather than the roughly 3 MB
`id`/`name`/`created` alone would be. Raising `DBGEN4_ITERATIONS` to 250, the
previous default, moves a million rows and around 5 GB. See
`db/db2/create_table_perf.sql` for why the column is there and why its content
is random rather than repeated.

`DBGEN4_COMMIT_EVERY` defaults to a commit after every block. A single
transaction over a large enough run does not fit - at 250 iterations it would
need the full ~5 GB - and DB2 rolls the transaction back with SQL0964 once the
log fills. Committing per block is what bulk loading does in practice, and it
bounds how much work a failure discards. Raise it to measure what a less
frequent commit buys - the timing report breaks the commit out separately -
but each commit still has to fit in the log, so raising `DBGEN4_COMMIT_EVERY`
risks SQL0964 on its own once `DBGEN4_BUFFER_SIZE` x `DBGEN4_COMMIT_EVERY`
rows (at ~5.4 KB each, with tran) approach the log's ~1 GB capacity.

The development database was configured for this with

    db2 UPDATE DB CFG FOR dbgen4 USING LOGFILSIZ 16384 LOGPRIMARY 10 LOGSECOND 6

which is 16 files of 64 MB, about 1 GB in all (it was 100 MB). The change needs
the database deactivated and reactivated before it takes effect.

A smaller run, for a quick check:

    DBGEN4_TEST_HOST=localhost DBGEN4_TEST_PORT=50000 DBGEN4_TEST_DB=dbgen4 \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
    DBGEN4_BUFFER_SIZE=100 DBGEN4_ITERATIONS=3 \
      ./build/debug/test_crud_db2 "[.benchmark]" -s

    DBGEN4_TEST_HOST=postgres.lan DBGEN4_TEST_PORT=5432 DBGEN4_TEST_DB=dbgen4 \
    DBGEN4_TEST_USER=dbgen4 DBGEN4_TEST_PASS=dbgen4 \
    DBGEN4_BUFFER_SIZE=100 DBGEN4_ITERATIONS=3 \
      ./build/debug/test_crud_psql "[.benchmark]" -s

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

## integrating into another CMake project

dbgen4 is meant to be pulled in as a subdirectory of your own build, not
installed system-wide or copied in - one checkout, referenced by whatever
projects need it. `CMakeLists.txt` is written to support this directly: it
detects whether it is the top-level project and only turns on its own tests,
sanitizers and Doxygen docs when it is.

### getting the source in

Any mechanism that ends in `add_subdirectory(<path-to-dbgen4>)` works. Using
[CPM.cmake](cmake/CPM.cmake) - the same tool dbgen4 itself uses for its own
dependencies - keeps the version pinned to a tag and avoids a git submodule:

    include(cmake/CPM.cmake)   # or your own copy of it
    CPMAddPackage(
        NAME dbgen4
        GITHUB_REPOSITORY <org>/dbgen4
        GIT_TAG v0.1.3
    )

Plain `FetchContent` or a git submodule work identically, since all that
matters is that dbgen4's `CMakeLists.txt` runs as a subdirectory of yours.

### what you get

| target | what it is | when you need it |
| --- | --- | --- |
| `dbgen4::db2_rtl` | static library: the neutral `rtl` vocabulary plus the DB2 backend | link this if your generated code targets db2 |
| `dbgen4::psql_rtl` | static library: the neutral `rtl` vocabulary plus the psql backend | link this if your generated code targets psql |
| `dbgen4-db2` | the generator executable, built against DB2 | invoke at build time to turn a `.yaml` schema into `.hpp`/`.cpp` |
| `dbgen4-psql` | the generator executable, built against psql | same, for psql |

Each pair only exists if the matching backend was found on the machine doing
the configuring (`DB2_FOUND`/`LIBPQ_FOUND`) - guard your own use of them with
`if(TARGET dbgen4-psql)` etc. if you need the configure step to succeed
either way.

The executables are plain `${PROJECT_NAME}-${backend}` targets rather than
`dbgen4::` aliases: CMake only allows `ALIAS` on an executable from 3.26
onward, and dbgen4 targets 3.25. The names are already unique.

### wiring it into your build

    add_subdirectory(third_party/dbgen4)

    # 1. generate code from your schema at build time
    set(GEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${GEN_OUT})
    add_custom_command(
        OUTPUT ${GEN_OUT}/myschema.hpp ${GEN_OUT}/myschema.cpp
        COMMAND $<TARGET_FILE:dbgen4-psql>
                -o ${GEN_OUT} -t psql -n mydb -u dbuser -p secret
                ${CMAKE_CURRENT_SOURCE_DIR}/myschema.yaml
        DEPENDS dbgen4-psql ${CMAKE_CURRENT_SOURCE_DIR}/myschema.yaml
    )
    add_custom_target(generate_myschema DEPENDS ${GEN_OUT}/myschema.hpp ${GEN_OUT}/myschema.cpp)

    # 2. compile and link the generated code
    add_executable(myapp main.cpp ${GEN_OUT}/myschema.cpp)
    add_dependencies(myapp generate_myschema)
    target_include_directories(myapp PRIVATE ${GEN_OUT})
    target_link_libraries(myapp PRIVATE dbgen4::psql_rtl)

### what stays out of your build

With dbgen4 as a subdirectory, `BUILD_TESTING` and `ENABLE_SANITIZERS`
default to `OFF` regardless of your own project's settings for them -
`PROJECT_IS_TOP_LEVEL` is what decides this, not the option's own default.
dbgen4's `tidy` and Doxygen `docs` targets are still created (nothing calls
them unless you do), but none of dbgen4's own test binaries, Catch2, or the
`-fsanitize=...` compile options it uses for its own Debug builds leak into
your targets. Set `-DBUILD_TESTING=ON`/`-DENABLE_SANITIZERS=ON` explicitly
before the `add_subdirectory()` call if you do want dbgen4's own test suite
to build and run as part of your tree.

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
