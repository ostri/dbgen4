# dbgen4 user guide

How to use dbgen4-generated database access code - what dbgen4 is for, the public API a generated
statement exposes, one example per CRUD operation, the YAML schema format, batch (vectorized)
execution, and the asynchronous `rtl::async_db` facade.

This document covers *using* dbgen4: running the generator itself, and then writing code against
what it produces. For building/testing dbgen4 itself from source, see the top level
[README.md](../README.md).

## what dbgen4 is for

dbgen4 is a database access layer **generator**. Given a `.yaml` file that lists named SQL
statements (optionally one dialect per statement - `db2`, `psql`, `mariadb`, or a dialect-neutral
`sql`), it connects to a live database, asks it to describe each statement's parameters and result
columns (`PREPARE`/`DESCRIBE`), and emits one `.hpp`/`.cpp` pair of C++ classes per statement - one
class to hold the parameters, one to hold the result rows, one to run the statement.

The generated classes present **the same public interface regardless of backend**: the same
`prepare()`/`execute()`/`fetch()`/`get_param()`/`get_result()` methods, the same generated
`set_<col>()`/`<col>()` accessors, the same batch semantics. A caller who writes code against
`dbx::my_stmt::p` and `dbx::my_stmt::qry` does not need to know or care whether the schema was
generated with `dbgen4-db2` or `dbgen4-psql` - only the two backend-specific generator binaries and
the two `dbx::rtl` link targets (`dbgen4::db2_rtl`, `dbgen4::psql_rtl`) differ, chosen once at
build time.

This buys three things a caller would otherwise hand-write, and drift, one statement at a time:

- **Type safety without hand-written boilerplate.** Every column's C++ type, buffer width and null
  handling is derived from what the database itself reports for that statement, not retyped by
  hand and left to fall out of sync with the schema.
- **One SQL dialect per backend, one call site.** A statement can carry different SQL text for DB2
  (`?` placeholders, `VARCHAR`/`CLOB` types) and PostgreSQL (`$1, $2, ...` placeholders,
  `TEXT`/`BYTEA`), or a single dialect-neutral `sql:` block when the syntax happens to be
  identical either way - the calling C++ code never changes.
- **Compile-time validation of the schema.** Because the generator connects to a real database and
  prepares each statement before emitting code, a typo in a column name or an incompatible type
  fails the *build*, not a production run three weeks later.

## running the generator

The generator is built once per available backend, as two separate executables:
`dbgen4-db2` and `dbgen4-psql`. Each takes the exact same command line shape - only which one you
invoke decides which dialect key (`db2:`/`psql:`) each statement's SQL is validated and generated
against, and which of `dbx::db2_rtl`/`dbx::psql_rtl` the output needs linked in:

```sh
dbgen4-psql -t psql -n mydb -u dbuser -p secret --host localhost --port 5432 \
  -o ./generated schema.yaml
```

