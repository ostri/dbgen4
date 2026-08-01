cmake_minimum_required(VERSION 3.25)
project(dbgen4 VERSION 0.1.0 DESCRIPTION "database access code generator" LANGUAGES CXX)

option(BUILD_TESTING "Build the testing tree." ON)
include(CTest)
enable_testing()
# ===================================================================
# Force the same build tool and behavior as manual command line
# ===================================================================

# 1. Always use Ninja (fastest + single-config → CMAKE_BUILD_TYPE actually works)
if(NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "Ninja" CACHE STRING "Force Ninja generator" FORCE)
endif()

# 2. Disable multi-config nonsense that VS Code loves to enable
set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)

# 3. Default to Debug if nothing is specified (both CLI and VS Code)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type" FORCE)
endif()

# 4. Optional: lock it so VS Code cannot silently switch to multi-config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# === Build Type & NDEBUG ===
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# === C++ Standard ===
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure NDEBUG is defined in Release
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
else()
    add_compile_definitions(NDEBUG)
endif()

# ===================================================================
# Compiler Warnings
#
# Carried by an interface target rather than add_compile_options, because
# add_compile_options is directory scoped: it reaches every dependency CPM
# pulls into the build as well. -Werror on somebody else's code is how the
# pile of -Wno-error=... workarounds for Catch2 came about. Every target of
# ours links this; nothing else does.
# ===================================================================
add_library(dbgen4_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dbgen4_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror -Wnarrowing
        -Wswitch-enum -Werror=switch-enum -Wswitch-default)
    add_link_options(-fno-omit-frame-pointer)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Catch2's INTERNAL_CATCH_UNIQUE_NAME expands to __COUNTER__, which clang
    # reports under -Wpedantic as a C2y extension while gcc says nothing. It is
    # a construct inside a third party macro that the build accepts either way,
    # so it is silenced rather than allowed to fail -Werror on one compiler.
    target_compile_options(dbgen4_warnings INTERFACE -Wno-c2y-extensions)
endif()

# ===================================================================
# Sanitizers – enabled only in Debug and RelWithDebInfo builds
# Fully compatible with IBM DB2 thanks to protect_shadow_gap=0 workaround
# ===================================================================
option(ENABLE_SANITIZERS "Enable Address/Undefined/Leak sanitizers in Debug builds" ON)

if((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND ENABLE_SANITIZERS)

    # AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer (GCC & Clang)
    add_compile_options(-fsanitize=address,undefined,leak -fno-omit-frame-pointer -g3 -O1)
    add_link_options(   -fsanitize=address,undefined,leak)

    # Runtime options – fix DB2 shadow memory conflict and give nicer reports
    set(SANITIZER_ENV
        "ASAN_OPTIONS=protect_shadow_gap=0:detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=0:abort_on_error=1:strip_path_prefix=${CMAKE_SOURCE_DIR}/"
        "LSAN_OPTIONS=detect_leaks=1:suppressions=${CMAKE_SOURCE_DIR}/lsan.supp:print_suppressions=0"
        "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
    )

    # Helper to attach the environment variables to every executable that links DB2
    function(enable_sanitizers_for_target target_name)
        set_property(TARGET ${target_name} APPEND PROPERTY ENVIRONMENT ${SANITIZER_ENV})
        message(STATUS "Sanitizers + DB2 compatibility enabled for target: ${target_name}")
    endfunction()

else()
    message(STATUS "Release/MinSizeRel build → sanitizers disabled")
endif()

# ===================================================================
# Dependencies
#
# All of them come through CPM, pinned here rather than taken from whatever
# the distribution happens to ship, so that this file is the whole answer to
# "which versions does dbgen4 build against".
#
# CPM caches the downloaded sources in $CPM_SOURCE_CACHE when that is set -
# worth exporting it, since every build directory (build/debug, build/release,
# build-clangd) would otherwise fetch its own copy. See README.md.
#
# SYSTEM YES silences warnings from their headers; EXCLUDE_FROM_ALL keeps
# their install and test targets out of ours.
# ===================================================================

# Several of these still declare cmake_minimum_required(VERSION 3.x) from
# before CMake 4 dropped compatibility with it. This grants them the oldest
# policy level CMake 4 still accepts, without pinning older releases.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 11.2.0
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
)

