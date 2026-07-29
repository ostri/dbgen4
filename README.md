# dbgen4

database access layer generator

## build

   cmake --preset ninja-debug
   cmake --build --preset debug -j8
   ctest --test-dir build/debug -L unit --output-on-failure

Presets: `ninja-debug`, `ninja-release`, `ninja-relwithdebinfo`, `clangd`.

## build dependencies

### C++ libraries

fmt, spdlog, nlohmann_json, yaml-cpp, valijson and Catch2 are fetched and built
by [CPM.cmake](cmake/CPM.cmake), pinned in [CMakeLists.txt](CMakeLists.txt). No
system packages needed, and no `find_package` that silently picks up whatever
the distribution ships.

Export a source cache before the first configure, otherwise every build
directory (`build/debug`, `build/release`, `build-clangd`) downloads its own copy:

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
