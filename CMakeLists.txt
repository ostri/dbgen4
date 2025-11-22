cmake_minimum_required(VERSION 3.16)
project(dbgen4 VERSION 0.1.0 DESCRIPTION "database access code generator" LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# === C++ Standard ===
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === Build Type & NDEBUG ===
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Ensure NDEBUG is defined in Release
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
else()
    add_compile_definitions(NDEBUG)
endif()

# === Compiler Warnings ===
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wnarrowing -Wswitch-enum -Werror=switch-enum -Wswitch-default)
    add_link_options(-fno-omit-frame-pointer)
endif()

# === Dependencies ===
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(DB2 REQUIRED)
find_package(valijson REQUIRED)

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
add_library(log STATIC src/log/log.cpp)
target_include_directories(log
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/log
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common
)
target_link_libraries(log PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json)
target_compile_definitions(log
  PRIVATE
    SPDLOG_FMT_EXTERNAL
    SPDLOG_COMPILED_LIB
)

# === db2_rtl ===
set(SRC_RTL_FILES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/rtl.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/db2_rtl.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/buffer_dscr.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/no_params.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/no_results.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/parameter_root.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/result_root.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2/query.cpp

)
add_library(db2_rtl STATIC ${SRC_RTL_FILES})
target_include_directories(db2_rtl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2
        ${CMAKE_CURRENT_SOURCE_DIR}/src/log
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common
      PRIVATE
        $ENV{IBM_DB_INCLUDE}
)
target_link_libraries(db2_rtl PUBLIC DB2::DB2 PRIVATE log)

# === Main executable ===
set(SRC_FILES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parser/parser.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parser/data_statement.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parser/data_statements.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parser/sql_col_def.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parameters/cmd_line_params.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parameters/context.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common/build_type.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common/parser_errors.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common/common.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common/cli_constants.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/gen/generator.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/programs/appl.cpp
)
add_executable(${PROJECT_NAME}-db2 ${SRC_FILES})
target_compile_definitions(${PROJECT_NAME}-db2 PRIVATE SPDLOG_COMPILED_LIB)

target_include_directories(${PROJECT_NAME}-db2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log
    ${CMAKE_CURRENT_SOURCE_DIR}/src/programs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/gen
    ${CMAKE_SOURCE_DIR}/src/inja
    ${CMAKE_BINARY_DIR}/
)

target_link_libraries(${PROJECT_NAME}-db2 PRIVATE
    db2_rtl
    fmt::fmt
    yaml-cpp::yaml-cpp
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    valijson
    log
    stdc++exp
)

# === clang-tidy ===
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
if(CLANG_TIDY_EXECUTABLE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            -config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            ${SRC_FILES} ${SRC_RTL_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy"
    )
endif()

# === Doxygen ===
find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(Doxygen_FOUND)
    add_subdirectory(docs)
endif()

# === generated code test ======================
# ===================================================================
# Smoke test
# ===================================================================

# Izogni se konfliktu, če že obstaja target "test_t1" (npr. iz add_subdirectory(test))
if(NOT TARGET test_t1)
    set(SMOKE_TEST_TARGET_NAME test_t1)
else()
    set(SMOKE_TEST_TARGET_NAME smoke_test_t1)
    message(STATUS "Target 'test_t1' already exists → new smoke test name is: ${SMOKE_TEST_TARGET_NAME}")
endif()

set(TEST_YAML           ${CMAKE_CURRENT_SOURCE_DIR}/yaml/t1.yaml)
set(GEN_OUTPUT_DIR      ${CMAKE_BINARY_DIR}/out)
set(GEN_STAMP           ${GEN_OUTPUT_DIR}/generated.stamp)

file(MAKE_DIRECTORY ${GEN_OUTPUT_DIR})

set(GENERATED_HPP       ${GEN_OUTPUT_DIR}/t1.hpp)
set(GENERATED_CPP       ${GEN_OUTPUT_DIR}/t1.cpp)
set(SOURCE_TEST_CPP     ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_t1.cpp)
set(GEN_TEST_CPP        ${GEN_OUTPUT_DIR}/test_t1.cpp)

# Templates - if changed it must be regenerated
set(MAIN_HPP        ${CMAKE_BINARY_DIR}/template/main_hpp.jinja)
set(CPP_TEMPLATE
  ${CMAKE_BINARY_DIR}/template/main_cpp.jinja
  ${CMAKE_BINARY_DIR}/template/buf_cpp.jinja
  )
set(HPP_TEMPLATE
  ${CMAKE_BINARY_DIR}/template/main_hpp.jinja
  ${CMAKE_BINARY_DIR}/template/buf_hpp.jinja
  )

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
            -v -t db2 -n test -u ostri -p !123alfa
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

# 2. Custom target za lažji ročni zagon
add_custom_target(generate_smoke_test DEPENDS ${GEN_STAMP})

# 3. Smoke test executable
add_executable(${SMOKE_TEST_TARGET_NAME}
    ${GENERATED_CPP}
    ${GEN_TEST_CPP}
)

target_include_directories(${SMOKE_TEST_TARGET_NAME} PRIVATE ${GEN_OUTPUT_DIR} )
target_link_libraries(${SMOKE_TEST_TARGET_NAME} PRIVATE
    db2_rtl
    log
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
    valijson
)
# Preveri podporo za opozorilo (za C++)
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wswitch-default HAVE_SWITCH_DEFAULT_WARNING)
if(HAVE_SWITCH_DEFAULT_WARNING)
    target_compile_options(${SMOKE_TEST_TARGET_NAME} PRIVATE -Wno-switch-default)
endif()

add_dependencies(${SMOKE_TEST_TARGET_NAME} generate_smoke_test)

# 4. Dodaj v ctest (ctest bo zagnal smoke test)
if(BUILD_TESTING)
    add_test(NAME ${SMOKE_TEST_TARGET_NAME} COMMAND ${SMOKE_TEST_TARGET_NAME})
    set_tests_properties(${SMOKE_TEST_TARGET_NAME} PROPERTIES
        LABELS "smoke;generated"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

message(STATUS "Smoke test '${SMOKE_TEST_TARGET_NAME}' is ready.")