# SPDLOG_FMT_EXTERNAL has to agree between the spdlog we build here and
# log_impl.cpp that consumes it - a mismatch is an ODR violation that shows up
# as a link error at best. The matching definition is on the log target below.
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    GIT_TAG v1.15.3
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SPDLOG_FMT_EXTERNAL ON"
        "SPDLOG_BUILD_SHARED OFF"
        "SPDLOG_INSTALL OFF"
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_BUILD_TESTS OFF"
)

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    GIT_TAG v3.11.3
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS "JSON_BuildTests OFF"
)

CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG 0.8.0
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "YAML_CPP_BUILD_TESTS OFF"
        "YAML_CPP_BUILD_TOOLS OFF"
        "YAML_CPP_INSTALL OFF"
        "YAML_BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
    NAME valijson
    GITHUB_REPOSITORY tristanpenman/valijson
    GIT_TAG v1.0.6
    SYSTEM YES
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "valijson_BUILD_TESTS OFF"
        "valijson_BUILD_EXAMPLES OFF"
        "valijson_INSTALL_HEADERS OFF"
)

# Catch2 is only needed by the unit tests, but it belongs here with the rest
# rather than buried in the test block - it is fetched the same way as everything
# else, so the -Wno-error workarounds it used to need are gone.
if(BUILD_TESTING)
    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        GIT_TAG v3.7.1
        SYSTEM YES
        EXCLUDE_FROM_ALL YES
    )
    # catch_discover_tests lives in Catch2's extras and every test target below
    # uses it, so the module is pulled in here rather than beside one of them.
    # CPM exports Catch2_SOURCE_DIR; FetchContent used to export catch2_SOURCE_DIR.
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

# === Database backends ===
# Each backend is optional: the generator is built once per backend that is
# available, as dbgen4-<backend>. At least one is required.
find_package(DB2)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQ IMPORTED_TARGET libpq)
endif()

if(DB2_FOUND)
    message(STATUS "Backend db2:  enabled (${DB2_LIBRARIES})")
else()
    message(STATUS "Backend db2:  disabled - DB2 CLI not found")
endif()
if(LIBPQ_FOUND)
    message(STATUS "Backend psql: enabled (libpq ${LIBPQ_VERSION})")
else()
    message(STATUS "Backend psql: disabled - libpq not found")
endif()
if(NOT DB2_FOUND AND NOT LIBPQ_FOUND)
    message(FATAL_ERROR "No database backend found - install the DB2 CLI driver or libpq-devel.")
endif()

# # === inja dependency ===
# set_source_files_properties(
#     ${CMAKE_SOURCE_DIR}/src/inja/inja.hpp
#     PROPERTIES
#     COMPILE_FLAGS "-Wno-switch -Wno-switch-enum -Wno-error=switch-enum -Wno-switch-default"
# )
# === Copy config files ===
configure_file(${CMAKE_SOURCE_DIR}/config/log.debug.conf
               ${CMAKE_BINARY_DIR}/config/log.debug.conf COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/log.release.conf
               ${CMAKE_BINARY_DIR}/config/log.release.conf COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/main_hpp.jinja
               ${CMAKE_BINARY_DIR}/template/main_hpp.jinja COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/main_cpp.jinja
               ${CMAKE_BINARY_DIR}/template/main_cpp.jinja COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/buf_hpp.jinja
               ${CMAKE_BINARY_DIR}/template/buf_hpp.jinja COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/buf_cpp.jinja
               ${CMAKE_BINARY_DIR}/template/buf_cpp.jinja COPYONLY)


# === log library ===
set (LOG_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/log
    ${CMAKE_SOURCE_DIR}/src/dbgen4/common
)
add_library(log
  STATIC
    src/log/log.cpp
    src/log/log_impl.cpp
    )
# PUBLIC: log.hpp is the facade every other target includes, so its own headers
# have to travel with it.
target_include_directories(log
    PUBLIC ${LOG_INCLUDE_DIRS}
)
# fmt is PUBLIC because log.hpp includes <fmt/format.h>. spdlog and json stay
# PRIVATE - only log_impl.cpp knows about them, which is the point of the facade.
target_link_libraries(log
  PUBLIC  fmt::fmt
  PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json dbgen4_warnings
)
target_compile_definitions(log
  PRIVATE
    SPDLOG_FMT_EXTERNAL
    SPDLOG_COMPILED_LIB
)

