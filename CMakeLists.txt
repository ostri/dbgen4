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
configure_file(${CMAKE_SOURCE_DIR}/config/hpp_template.inja
               ${CMAKE_BINARY_DIR}/template/hpp_template.inja COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/cpp_template.inja
               ${CMAKE_BINARY_DIR}/template/cpp_template.inja COPYONLY)


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
     src/rtl/rtl.cpp
     src/rtl/db2/db2_rtl.cpp
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