Both connect to a live database while generating: the whole point is to `PREPARE`/`DESCRIBE` every
statement against the real schema before emitting a single line of C++ (see
[what dbgen4 is for](#what-dbgen4-is-for)), so the connection options below (`--db-name`,
`--username`, `--password`, `--host`, `--port`) name the database the *statements themselves* run
against - i.e. wherever your actual tables live, not some separate metadata store.

### options

- **`-t, --db-type <sql|mariadb|psql|db2>`** (optional, default `sql`) - which dialect key
  (`sql:`/`mariadb:`/`psql:`/`db2:`) a statement's SQL is resolved from, see
  [statement level keys](#statement-level-keys). An unrecognized value falls back to `sql` rather
  than failing the parse - state it explicitly to avoid surprises. In practice this should always
  match the backend the executable itself was built for (`-t psql` with `dbgen4-psql`,
  `-t db2` with `dbgen4-db2`); the option exists at the SQL-text-resolution level independently of
  that, but the two are meant to travel together.
- **`--host <name>`** (optional, default `localhost`) - the database server to connect to while
  generating.
- **`--port <n>`** (optional, default: the linked-in backend's own default, e.g. `5432` for psql) -
  the port on `--host` to connect to.
- **`-n, --db-name <name>`** (required) - the database to connect to.
- **`-u, --username <name>`** (required) - the database user to connect as.
- **`-p, --password <secret>`** (required) - the database user's password. Can be supplied instead
  through the `DBGEN4_PASSWORD` environment variable, which keeps a real password out of shell
  history and process listings (`ps` shows command-line arguments to anyone on the same machine); an
  explicit `-p`/`--password` on the command line takes precedence when both are present:

  ```sh
  DBGEN4_PASSWORD=secret dbgen4-psql -n mydb -u dbuser --host localhost schema.yaml
  ```

- **`-l, --max-field-len <n>`** (optional, default `4096`, must be a positive number) - the
  fallback width, in bytes, assumed for a string/binary column the database reports no declared
  length for (PostgreSQL `text`/`json`/`bytea`, MariaDB `TEXT`/`BLOB` all report `0`). Override it
  for one column at a time with that statement's own `field-len` yaml key instead, see
  [statement level keys](#statement-level-keys) - this command-line option is only the fallback for
  whatever `field-len` does not name.
- **`-o, --out-folder <path>`** (optional, default `./`) - where the generated `.hpp`/`.cpp` files
  are written.
- **`-v, --verbose`** (flag, default off) - raises the generator's own log level (`trace` in a debug
  build, `info` in a release build; without it, `info`/`warn` respectively) - turn this on to see
  per-statement progress and the resolved parameter dump while diagnosing a generation failure.
- **`-j, --parallel [n]`** (optional; absent means sequential, i.e. `-j1`) - process the yaml files
  using `n` worker threads, each opening its own database connection and pulling the next
  not-yet-processed file off a shared queue until none are left - so work stays balanced across
  workers even when files take very different amounts of time, and no file is ever handled by more
  than one worker. A *bare* `-j`/`--parallel` (no number given) uses the number of threads available
  on the machine instead. If any file fails, the other workers keep draining the queue rather than
  stopping early; the program's own exit code reflects the first error encountered.

  **Command line quirk:** a bare `-j`/`--parallel` placed *before* the file list is parsed as if the
  first filename were its value, and fails with a clear "not in range" error rather than silently
  doing the wrong thing. An explicit count (`-j4`, `-j 4`, `--parallel=4`) never has this problem, in
  any position - to auto-detect the thread count, either put the bare `-j`/`--parallel` *after* the
  file list, or just give an explicit count:

  ```sh
  dbgen4-psql -n mydb -u dbuser -j4 -o ./generated a.yaml b.yaml c.yaml d.yaml e.yaml
  ```

- **`files`** (positional, one or more, required) - the YAML schema files to process; each must
  already exist (checked before generation starts).

Running with no arguments at all prints the help text (equivalent to `--help`), rather than failing
outright with a wall of missing-required-option errors.

At `info` log level (i.e. always with `-j` and more than one worker, or with `-v`), a run also
reports: when each worker starts, for every file a worker processes - its name, how long it took,
and the generated `.hpp`/`.cpp` file names - and, once every worker is done, a final summary: number
of worker threads used, total wall clock time, number of files processed, and the average time per
file.

### wiring the generator into a CMake build

Rather than running the generator by hand, a consuming project typically wires it into its own
build as a custom command, so generated code is refreshed automatically whenever the `.yaml` schema
changes:

```cmake
add_subdirectory(third_party/dbgen4)

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

add_executable(myapp main.cpp ${GEN_OUT}/myschema.cpp)
add_dependencies(myapp generate_myschema)
target_include_directories(myapp PRIVATE ${GEN_OUT})
target_link_libraries(myapp PRIVATE dbgen4::psql_rtl)
```

See the top level [README.md](../README.md#integrating-into-another-cmake-project) for the full
detail on pulling dbgen4 in as a subdirectory (via [CPM.cmake](../cmake/CPM.cmake) or plain
`add_subdirectory()`), what each of `dbgen4::db2_rtl`/`dbgen4::psql_rtl`/`dbgen4-db2`/`dbgen4-psql`
is and when you need it, and what stays out of your own build (dbgen4's own tests/sanitizers default
off when it is used as a subdirectory).

## public API

Everything generated for one schema file lands inside `namespace dbx::<schema filename>` (e.g.
`crud.yaml` -> `dbx::crud`), and inside that, one nested namespace per statement id (e.g. statement
id `ins` -> `dbx::crud::s_ins`). This is what lets two different `.yaml` files reuse the same
statement id (`ins`, `sel`, ...) without colliding when both headers are included together - each
lives in its own schema namespace.

Each statement namespace (`dbx::<schema>::s_<id>`) holds up to three classes, depending on whether
the statement takes parameters and/or returns rows:

- **`p`** (present when the statement has parameters) - the parameter buffer, deriving from
  `rtl::parameter_root`.
- **`r`** (present when the statement returns rows, i.e. a `select`) - the result buffer, deriving
  from `rtl::result_root`.
- **`qry`** (always present, aliased `stmt` for source compatibility with older code - both names
  refer to the same class, new code may use either) - the statement itself, deriving from
  `rtl::query<p or rtl::no_params, r or rtl::no_results>`.

A statement with no parameters uses `rtl::no_params` in place of `p`; a statement with no result
rows (insert/update/delete) uses `rtl::no_results` in place of `r`. Both are empty placeholder
types - `get_param()`/`get_result()` simply do not exist (a compile error, not a null pointer
return) on a `qry` instantiated with them, so calling the wrong one for a given statement is caught
by the compiler, not discovered at runtime.

### `qry`/`stmt` - the generated statement class

```cpp
namespace dbx::crud::s_ins
{
  class qry : public rtl::query<s_ins::p, rtl::no_results>
  {
  public:
    static consteval cstr_t sql() noexcept;      // the statement text, as sent to the server
    explicit qry(const rtl::database* db);
    using rtl::query<s_ins::p, rtl::no_results>::query;
  };
  using stmt = qry;
}
```

`qry` derives from `rtl::query<params, results>`, whose public members are the actual,
backend-neutral surface every caller programs against:

| method | semantics |
| --- | --- |
| `explicit query(const database* db, std::string_view sql = qry::sql())` | Binds the statement to an already-connected `database*` (not owned - must outlive the query). The generated `qry`'s own constructor already passes `sql()`, so callers just write `s_ins::qry ins(&db);`. |
| `std::expected<void, backend_error> prepare()` | Parses/plans the statement on the server and binds every parameter/result buffer's storage to the driver. Must run *before* `execute()`, and again after any `set_buffer_size()` call (see [batch execution](#batch-vectorized-execution)) - `check_layout()` refuses to run against buffers that moved since the last `prepare()`. |
| `std::expected<void, backend_error> execute()` | Runs the statement with whatever is currently in the parameter buffer. For a statement with a result set, this is what a subsequent `fetch()` reads from. |
| `std::expected<bool, backend_error> fetch()` | Pulls the next window of rows into the result buffer (only exists when the statement has `results`). Returns `false` once the result set is exhausted; repeat calls to walk a result set larger than the buffer's own `buffer_size()`. |
| `int64_t affected_rows() const noexcept` | Rows the last `execute()` inserted/updated/deleted; `-1` when the backend did not report a count (e.g. a `select`). |
| `bool is_prepared() const noexcept` | Whether `prepare()` has succeeded and not since been invalidated. |
| `std::shared_ptr<params> get_param() noexcept` | The parameter buffer to fill in before `execute()`. Only exists (compiles) when the statement has parameters. |
| `std::shared_ptr<results> get_result_buffer() noexcept` | The result buffer, for callers that want to `set_buffer_size()` it before `prepare()`. |
| `std::shared_ptr<const results> get_result() const noexcept` | The result buffer, read-only, for reading rows back after `fetch()`. |

`prepare()`/`execute()`/`fetch()` all return `std::expected<..., backend_error>` (`rtl::odbc_error`
for DB2, `rtl::psql_error` for PostgreSQL) - check with `if (auto r = ...; ! r) { use r.error() }`
before trusting the call succeeded, the same pattern on both backends.

### `p` - the parameter buffer

One column-wise array per parameter, sized to hold `par-buf-size` rows (see
[statement level keys](#statement-level-keys); overridable at run time with `set_buffer_size()`,
see [batch execution](#batch-vectorized-execution)). For each parameter column `<col>` the
generator emits:

| member | semantics |
| --- | --- |
| `void set_<col>(const T& v, size_t row = 0)` | Writes column `<col>`'s value for row `row` (default: row 0, the common single-row case). |
| `T <col>(size_t row = 0) const` | Reads back column `<col>`'s value for row `row`. |
| `void set_<col>_null(size_t row = 0)` | Marks column `<col>` as SQL `NULL` for row `row`. |
| `bool is_<col>_null(size_t row = 0) const` | Whether column `<col>` is `NULL` for row `row`. |
| `size_t <col>_length(size_t row = 0) const` | The value's length in bytes for row `row` (string/binary columns; `0` for a null value). |
| `const row_t& row(size_t row = 0) const` | One row, as a struct of references into the same column-wise storage, one member per column - reads a whole row at once instead of column by column. |
| `const std::vector<row_t>& row_wise() const` | Every row of the buffer, same `row_t` view as above. |
| `void set_buffer_size(size_t rows)` (inherited from `rtl::parameter_root`) | Resizes every column array to hold `rows` rows. Must be called *before* `prepare()` - `prepare()` hands the driver raw pointers into these arrays, and resizing after that point invalidates them (`check_layout()` catches this and refuses to `execute()`). |
| `size_t buffer_size() const noexcept` | How many rows the buffer currently holds. |
| `bool is_batch() const noexcept` (inherited) | `buffer_size() > 1`, i.e. whether this buffer sends more than one row per `execute()`. |
| `std::span<uint16_t> row_status() noexcept` (inherited) | Per-row outcome of the last batch `execute()`, see [batch execution](#batch-vectorized-execution). Empty when the buffer is not a batch. |
| `void reset_all_null() noexcept` | Marks every field of every row `NULL`, useful before filling in only some columns of a batch. |
| `str_t dump(uint8_t offs = 0) const` | A human-readable dump of the buffer's contents, for logging/debugging. |

### `r` - the result buffer

Mirrors `p`, but read-only (result rows come *from* the server, so there is nothing to write back):

| member | semantics |
| --- | --- |
| `T <col>(size_t row = 0) const` | Reads back column `<col>`'s value for row `row`. |
| `bool is_<col>_null(size_t row = 0) const` | Whether column `<col>` is `NULL` for row `row`. |
| `size_t <col>_length(size_t row = 0) const` | The value's length in bytes for row `row` (string/binary columns; `0` for a null value). |
| `const row_t& row(size_t row = 0) const` | One row, as a struct of references into the same column-wise storage, one member per column. |
| `const std::vector<row_t>& row_wise() const` | Every row of the buffer, same `row_t` view as above. |
| `void set_buffer_size(size_t rows)` (inherited from `rtl::result_root`) | Sets the *fetch window* - how many rows one `fetch()` call returns at most. Must be called *before* `prepare()`, same reason as on `p`. A result set larger than the window is read with repeated `fetch()` calls. |
| `size_t buffer_size() const noexcept` | How many rows the buffer/fetch window is currently sized for. |
| `size_t occupied() const noexcept` (inherited) | How many of the buffer's `buffer_size()` slots the last `fetch()` actually filled - short only on the final window of a result set. |
| `str_t dump(uint8_t offs = 0) const` | A human-readable dump of the buffer's contents, for logging/debugging. |

## one example per CRUD operation

Schema (`db/psql/create_table_crud.sql`, with a matching `db/db2/` counterpart kept structurally
aligned the same way the ach project keeps its own two schema files in sync):

```sql
create table crud_test (
  id      integer     not null primary key,
  name    varchar(64) not null,
  created date        not null
);
```

`crud.yaml` (see [yaml/crud.yaml](../yaml/crud.yaml) for the full file this is excerpted from):

```yaml
statements:
  - id: ins
    par-buf-size: 1
    parameter-names: [id, name, created]
    db2:  insert into crud_test (id, name, created) values (?, ?, ?)
    psql: insert into crud_test (id, name, created) values ($1, $2, $3)

  - id: sel
    res-buf-size: 1
    par-buf-size: 1
    parameter-names: [id]
    db2:  select id, name, created from crud_test where id = ?
    psql: select id, name, created from crud_test where id = $1

  - id: upd
    par-buf-size: 1
    parameter-names: [name, created, id]
    db2:  update crud_test set name = ?, created = ? where id = ?
    psql: update crud_test set name = $1, created = $2 where id = $3

  - id: del
    par-buf-size: 1
    parameter-names: [id]
    db2:  delete from crud_test where id = ?
    psql: delete from crud_test where id = $1
```

Generated with, for example:

```sh
dbgen4-psql -t psql -n mydb -u dbuser -p secret -o ./generated yaml/crud.yaml
```

which produces `generated/crud.hpp`/`generated/crud.cpp`, declaring `dbx::crud::s_ins`,
`dbx::crud::s_sel`, `dbx::crud::s_upd`, `dbx::crud::s_del`.

### insert

```cpp
#include "crud.hpp"

dbx::crud::s_ins::stmt ins(&db);           // db: an already-connected rtl::database&
if (auto prepared = ins.prepare(); ! prepared)
{
  log.error("prepare(ins) failed: {}", prepared.error().message);
  return;
}

ins.get_param()->set_id(42);
ins.get_param()->set_name("example");
ins.get_param()->set_created(rtl::date{.year = 2026, .month = 8, .day = 15});

if (auto executed = ins.execute(); ! executed)
{
  log.error("execute(ins) failed: {}", executed.error().message);
  db.rollback();
  return;
}
db.commit();
```

### select

```cpp
dbx::crud::s_sel::stmt sel(&db);
if (auto prepared = sel.prepare(); ! prepared) { /* handle error */ }

sel.get_param()->set_id(42);
if (auto executed = sel.execute(); ! executed) { /* handle error */ }

for (auto got = sel.fetch(); got && *got; got = sel.fetch())
{
  auto rows = sel.get_result();
  for (size_t row = 0; row < rows->occupied(); ++row)
  {
    std::int32_t id      = rows->id(row);
    dbx::cstr_t  name    = rows->name(row);   // view into the row's own buffer
    rtl::date    created = rows->created(row);
    // ... use id/name/created ...
  }
}
db.commit();
```

### update

```cpp
dbx::crud::s_upd::stmt upd(&db);
if (auto prepared = upd.prepare(); ! prepared) { /* handle error */ }

upd.get_param()->set_name("renamed");
upd.get_param()->set_created(rtl::date{.year = 2026, .month = 8, .day = 16});
upd.get_param()->set_id(42); // the where-clause parameter, third placeholder

if (auto executed = upd.execute(); ! executed) { /* handle error */ }
db.commit();
std::int64_t rows_changed = upd.affected_rows();
```

Parameter order follows the SQL text's own placeholder order - `upd`'s yaml lists
`parameter-names: [name, created, id]` because the `set` clause's placeholders come before the
`where`'s in the statement text, not because of any significance to the order otherwise.

### delete

```cpp
dbx::crud::s_del::stmt del(&db);
if (auto prepared = del.prepare(); ! prepared) { /* handle error */ }

del.get_param()->set_id(42);
if (auto executed = del.execute(); ! executed) { /* handle error */ }
db.commit();
```

## the YAML schema file

A schema `.yaml` file declares one document (one `summary`/`description`, usually one logical
group of tables) holding a `statements:` sequence, one entry per named SQL statement dbgen4 should
turn into a `p`/`r`/`qry` triple.

### document level keys

- **`summary`** (optional) - one-line summary of the whole document; carried into the generated
  file's own doc comment.
- **`description`** (optional) - longer, multi-line description (YAML block scalar `|`).
- **`statements`** (required) - the sequence of statement entries described below.

### statement level keys

- **`id`** (required) - unique identifier for this statement within the file; becomes the `s_<id>`
  namespace. Must be unique *per file* - the schema-wrapping `dbx::<schema>` namespace is what then
  lets two *different* files reuse the same id without colliding when both are `#include`d
  together.
- **`summary`** (optional, default `""`) - one-line summary, carried into the generated statement's
  doc comment.
- **`dscr`** (optional, default `""`) - longer description, same purpose as `summary` but
  multi-line.
- **`sql`** / **`db2`** / **`psql`** / **`mariadb`** (at least one required) - `sql` is
  dialect-neutral SQL text, used when no more specific dialect key is present for the backend being
  generated; `db2`/`psql`/`mariadb` are backend-specific SQL text (`?` placeholders for db2, `$1,
  $2, ...` for psql) that override `sql` when generating for that backend. A statement needs at
  least one of these four to resolve to non-empty text for the backend being generated, or the
  generator fails with "SQL is missing" for that statement.
- **`par-buf-size`** (optional, default `1`) - how many parameter rows the generated `p` class
  allocates by default, see [batch execution](#batch-vectorized-execution). A caller can still ask
  for a different size at run time via `set_buffer_size()` without regenerating.
- **`res-buf-size`** (optional, default `1`) - how many result rows the generated `r` class
  allocates by default, i.e. the default fetch window size - same run-time override via
  `set_buffer_size()`.
- **`parameter-names`** (optional, default: the database's own placeholder names) - overrides the
  generated `p` class's column names, in placeholder order (first name <-> `$1`/first `?`, and so
  on).
- **`result-names`** (optional, default: the database's own column names) - overrides the generated
  `r` class's column names, in `select`-list order.
- **`field-len`** (optional, default `{}`) - a map of column name to width in bytes, for
  string/binary columns whose width the database does not report (PostgreSQL `text`/`json`/`bytea`,
  MariaDB `TEXT`/`BLOB` all report width `0`). Falls back to the generator's own `--max-field-len`
  (default 4096) when neither this nor the database gives a width. An entry naming a non-string
  column, or a column not actually in this statement's params/results, is silently ignored -
  harmless to state unconditionally across several statements that only some of them apply to.
- **`before`** / **`after`** (optional) - sub-maps with the same `sql`/`db2`/`psql`/`mariadb`
  dialect keys as the statement itself, see [`before`/`after`](#beforeafter-staging-sql) below.

### `before`/`after`: staging sql

```yaml
- id: ins_from_staging
  before:
    sql: create temporary table stage_ct_in (id integer, name varchar(64))
  after:
    sql: drop table stage_ct_in
  sql: insert into crud_test (id, name) select id, name from stage_ct_in
```

`before`'s SQL runs once, in its own unit of work, immediately before the generator asks the
database to describe this statement's own SQL - needed when that SQL references something (like a
staging table) that does not otherwise exist in the schema. `after`'s SQL always runs afterward, in
the same unit of work, regardless of whether the statement's own SQL validated - so a failure
partway through generation does not leave `before`'s side effects (e.g. an undropped staging table)
behind. Neither is emitted into the generated code itself; both are purely a generation-time
convenience for validating SQL that depends on schema not otherwise present.

## batch (vectorized) execution

A `p`/`r` buffer is not fixed at the size the YAML file declared (`par-buf-size`/`res-buf-size`) -
`set_buffer_size(rows)` resizes it at run time, and a buffer sized to more than one row makes
`execute()` send *every row in one call*, not one row per round trip. Both backends implement this
as true vectorized parameter binding, not a client-side loop:

- **DB2** binds the buffer as an ODBC parameter array (`SQL_ATTR_PARAMSET_SIZE`) - the driver sends
  the whole array to the server in one native batch operation.
- **PostgreSQL** uses libpq pipeline mode (`execute_batch()`) - every row's `PQsendQueryPrepared` is
  queued without waiting for a reply, then one `PQpipelineSync` flushes them together, cutting the
  batch down to one network round trip's worth of latency instead of `rows` of them.

`set_buffer_size()` must be called *before* `prepare()` (it reallocates the column storage;
`prepare()` is what hands the driver pointers into it - resizing afterward is refused by
`check_layout()`, see the [`p` reference](#p---the-parameter-buffer) above).

The two backends genuinely diverge on **partial failure** within one batch:

- DB2 can report exactly which rows landed and which were refused, via `row_status()` (one
  `SQL_PARAM_ERROR`/success entry per row) - nine rows can succeed while a tenth, say, violating a
  constraint, is reported and refused without discarding the other nine.
- PostgreSQL's pipeline mode is all-or-nothing: one bad row aborts the rest of the pipelined batch
  (`25P02`), so a batch either lands whole or fails whole.

A caller that needs identical behavior on both backends should not rely on partial success - either
validate rows before batching them, or treat a batch as one all-or-nothing unit and retry failed
rows individually to learn which one was bad.

### batch insert

```cpp
constexpr std::size_t batch_rows = 1000;

dbx::crud::s_ins::stmt ins(&db);
auto par = ins.get_param();
par->set_buffer_size(batch_rows);   // before prepare()
if (auto prepared = ins.prepare(); ! prepared) { /* handle error */ }

for (std::size_t row = 0; row < batch_rows; ++row)
{
  par->set_id(first_id + static_cast<std::int32_t>(row), row);
  par->set_name(name_for(row), row);
  par->set_created(date_for(row), row);
}

if (auto executed = ins.execute(); ! executed)   // one execute, 1000 rows sent
{
  log.error("batch insert failed: {}", executed.error().message);
  db.rollback();
}
else
{
  db.commit();
  log.info("{} rows inserted", ins.affected_rows());
}
```

### batch update

Same shape - the `where`-clause parameter (here, `id`) varies per row just like every other column,
since each row of the buffer is a fully independent set of bind values for the same statement text:

```cpp
dbx::crud::s_upd::stmt upd(&db);
auto par = upd.get_param();
par->set_buffer_size(batch_rows);
if (auto prepared = upd.prepare(); ! prepared) { /* handle error */ }

for (std::size_t row = 0; row < batch_rows; ++row)
{
  par->set_name(new_name_for(row), row);
  par->set_created(new_date_for(row), row);
  par->set_id(id_for(row), row);
}

if (auto executed = upd.execute(); ! executed) { /* handle error */ }
db.commit();
```

### batch select

Batching a `select` means widening the *fetch window* - how many rows one `fetch()` pulls back at
once - not sending several `select`s in one call:

```cpp
dbx::crud::s_sel_range::stmt sel(&db);
sel.get_result_buffer()->set_buffer_size(batch_rows); // fetch window, before prepare()
if (auto prepared = sel.prepare(); ! prepared) { /* handle error */ }

sel.get_param()->set_id_from(first_id);
sel.get_param()->set_id_to(first_id + static_cast<std::int32_t>(batch_rows) - 1);
if (auto executed = sel.execute(); ! executed) { /* handle error */ }

std::size_t total = 0;
for (auto got = sel.fetch(); got && *got; got = sel.fetch())
{
  auto rows = sel.get_result();
  total += rows->occupied();
  for (std::size_t row = 0; row < rows->occupied(); ++row)
  {
    // ... use rows->id(row), rows->name(row), rows->created(row) ...
  }
}
db.commit();
```

A result set larger than `batch_rows` still comes back correctly - `fetch()` is called again for
each further window, `occupied()` reporting a short count only on the last one.

### batch delete

```cpp
dbx::crud::s_del::stmt del(&db);
auto par = del.get_param();
par->set_buffer_size(batch_rows);
if (auto prepared = del.prepare(); ! prepared) { /* handle error */ }

for (std::size_t row = 0; row < batch_rows; ++row) par->set_id(id_for(row), row);

if (auto executed = del.execute(); ! executed) { /* handle error */ }
db.commit();
```

## asynchronous execution: `rtl::async_db`

`rtl::async_db` (declared in [src/rtl/async_db.hpp](../src/rtl/async_db.hpp)) runs a *sequence of
different statements* - `connect, q1, q2, ..., qn, commit`, each possibly a different
parameter/result type, all on one connection inside one transaction - on a dedicated worker thread,
so the caller's own thread does not sit idle for the length of every round trip. It hands each
statement to the worker and goes back to preparing the next one's parameters, overlapping
application work with database latency.

What it is *not*: a way to run SQL in parallel. One connection carries one statement at a time -
both libpq and ODBC forbid using a connection from two threads at once - so throughput across
several connections still needs several `async_db` instances (or several plain connections), not
one `async_db` doing more work internally. It is also not `co_await`-based: `async_db` predates any
move to C++20 coroutines in this codebase and is implemented with a plain worker thread, a
one-deep job queue, and condition variables - every call below is an ordinary blocking-or-not
function call, never an awaitable one.

Key API:

- **`static std::expected<std::unique_ptr<async_db>, db_error> create(db& database)`** - takes over
  an already-connected `rtl::db&` and starts the worker thread; the caller must not touch `database`
  directly again while the returned object lives. Not a plain constructor: starting a `std::thread`
  can itself fail (`std::system_error`, e.g. a process/system thread limit already reached - a real
  possibility for code that opens several `async_db` instances in a row), and `create()` reports that
  failure the same `std::expected` way every other call on this class does, rather than making
  construction the one place in this API that has to be wrapped in `try`/`catch`. `async_db` is
  neither copyable nor movable (its worker thread captures `this`), hence `unique_ptr` rather than a
  value return.
- **`std::expected<query_handle<P,R>, db_error> prepare<P,R>(std::string_view sql, size_t
  param_rows = 1, size_t result_rows = 1)`** - registers and prepares a statement synchronously (a
  statement that fails to prepare is a programming fault, worth learning about immediately rather
  than three `exec()`s later). `param_rows`/`result_rows` set the buffer sizes - the same batch
  semantics as `set_buffer_size()` above, just supplied here because sizing has to happen before
  `prepare()` and this is the only reachable moment for it. Returns a `query_handle<P,R>`, cheap to
  copy, that names the statement in later calls.
- **`std::expected<exec_status, db_error> exec(const query_handle<P,R>& h)`** - hands a statement to
  the worker, blocking only if the one-deep queue is not free yet (a previous job is still queued or
  running) - the same wait `submit()` always did silently, just with a return value this time. Works
  for both `no_results` and row-returning handles, unlike `submit()`. Returns
  `exec_status::finished` once the job is queued (which also means the *previous* job, if any, is
  now known to be finished - see [`exec()`/`is_finished()` in detail](#execis_finished-in-detail)
  below) - or the sticky error if one was already pending (the job is discarded, same as `submit()`'s
  contract).
- **`std::expected<exec_status, db_error> is_finished() const`** - polls, without blocking, whether
  the worker has finished the last `exec()`'d job: `still_pending` while it is queued or running,
  `finished` once the queue is empty and the worker is idle, or the sticky error if that job (or an
  earlier one still remembered) failed. See [`exec()`/`is_finished()` in detail](#execis_finished-in-detail)
  below for the full model this pair implements and what `finished` does and does not tell the
  caller.
- **`bool cancel() const noexcept`** - aborts whatever statement the worker is *currently running*,
  callable from any other thread. Distinct from letting a statement run to completion and from the
  destructor's own drain-then-stop (which waits a running statement out rather than interrupting it):
  `cancel()` is the one way to reach in and abort a statement that has been running longer than the
  caller is willing to wait (a timeout, a user-initiated abort, ...). Implemented with each backend's
  own native cancel primitive - PostgreSQL's `PQcancelBlocking` (connection-level), DB2's `SQLCancel`
  (statement-level, via the specific `SQLHSTMT` of whichever task is currently running) - both safe to
  call concurrently with the very `execute()`/`fetch()` they interrupt. Returns `true` if a statement
  was actually running and the cancel request was sent for it (this says nothing about whether the
  statement has actually stopped by the time `cancel()` returns - only that the request was sent);
  `false` if nothing was running or the request itself could not be sent. Whichever thread is blocked
  in `exec()`/`is_finished()`/`drain()`/`commit()` for the cancelled statement gets back an error
  instead of continuing to wait - a cancelled statement counts as a failure for the sticky-error/
  rollback machinery like any other.
- **`void submit(const query_handle<P,R>& h)`** (only for `R = rtl::no_results`) - the original,
  narrower sibling of `exec()`: hands a row's worth of parameters to the worker and returns, without
  telling the caller anything at all - not "queued", not "succeeded". Kept for existing code and for
  the (rare) case where not even `exec()`'s `std::expected<exec_status, ...>` return value is wanted.
  New code should prefer `exec()`, which does everything `submit()` does and also reports whether the
  job was actually accepted. Takes a snapshot of `h.param()`'s current values before returning, so the
  caller may start filling in the next row the moment `submit()` returns.
- **`std::expected<bool, db_error> execute_sync(const query_handle<P,R>& h)`** - runs a statement
  and *waits* for it, draining every earlier `submit()`/`exec()` first so the statement sees the whole
  transaction so far, then executing this one and (if it returns rows) fetching the first buffer of
  them. The original way to run a row-returning statement - `exec()` followed by polling
  `is_finished()` does the same job with a non-blocking option in between; use whichever fits the
  caller better.
- **`std::expected<bool, db_error> fetch_more(const query_handle<P,R>& h)`** - walks the rest of a
  row-returning statement's result set, one buffer at a time, the same false-at-end-of-data
  contract as the generated `qry::fetch()`. Works the same after `execute_sync()` or after `exec()`/
  `is_finished()` reported `finished` - both leave the first buffer already fetched.
- **`void drain()`** - waits for everything submitted so far to finish. Rarely needed directly -
  `commit()`/`execute_sync()` already drain on their own.
- **`std::expected<void, db_error> commit()`** - drains, then commits, unless a statement failed
  anywhere in the sequence, in which case it rolls back instead and returns that first error
  (sticky: later errors after the first are recorded but not surfaced, same idea as
  `std::ostream`'s failbit - keep going, fail at the end).
- **`std::expected<void, db_error> rollback()`** - drains, then rolls back and clears the sticky
  error; the object is usable again afterward.
- **`std::optional<db_error> error() const`** / **`bool has_error() const`** - inspect the sticky
  first error without committing/rolling back.

### which API to reach for

Three layers exist, in increasing order of what they buy and what they cost - reach for the
narrowest one that fits, not `async_db` by default:

- **A plain generated `qry`** (see [public API](#public-api) above) for a short sequence, especially
  one where each statement's own SQL or parameters depend on the *previous* statement's result - the
  generator's own `parser::load_file_meta_data()` is exactly this shape (`before_sql`, then
  `get_sql_metadata()`, then `after_sql`, each depending on what came before) and deliberately does
  not use `async_db` for it. `async_db` exists to overlap *independent* work with database latency;
  work that cannot start until the previous step's answer is known has nothing to overlap, and pays
  for a worker thread, two condition variables, and a snapshot-copy per call for no benefit. A single
  `select count(*)` or a one-off `insert` reads simpler as a plain `qry` too - `create()`, an
  `exec()`/`is_finished()` poll loop, and `commit()` is more ceremony than the statement itself for a
  sequence of one.
- **`submit()`/`execute_sync()`** for a longer sequence of independent statements where the caller
  has no other work to interleave between handing off one job and the next - fire-and-forget for
  every `no_results` statement, one blocking call for the `select`(s) at the end.
- **`exec()`/`is_finished()`** for the same shape of sequence, when the caller *does* have something
  useful to do between jobs (more parameter rows to compute, a progress counter to update, a
  `cancel()` deadline to check) rather than just wanting to fire the next `submit()` immediately -
  `is_finished()` lets that other work happen without blocking, at the cost of a poll loop instead of
  one blocking call.

Nothing here is coroutine-based, so there is no `co_await`-only interface to design toward - every
layer is an ordinary function, and the synchronous layer (plain `qry`) is not a fallback hidden
behind `async_db`'s own API. It stays a fully public, independently useful layer of its own -
`async_db`'s `detail::task<P,R>` is *built on it* internally (`query<P,R> q_`), and the generator's
own most sequence-heavy code path (statement validation while generating) uses the plain layer
directly rather than going through `async_db` at all.

### `exec()`/`is_finished()` in detail

**Why a `select` still needs waiting for, even here.** "Asynchronous" describes the *caller's* side
only: `exec()` lets the caller's thread move on to the next row without waiting for that row's own
`INSERT` to finish - the worker thread executes it in the background while the caller is still
filling in the next one. A `select`'s result cannot follow that pattern: nothing can be handed back
to the caller before the statement has actually run. `exec()` still accepts a row-returning handle
(unlike `submit()`, which does not even compile for one) - what changes is that the caller then polls
`is_finished()` to learn when `fetch()`/`fetch_more()` are safe to call, instead of `execute_sync()`
blocking through the whole round trip in one call.

`exec()` itself only blocks on *queue room*, never on the job it just handed over - look at its
implementation (`post_to_worker()`, in `async_db.cpp`): a one-deep queue means `exec()` returning
without blocking (queue was already empty) or after a short wait (queue had a job in it) tells the
caller nothing about the timing of the round trip itself. That is what `is_finished()` is for -
polled afterward, on its own schedule, however many times the caller likes, at zero cost beyond
locking a mutex to read two flags. Because the queue is exactly one job deep, "the queue has room
again" and "the previous job is done" are the same event, so there is nothing to track per statement
handle beyond the ordinary handle the caller already has from `prepare()`.

**What `finished` does not say.** It never distinguishes "this was a `no_results` statement, nothing
more to do" from "this was a `select`, rows are waiting to be fetched" - that distinction was never
`is_finished()`'s to make. The caller already knows which kind of statement it ran: a
`query_handle<P, rtl::no_results>` has no `fetch()`/`fetch_more()` to call on it at all (a compile
error, not a runtime one - see `execute_sync()`'s own note above), while a `query_handle<P, R>` with a
real result type does, and calling it once `is_finished()` says `finished` is exactly the same
"drain the next buffer of rows" loop `execute_sync()`/`fetch_more()` already use.

**Calling `exec()` again before a `select`'s own results have been drained is not a distinct error
state** - the queue is simply not free yet (the worker considers the previous select "still running"
in the relevant sense until its own next job can be queued), so `exec()` blocks exactly as it would
for a still-running insert. The caller falls back to synchronous behaviour rather than losing pending
rows or hitting a special case.

**Why the example below still calls `execute_sync()`, and why it matters where.** "Asynchronous"
here describes the *caller's* side only: `submit()` lets the caller's thread move on to the next row
without waiting for that row's own `INSERT` to finish - the worker thread executes it in the
background while the caller is still filling in the next one. A `select`'s result cannot follow that
pattern: nothing can be handed back to the caller before the statement has actually run, so a
row-returning statement always goes through `execute_sync()` instead of `submit()` (plugging a
`select` into `submit()` does not even compile - `submit()` is constrained to `R = rtl::no_results`,
see the table above).

`execute_sync()` does not just wait for its *own* statement - look at its implementation
(`run_on_worker()`, in `async_db.cpp`): it posts the job, then calls `drain()`, which blocks the
caller's thread until the worker's queue is empty *and* the worker is idle. That means `execute_sync()`
first waits out every `submit()` queued ahead of it (so the caller does get to see their combined
effect - the `select count(*)` below is guaranteed to count every row inserted before it, same
connection, same transaction, same order they were submitted in), and *then*, because the caller's
thread is now sitting inside `drain()` with nothing else to do, waits for its own round trip too -
functionally a plain blocking call at that point, no different from calling the generated `qry`
directly. Nothing queued *after* it can start until it returns, either.

This is why a `select`'s position in the sequence is not a matter of taste: putting one in the
*middle* forces the caller to stop and wait right there, stalling every `submit()` that would
otherwise still be queuing up behind it - the overlap `async_db` exists for is gone for as long as
that `select` takes. Putting every `select` *after* every `submit()`-only statement costs exactly one
such stall, in total, at a point where the caller has no more rows to hand off anyway - nothing is
lost by waiting exactly where the caller was going to run out of work regardless. So the rule is not
"selects are fast, put them last" - it is "a select blocks the caller no matter where it sits, so
place it where that block cannot compete with a submit() that could otherwise be overlapping
database work with the caller's own":

**What `submit()` itself tells you: nothing.** It returns `void` - not "queued", not "succeeded",
not even "attempted yet". This is the same trade-off as everything else about `submit()`: the caller
does not wait for row N's own outcome before moving on to row N+1, so there is nothing yet to report
back at the `submit()` call site itself. Errors are *sticky* instead (see the file's own "Errors are
sticky" note in `async_db.hpp`): the worker remembers only the **first** failure across every
`submit()`-only statement, silently drops every job after it (`commit`/`rollback` are the only
exceptions), and that one error only surfaces at the next call that actually waits -
`drain()`, `execute_sync()`, or `commit()`. So the loop below cannot tell you, while it runs, whether
row 3 of 500 just failed to update - only that call sequence, the two `if` checks right after the
loops, is where that shows up, and even then only as "the first row that failed", not which one.
If you need to know *which* row failed, `submit()` is the wrong tool for that row - drive it through
`execute_sync()` instead, one at a time, and check its own `std::expected` per row.

```cpp
auto work_h = rtl::async_db::create(db); // db: already connected
if (! work_h) { /* handle work_h.error() */ }
auto& work = *work_h.value();

auto ins_h  = work.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
auto ins2_h = work.prepare<dbx::crud::s_upd::p, rtl::no_results>(dbx::crud::s_upd::qry::sql());
if (! ins_h || ! ins2_h) { /* handle .error() */ }

for (const auto& row : rows_to_insert)
{
  ins_h->param()->set_id(row.id);
  ins_h->param()->set_name(row.name);
  ins_h->param()->set_created(row.created);
  work.submit(*ins_h);           // returns immediately; caller fills the next row while the worker inserts this one
}

for (const auto& row : rows_to_touch)
{
  ins2_h->param()->set_name(row.name);
  ins2_h->param()->set_created(row.created);
  ins2_h->param()->set_id(row.id);
  work.submit(*ins2_h);          // still overlapping - nothing has forced a wait yet
}

// Only now, once every submit()-only statement is queued, ask for something that needs an answer.
// execute_sync() below is where the caller actually stops and waits - see the explanation above for
// why that wait cannot be avoided, only placed where nothing else was left to overlap it with.
auto cnt_h = work.prepare<rtl::no_params, dbx::crud::s_perf_count::r>(
  dbx::crud::s_perf_count::qry::sql());
if (! cnt_h) { /* handle cnt_h.error() */ }

// If any submit() above failed, this is the first place that shows up: fetched.error() below
// carries whichever was the FIRST failure among every queued insert/update, not which row it was.
auto fetched = work.execute_sync(*cnt_h);   // drains every submitted insert/update first, then runs+fetches this select
if (! fetched) { /* handle fetched.error() */ }
std::int64_t total = cnt_h->result()->cnt(0);

// A second, redundant-looking check: commit() would report the same sticky error again if
// execute_sync() had not already failed above - kept here because commit() is also how you learn
// about a failure when the sequence has NO select at all (no execute_sync() to surface it earlier).
if (auto committed = work.commit(); ! committed)
{
  log.error("transaction failed: {}", committed.error().message);
}
```

The `select` could instead have been issued right after the first `for` loop, before the second one
- it would still have returned the correct count (every row submitted so far), but the second loop's
`submit()`s would then queue up *behind* that wait instead of overlapping the first loop's own
inserts, buying nothing and costing the same stall regardless of where it sits relative to the
`submit()` calls before it. Moving it to the end, after every `submit()`-only statement, is what
turns that unavoidable wait into the only one the whole sequence pays.

### `exec()`/`is_finished()`/`cancel()` example

Same sequence as above, rewritten around `exec()`/`is_finished()` instead of `submit()`/
`execute_sync()` - useful when there is other work to do between handing off jobs (here, a running
row counter) rather than firing the next job immediately, plus a `cancel()` guarding against a
statement that runs for longer than the caller is willing to wait:

```cpp
auto work_h = rtl::async_db::create(db);
if (! work_h) { /* handle work_h.error() */ }
auto& work = *work_h.value();

auto ins_h = work.prepare<dbx::crud::s_ins::p, rtl::no_results>(dbx::crud::s_ins::qry::sql());
if (! ins_h) { /* handle ins_h.error() */ }

std::size_t rows_queued = 0;
for (const auto& row : rows_to_insert)
{
  ins_h->param()->set_id(row.id);
  ins_h->param()->set_name(row.name);
  ins_h->param()->set_created(row.created);

  // exec() only blocks if the previous job is still queued/running - with small, fast inserts
  // this is rarely a real wait, but it is always a real one when it happens, unlike submit()'s
  // silent version of the same wait.
  const auto st = work.exec(*ins_h);
  if (! st) { /* handle st.error() - the sticky error, same as submit()'s would surface later */ }
  ++rows_queued; // work this loop can do regardless of whether exec() just blocked or not
}

auto cnt_h = work.prepare<rtl::no_params, dbx::crud::s_perf_count::r>(dbx::crud::s_perf_count::qry::sql());
if (! cnt_h) { /* handle cnt_h.error() */ }

const auto submitted = work.exec(*cnt_h); // queues the select; does not wait for it to run
if (! submitted) { /* handle submitted.error() */ }

// Poll instead of blocking - useful when there is other bookkeeping to interleave, and a natural
// place to enforce a deadline via cancel().
const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
rtl::exec_status status = rtl::exec_status::still_pending;
while (status == rtl::exec_status::still_pending)
{
  if (std::chrono::steady_clock::now() > deadline)
  {
    work.cancel(); // aborts the select currently running, if any
    break;
  }
  const auto st = work.is_finished();
  if (! st) { /* handle st.error() - reported here instead of at a blocking execute_sync() */ break; }
  status = *st;
  // ... other useful work could go here while status is still_pending ...
}

if (status == rtl::exec_status::finished)
{
  // is_finished() said finished - cnt_h's own type (a real result buffer, not rtl::no_results) is
  // what tells us fetch() applies here, not anything is_finished() itself reported.
  std::int64_t total = cnt_h->result()->cnt(0);
}

if (auto committed = work.commit(); ! committed)
{
  log.error("transaction failed: {}", committed.error().message);
}
```

For running several such sequences *concurrently* (horizontal throughput across more than one
connection), open one `async_db` per connection - see `src/programs/appl.cpp`'s own use of several
worker threads, each with its own connection, for how the generator itself does this for
`-j`/`--parallel`.
