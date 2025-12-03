cmake_minimum_required(VERSION 3.16)
project(dbgen4 VERSION 0.1.0 DESCRIPTION "database access code generator" LANGUAGES CXX)

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

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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

# === Compiler Warnings ===
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wnarrowing -Wswitch-enum -Werror=switch-enum -Wswitch-default)
    add_link_options(-fno-omit-frame-pointer)
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
add_library(log
  STATIC
    src/log/log.cpp
    src/log/log_impl.cpp
    )
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
     ${CMAKE_CURRENT_SOURCE_DIR}/src/dbgen4/parser/parse_yaml.cpp
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

# Enable sanitizer environment for all DB2-linked executables - should be at the end
if((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND ENABLE_SANITIZERS)
    enable_sanitizers_for_target(${PROJECT_NAME}-db2)
    enable_sanitizers_for_target(${SMOKE_TEST_TARGET_NAME})
endif()

# # ──────────────────────────────────────────────────────────────
# # Describe build type
# # ──────────────────────────────────────────────────────────────
# if(CMAKE_BUILD_TYPE)
#     string(TOUPPER "${CMAKE_BUILD_TYPE}" _build_type_upper)
#     message(STATUS "─────────────────────────────────────────────────────")
#     message(STATUS "BUILD TYPE              : ${CMAKE_BUILD_TYPE}")
#     message(STATUS "CMAKE_CXX_FLAGS         : ${CMAKE_CXX_FLAGS}")
#     message(STATUS "CMAKE_CXX_FLAGS_${_build_type_upper} : ${CMAKE_CXX_FLAGS_${_build_type_upper}}")
#     message(STATUS "NDEBUG defined          : ${CMAKE_CXX_FLAGS_${_build_type_upper}}")
#     if(CMAKE_CXX_FLAGS_${_build_type_upper} MATCHES "-g")
#         message(STATUS "Debug symbols           : YES")
#     else()
#         message(STATUS "Debug symbols           : NO")
#     endif()
#     if(CMAKE_CXX_FLAGS_${_build_type_upper} MATCHES "fsanitize")
#         message(STATUS "Sanitizers              : YES")
#     else()
#         message(STATUS "Sanitizers              : NO")
#     endif()
#     message(STATUS "─────────────────────────────────────────────────────")
# else()
#     # Multi-config generatorators (Visual Studio, Xcode, Ninja Multi-Config …)
#     message(STATUS "Multi-config generator – possible build types: ${CMAKE_CONFIGURATION_TYPES}")
# endif()
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