# === rtl_logger library ===
# Replaces `log` as the facade every backend and the generator link against.
# Built the same way (PIMPL hides spdlog behind logger_impl.hpp/cpp). `log`
# itself is kept only for its own unit test (src/test/log_test.cpp).
set (RTL_LOGGER_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/logger
    ${CMAKE_SOURCE_DIR}/src/dbgen4/common
)
add_library(rtl_logger
  STATIC
    src/logger/logger.cpp
    src/logger/logger_impl.cpp
    )
target_include_directories(rtl_logger
    PUBLIC ${RTL_LOGGER_INCLUDE_DIRS}
)
target_link_libraries(rtl_logger
  PUBLIC  fmt::fmt
  PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json dbgen4_warnings
)
target_compile_definitions(rtl_logger
  PRIVATE
    SPDLOG_FMT_EXTERNAL
    SPDLOG_COMPILED_LIB
)

# === db2_rtl ===
# Include dirs every backend needs: the neutral rtl vocabulary, the logger
# facade and the dbgen4 common helpers.
set (RTL_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/src/rtl
  ${CMAKE_SOURCE_DIR}/src/logger
  ${CMAKE_SOURCE_DIR}/src/dbgen4/common
)

if(DB2_FOUND)
    add_library(db2_rtl STATIC
      ${CMAKE_SOURCE_DIR}/src/rtl/rtl.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/parameter_root.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/db2/db2_rtl.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/db2/odbc_error.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/db2/query.cpp
    )
    target_include_directories(db2_rtl PUBLIC
        ${RTL_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src/rtl/db2
        $ENV{IBM_DB_INCLUDE}
    )
    target_link_libraries(db2_rtl PUBLIC DB2::DB2 rtl_logger PRIVATE dbgen4_warnings)
endif()

if(LIBPQ_FOUND)
    add_library(psql_rtl STATIC
      ${CMAKE_SOURCE_DIR}/src/rtl/rtl.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/parameter_root.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/psql/psql_rtl.cpp
      ${CMAKE_SOURCE_DIR}/src/rtl/psql/query.cpp
    )
    target_include_directories(psql_rtl PUBLIC
        ${RTL_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src/rtl/psql
    )
    target_link_libraries(psql_rtl PUBLIC PkgConfig::LIBPQ rtl_logger PRIVATE dbgen4_warnings)
endif()

# === Main executable ===
set(GEN_SRC_FILES
#     ${CMAKE_SOURCE_DIR}/src/dbgen4.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parser/parser.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parser/data_statement.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parser/data_statements.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parser/sql_col_def.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parser/parse_yaml.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parameters/cmd_line_params.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/parameters/context.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/common/build_type.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/common/parser_errors.cpp
     ${CMAKE_SOURCE_DIR}/src/dbgen4/common/common.cpp

     ${CMAKE_SOURCE_DIR}/src/dbgen4/gen/generator.cpp
     ${CMAKE_SOURCE_DIR}/src/programs/appl.cpp
)
set(GEN_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/logger
    ${CMAKE_SOURCE_DIR}/src/programs
    ${CMAKE_SOURCE_DIR}/src/dbgen4/common
    ${CMAKE_SOURCE_DIR}/src/dbgen4/gen
    ${CMAKE_SOURCE_DIR}/src/dbgen4/parameters
    ${CMAKE_SOURCE_DIR}/src/dbgen4/parser
    ${CMAKE_SOURCE_DIR}/src/rtl
    ${CMAKE_SOURCE_DIR}/src/inja
)

# The generator sources are backend agnostic - the only thing that differs
# between dbgen4-db2 and dbgen4-psql is which rtl library provides make_db().
function(add_generator_target backend rtl_library)
    set(target ${PROJECT_NAME}-${backend})
    add_executable(${target} ${CMAKE_SOURCE_DIR}/src/dbgen4.cpp ${GEN_SRC_FILES})
    target_compile_definitions(${target} PRIVATE SPDLOG_COMPILED_LIB)
    target_include_directories(${target} PUBLIC ${GEN_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE
        ${rtl_library}
        yaml-cpp::yaml-cpp
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        valijson
        rtl_logger
        stdc++exp
        dbgen4_warnings
    )
endfunction()

if(DB2_FOUND)
    add_generator_target(db2 db2_rtl)
endif()
if(LIBPQ_FOUND)
    add_generator_target(psql psql_rtl)
endif()

# === clang-tidy ===
# Every .cpp under src/ goes through this target, not just GEN_SRC_FILES - that
# list is only the main executable's sources, so db2_rtl, psql_rtl and the
# generator sources were silently skipped before. Headers are covered too:
# clang-tidy analyzes them as included by these translation units, same as it
# does for the source-filtered .cpp itself.
#
# run-clang-tidy (not clang-tidy directly) so this uses all available cores -
# it drives one clang-tidy subprocess per translation unit via -j, instead of
# one process working through the whole list serially.
#
# -source-filter restricts to our own .cpp files: the compilation database
# also lists every CPM dependency's sources (fmt, spdlog, yaml-cpp, Catch2),
# which would otherwise be tidied as their own translation units too.
# src/test/ is excluded from that filter, same as CXX_CLANG_TIDY "" on the
# unit_tests target below: Catch2's macros trigger clang-tidy false positives
# there.
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py)
if(RUN_CLANG_TIDY_EXECUTABLE)
    add_custom_target(tidy
        COMMAND ${RUN_CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            -config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            -quiet
            -source-filter "^${CMAKE_SOURCE_DIR}/src/(?!test/).*\\.cpp$"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy (parallel)"
        VERBATIM
    )
endif()

# === Doxygen ===
find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(Doxygen_FOUND)
    add_subdirectory(docs)
endif()

# === generated code test ======================
# Shared between the db2-only smoke test below and the crud test targets
# further down, which run for whichever backend or backends were found - so
# these live above both rather than inside either's guard.
set(GEN_OUTPUT_DIR      ${CMAKE_BINARY_DIR}/out)
file(MAKE_DIRECTORY ${GEN_OUTPUT_DIR})
set(CPP_TEMPLATE
  ${CMAKE_BINARY_DIR}/template/main_cpp.jinja
  ${CMAKE_BINARY_DIR}/template/buf_cpp.jinja
  )
set(HPP_TEMPLATE
  ${CMAKE_BINARY_DIR}/template/main_hpp.jinja
  ${CMAKE_BINARY_DIR}/template/buf_hpp.jinja
  )

# ===================================================================
# Smoke test
#
# Generates code from yaml/t1.yaml against a live database, compiles it and
# runs it. Tied to db2 for now: the generated code links the ODBC runtime in
# rtl/db2/query.hpp, which has no psql counterpart yet.
# ===================================================================
if(DB2_FOUND)

# Where the tests that need a live DB2 should connect. Override at configure
# time, e.g. -DDB2_TEST_PASS=...
#
# A dedicated account rather than a developer's own login. DB2 authenticates
# against the OS here, so using a personal account would mean the build carries
# that person's login password in a cache variable, and every checkout would
# need its own. dbgen4 owns its schema and holds DBADM on the test database, so
# the tests can create, fill and drop their tables without touching anybody
# else's. See README.md for how to set it up.
set(DB2_TEST_HOST "localhost"  CACHE STRING "host of the DB2 used by the tests")
set(DB2_TEST_PORT "50000"      CACHE STRING "port of the DB2 used by the tests")
set(DB2_TEST_DB   "test"       CACHE STRING "database name used by the tests")
set(DB2_TEST_USER "dbgen4"     CACHE STRING "user for the test database")
set(DB2_TEST_PASS "dbgen4"     CACHE STRING "password for the test database")

if(NOT TARGET test_t1)
    set(SMOKE_TEST_TARGET_NAME test_t1)
else()
    set(SMOKE_TEST_TARGET_NAME smoke_test_t1)
    message(STATUS "Target 'test_t1' already exists → new smoke test name is: ${SMOKE_TEST_TARGET_NAME}")
endif()

set(TEST_YAML           ${CMAKE_SOURCE_DIR}/yaml/t1.yaml)
set(GEN_STAMP           ${GEN_OUTPUT_DIR}/generated.stamp)

set(GENERATED_HPP       ${GEN_OUTPUT_DIR}/t1.hpp)
set(GENERATED_CPP       ${GEN_OUTPUT_DIR}/t1.cpp)
set(SOURCE_TEST_CPP     ${CMAKE_SOURCE_DIR}/src/test/test_t1.cpp)
set(GEN_TEST_CPP        ${GEN_OUTPUT_DIR}/test_t1.cpp)

set(MAIN_HPP        ${CMAKE_BINARY_DIR}/template/main_hpp.jinja)

# Main program for smoke test
if(NOT EXISTS ${SOURCE_TEST_CPP})
    message(FATAL_ERROR "File src/test/test_t1.cpp is missing – smoke test can't be built!")
endif()

# 1. code generation
add_custom_command(
    OUTPUT
        ${GEN_STAMP}
        ${GENERATED_HPP}
        ${GENERATED_CPP}
        ${GEN_TEST_CPP}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SOURCE_TEST_CPP} ${GEN_TEST_CPP}
    COMMAND ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-db2
            -o ${GEN_OUTPUT_DIR}
            -v -t db2 -n ${DB2_TEST_DB} -u ${DB2_TEST_USER} -p ${DB2_TEST_PASS}
            ${TEST_YAML}
    COMMAND ${CMAKE_COMMAND} -E touch ${GEN_STAMP}
    DEPENDS
        ${PROJECT_NAME}-db2                     # generator program
        ${TEST_YAML}                            # test yaml file
        ${SOURCE_TEST_CPP}                      # main program source (test_t1.cpp)
        ${HPP_TEMPLATE}                         # template files for hpp generation
        ${CPP_TEMPLATE}                         # template files for cpp generation
    COMMENT "Generating from '${TEST_YAML}' to '${GEN_OUTPUT_DIR}'."
    VERBATIM
)

add_custom_target(generate_smoke_test DEPENDS ${GEN_STAMP})

# 3. Smoke test executable
add_executable(${SMOKE_TEST_TARGET_NAME}
    ${GENERATED_CPP}
    ${GEN_TEST_CPP}
#    ${GEN_SRC_FILES}
)

# rtl_logger publishes its own include dirs now, so only the generated output
# and the rtl vocabulary have to be named here
target_include_directories(${SMOKE_TEST_TARGET_NAME} PRIVATE ${GEN_OUTPUT_DIR} ${RTL_INCLUDE_DIRS})
target_link_libraries(${SMOKE_TEST_TARGET_NAME} PRIVATE
    Catch2::Catch2WithMain
    db2_rtl
    rtl_logger
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
    valijson
    dbgen4_warnings
)
add_dependencies(${SMOKE_TEST_TARGET_NAME} generate_smoke_test)

if(BUILD_TESTING)
    # One ctest entry per TEST_CASE rather than one for the binary, so a failure
    # names the property that broke. Discovery only lists the cases, and these
    # touch no database, so it is safe to do at build time.
    catch_discover_tests(${SMOKE_TEST_TARGET_NAME}
        PROPERTIES LABELS "smoke;generated"
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

message(STATUS "Smoke test '${SMOKE_TEST_TARGET_NAME}' is ready.")

# Enable sanitizer environment for all DB2-linked executables - should be at the end
if((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND ENABLE_SANITIZERS)
    enable_sanitizers_for_target(${PROJECT_NAME}-db2)
    enable_sanitizers_for_target(${SMOKE_TEST_TARGET_NAME})
endif()

endif() # DB2_FOUND - smoke test

# ===================================================================
# crud round trip test
#
# Unlike the smoke test above, this one actually talks to the database
# through the generated code: insert, read back and compare, update, read
# back and compare, delete, confirm gone.
#
# One function instantiated once per backend, since db2 and psql each need
# their own generated crud.hpp/crud.cpp (backend specific placeholder style),
# their own runtime library and their own test fixture (test_db.hpp resolved
# through a per-backend include directory - see src/test/db2 and
# src/test/psql). Everything else - target layout, ctest discovery, the
# environment variable workaround below - is identical between the two, which
# is what this function captures. Not nested inside DB2_FOUND: a psql-only
# build (no DB2 CLI installed) must still get its crud test.
# ===================================================================
function(add_crud_test_target backend rtl_library test_db_include_dir)
    set(extra_sources ${ARGN})
    set(out_dir     ${GEN_OUTPUT_DIR}/${backend})
    set(crud_yaml   ${CMAKE_SOURCE_DIR}/yaml/crud.yaml)
    set(crud_hpp    ${out_dir}/crud.hpp)
    set(crud_cpp    ${out_dir}/crud.cpp)
    set(crud_stamp  ${out_dir}/crud.stamp)
    set(target      test_crud_${backend})
    string(TOUPPER ${backend} backend_upper)
    set(test_db_env ${backend_upper}_TEST)

    file(MAKE_DIRECTORY ${out_dir})

    add_custom_command(
        OUTPUT ${crud_stamp} ${crud_hpp} ${crud_cpp}
        COMMAND ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${backend}
                -o ${out_dir}
                -t ${backend} --host ${${test_db_env}_HOST} --port ${${test_db_env}_PORT}
                -n ${${test_db_env}_DB} -u ${${test_db_env}_USER} -p ${${test_db_env}_PASS}
                ${crud_yaml}
        COMMAND ${CMAKE_COMMAND} -E touch ${crud_stamp}
        DEPENDS ${PROJECT_NAME}-${backend} ${crud_yaml} ${HPP_TEMPLATE} ${CPP_TEMPLATE}
        COMMENT "Generating crud test code (${backend}) from '${crud_yaml}'."
        VERBATIM
    )
    add_custom_target(generate_crud_test_${backend} DEPENDS ${crud_stamp})

    add_executable(${target} ${crud_cpp}
        ${CMAKE_SOURCE_DIR}/src/test/test_crud.cpp
        ${CMAKE_SOURCE_DIR}/src/test/test_types.cpp
        ${CMAKE_SOURCE_DIR}/src/test/test_crud_batch.cpp
        ${extra_sources})
    target_include_directories(${target} PRIVATE ${out_dir} ${RTL_INCLUDE_DIRS}
                               ${CMAKE_SOURCE_DIR}/src/rtl/${backend} ${test_db_include_dir} $ENV{IBM_DB_INCLUDE})
    target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain ${rtl_library} rtl_logger
                          spdlog::spdlog nlohmann_json::nlohmann_json stdc++exp dbgen4_warnings)
    add_dependencies(${target} generate_crud_test_${backend})

    # Catch2 owns argv, so the connection details reach the test through the
    # environment instead of as positional arguments.
    set(test_env
        "DBGEN4_TEST_HOST=${${test_db_env}_HOST}"
        "DBGEN4_TEST_PORT=${${test_db_env}_PORT}"
        "DBGEN4_TEST_DB=${${test_db_env}_DB}"
        "DBGEN4_TEST_USER=${${test_db_env}_USER}"
        "DBGEN4_TEST_PASS=${${test_db_env}_PASS}"
    )
    # ENVIRONMENT is a test property, so the sanitizer options belong here to
    # actually reach the run - enable_sanitizers_for_target sets them on the
    # target, where ctest never looks at them.
    if(DEFINED SANITIZER_ENV)
        list(APPEND test_env ${SANITIZER_ENV})
    endif()

    # One ctest entry per TEST_CASE. Discovery runs the binary with --list-tests,
    # which does not connect, so a database that is down costs a failing test
    # rather than a failing configure.
    #
    # ENVIRONMENT is deliberately not passed through PROPERTIES here. catch_discover_tests
    # forwards those to its discovery script as -D "TEST_PROPERTIES=...", and that round
    # trip through a command line flattens a multi entry list: only the first variable
    # survives, so LABELS and WORKING_DIRECTORY (one value each) arrive intact while the
    # environment silently loses all but DBGEN4_TEST_HOST. That showed up as every live
    # test failing to connect with the built in defaults while the same binary ran green
    # by hand. Verified against Catch2 3.7.1; no amount of escaping at this call site
    # fixes it, because the flattening happens before the value gets here.
    # TEST_PREFIX matters beyond cosmetics here: db2 and psql share every
    # TEST_CASE name in test_crud.cpp/test_types.cpp (that is the point - the
    # same source runs against both), and CTest's set_tests_properties()
    # matches by name across the whole suite. Without a prefix the two
    # backends' tests collide under one name and the environment meant for
    # one binary gets applied to the other.
    catch_discover_tests(${target}
        TEST_PREFIX "${backend}: "
        PROPERTIES LABELS "crud;generated;live-db;${backend}"
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # So the environment is set after discovery instead. ${${target}_TESTS} only
    # exists while CTest reads the test file, which is exactly when this fragment
    # is evaluated, and the list reaches set_tests_properties without crossing a
    # command line.
    file(GENERATE
         OUTPUT "${CMAKE_BINARY_DIR}/${target}_env.cmake"
         CONTENT "set_tests_properties(\${${target}_TESTS} PROPERTIES ENVIRONMENT \"${test_env}\")\n")
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
                 "${CMAKE_BINARY_DIR}/${target}_env.cmake")

    if((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND ENABLE_SANITIZERS)
        enable_sanitizers_for_target(${target})
    endif()
endfunction()

if(DB2_FOUND AND BUILD_TESTING)
    add_crud_test_target(db2 db2_rtl ${CMAKE_SOURCE_DIR}/src/test/db2
        ${CMAKE_SOURCE_DIR}/src/test/test_perf.cpp
        ${CMAKE_SOURCE_DIR}/src/test/test_bench.cpp
        ${CMAKE_SOURCE_DIR}/src/test/test_crud_batch_db2.cpp)
endif()

if(LIBPQ_FOUND AND BUILD_TESTING)
    # Where the tests that need a live PostgreSQL should connect. Override at
    # configure time, e.g. -DPSQL_TEST_PASS=... Mirrors DB2_TEST_* above - see
    # README.md for how the dbgen4 role/database was set up.
    set(PSQL_TEST_HOST "postgres.lan" CACHE STRING "host of the PostgreSQL used by the tests")
    set(PSQL_TEST_PORT "5432"         CACHE STRING "port of the PostgreSQL used by the tests")
    set(PSQL_TEST_DB   "dbgen4"       CACHE STRING "database name used by the tests")
    set(PSQL_TEST_USER "dbgen4"       CACHE STRING "user for the test database")
    set(PSQL_TEST_PASS "dbgen4"       CACHE STRING "password for the test database")

    # test_perf.cpp/test_bench.cpp are not passed here: they measure batch
    # throughput at scale (hundreds/thousands of rows), which is a separate
    # question from the correctness test_crud_batch.cpp/execute_batch() cover.
    add_crud_test_target(psql psql_rtl ${CMAKE_SOURCE_DIR}/src/test/psql
        ${CMAKE_SOURCE_DIR}/src/test/test_crud_batch_psql.cpp)
endif()

# === common headers – interface library
add_library(project_common INTERFACE)
target_include_directories(project_common INTERFACE
    ${CMAKE_SOURCE_DIR}/src/dbgen4/common
    ${CMAKE_SOURCE_DIR}/src/log
    ${CMAKE_SOURCE_DIR}/src/logger
    ${CMAKE_SOURCE_DIR}/src/rtl
    ${CMAKE_SOURCE_DIR}/src/rtl/db2
)
# ===================================================================
# Unit tests with Catch2
# ===================================================================
if(BUILD_TESTING)
    enable_testing()

    # Catch2 itself comes from the dependency block above. It needs no warning
    # suppressions any more: our flags live on dbgen4_warnings, which Catch2
    # does not link.
    #
    # clang-tidy is the one thing still worth turning off for it, and only when
    # something has actually turned clang-tidy on for the whole build.
    if(DEFINED CMAKE_CXX_CLANG_TIDY)
        foreach(catch_target Catch2 Catch2WithMain)
            if(TARGET ${catch_target})
                set_target_properties(${catch_target} PROPERTIES CXX_CLANG_TIDY "")
            endif()
        endforeach()
    endif()

    # src/test/log_test.cpp is excluded here: it exercises the old `log` facade,
    # which nothing in the application uses any more (see rtl_logger below).
    # Linking both `log` and `rtl_logger` into one process means two independent
    # spdlog instances fight over spdlog's single global thread pool in async
    # mode (spdlog::init_thread_pool() from the second one orphans the first
    # logger's pool, surfacing as "async log: thread pool doesn't exist anymore"
    # at shutdown) - so the two facades are kept out of the same binary.
    add_executable(unit_tests
        src/test/main.cpp
        src/test/parser_test.cpp
        ${GEN_SRC_FILES}
    )
    set_target_properties(unit_tests PROPERTIES
      CXX_CLANG_TIDY ""          # no clang-tidy for test cases (catch triggers false positives)
    )

    # the unit tests exercise the generator, which needs make_db() from some
    # backend - either will do, prefer db2 when both are present
    if(DB2_FOUND)
        set(UNIT_TEST_RTL db2_rtl)
    else()
        set(UNIT_TEST_RTL psql_rtl)
    endif()

    target_link_libraries(unit_tests PRIVATE
        Catch2::Catch2WithMain
        rtl_logger
        ${UNIT_TEST_RTL}
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        valijson
        stdc++exp
        dbgen4_warnings
    )

    # Catch2::Catch2WithMain carries its own include directories - the manual
    # ${catch2_SOURCE_DIR}/src entry that used to be here was redundant.
    target_include_directories(unit_tests
      PRIVATE
        ${GEN_INCLUDE_DIRS}
        ${RTL_INCLUDE_DIRS}
    )

    target_compile_features(unit_tests PRIVATE cxx_std_23)

    if(ENABLE_SANITIZERS AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        enable_sanitizers_for_target(unit_tests)
    endif()

    catch_discover_tests(unit_tests PROPERTIES LABELS "unit")
endif()
# ===================================================================
# Show real compile and link flags used for selected targets
# (works correctly with modern CMake + add_compile_options / target_*)
# ===================================================================
function(show_target_build_info target)
    if(NOT TARGET ${target})
        return()
    endif()

    #message(STATUS "═════════════════════════════════════════════════════")
    message(STATUS "TARGET                  : ${target}")
    message(STATUS "BUILD TYPE              : ${CMAKE_BUILD_TYPE}")

    # --- Compile options (includes add_compile_options, target_compile_options, etc.) ---
    get_target_property(_compile_opts ${target} COMPILE_OPTIONS)
    if(NOT _compile_opts)
        set(_compile_opts "<none>")
    endif()

    get_target_property(_iface_compile ${target} INTERFACE_COMPILE_OPTIONS)
    if(_iface_compile)
        list(APPEND _compile_opts "INTERFACE_COMPILE_OPTIONS: ${_iface_compile}")
    endif()

    # --- Link options ---
    get_target_property(_link_opts ${target} LINK_OPTIONS)
    if(NOT _link_opts)
        set(_link_opts "<none>")
    endif()

    get_target_property(_iface_link ${target} INTERFACE_LINK_OPTIONS)
    if(_iface_link)
        list(APPEND _link_opts "INTERFACE_LINK_OPTIONS: ${_iface_link}")
    endif()

    # --- Final output ---
    message(STATUS "COMPILE_OPTIONS         : ${_compile_opts}")
    message(STATUS "LINK_OPTIONS            : ${_link_opts}")

    # Helper checks
    string(JOIN " " _all_compile_flags ${_compile_opts})
    if(_all_compile_flags MATCHES "-g")
        message(STATUS "Debug symbols (-g)      : YES")
    else()
        message(STATUS "Debug symbols (-g)      : NO")
    endif()

    if(_all_compile_flags MATCHES "sanitize")
        message(STATUS "Sanitizers              : YES")
    else()
        message(STATUS "Sanitizers              : NO")
    endif()

    message(STATUS "═════════════════════════════════════════════════════")
endfunction()

message(STATUS "═════════════════════════════════════════════════════")
# Show info for the main executable
show_target_build_info(${PROJECT_NAME}-db2)

# Optionally show for the smoke test as well (uncomment if desired)
show_target_build_info(test_t1)

# Very compact one-liner (useful when you just want a quick check)
get_target_property(C ${PROJECT_NAME}-db2 COMPILE_OPTIONS)
get_target_property(L ${PROJECT_NAME}-db2 LINK_OPTIONS)
message(STATUS "QUICK → ${CMAKE_BUILD_TYPE} | COMPILE: ${C} | LINK: ${L}")