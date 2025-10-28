# Works with 3.11 and tested through 3.22
cmake_minimum_required(VERSION 4.1.2)

project(
  dbgen4
  VERSION 0.1.0
  DESCRIPTION "database access code generator"
  LANGUAGES CXX
)
include(FetchContent)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

  # Only do these if this is the main project, and not if it is included through add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  set(CMAKE_CXX_STANDARD 23)                              # compiler level
  set(CMAKE_CXX_STANDARD_REQUIRED ON)                     # compiler requrement enforced
  set(CMAKE_CXX_EXTENSIONS OFF)                           # only standard c++
  set(PREBUILT_MODULE_PATH ${CMAKE_BINARY_DIR}/modules)   # modules are stored in build/modules
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)                   # it produces compile_commands.json

  if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
      add_compile_options(
      -Wall
      -Wextra
      -pedantic
      -Werror
      -Wnarrowing
      # -pthread
      -fsanitize=leak
      #-fsanitize=address
      #-fno-omit-frame-pointer
      #-fsanitize=undefined
      #-fno-sanitize-recover=all
      #-fsanitize=float-divide-by-zero
      #-fsanitize=float-cast-overflow
      #-fno-sanitize=null
      #-fno-sanitize=alignment
      )
      add_link_options(
        -fno-omit-frame-pointer
        -fsanitize=address
      #  -fsanitize=undefined
      )
    # elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    #   set(OPTIONS /W4 /WX)
    #   if(HAS_PERMISSIVE_FLAG)
    #       set(OPTIONS ${OPTIONS} /permissive-)
    #   endif()
    endif()
    # set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    # set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
endif()


# include(FetchContent)

# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
# FetchContent_MakeAvailable(json)

# # Enable clang-tidy during build
# set(CMAKE_CXX_CLANG_TIDY
#     clang-tidy;
#     -format-style=file;
# )

# Docs only available if this is the main app
find_package(Doxygen OPTIONAL_COMPONENTS dot mscgen dia)
if(Doxygen_FOUND)
  add_subdirectory(docs)
else()
  message(STATUS "Doxygen not found, not building docs")
endif()

# # The compiled library code is here
add_subdirectory(examples)
# add_subdirectory(externals)

set(SRC_SUBDIRS 
  src/log
  src/programs
  src/dbgen4/parser
  src/dbgen4/parameters
  src/dbgen4/common
  src/rtl
  src/rtl/db2
)
# foreach(SUBDIR ${SRC_SUBDIRS})
#   add_subdirectory(${SUBDIR})
# endforeach()

#dependant libraries
#FetchContent_Declare(
#  fmt
#  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
#  GIT_TAG 10.2.1
#  GIT_SHALLOW TRUE
#)
#FetchContent_MakeAvailable(fmt)
# FetchContent_Declare(
#   spdlog
#   GIT_REPOSITORY https://github.com/gabime/spdlog.git
#   GIT_TAG v1.14.1
#   GIT_SHALLOW TRUE
# )
# set(SPDLOG_FMT_EXTERNAL ON)
# FetchContent_MakeAvailable(spdlog)

# FetchContent_Declare(
#   yaml-cpp
#   GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
#   GIT_TAG master # Can be a tag (yaml-cpp-x.x.x), a commit hash, or a branch name (master)
# )
# FetchContent_MakeAvailable(yaml-cpp)


find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)
add_definitions(-DSPDLOG_FMT_EXTERNAL)
find_package(spdlog REQUIRED)
find_package(ODBC REQUIRED)
if (ODBC_FOUND)
    message(STATUS "ODBC library found.")
else()
    message(FATAL_ERROR "ODBC library not found.  sudo dnf install unixODBC-devel")
endif()

add_executable(${PROJECT_NAME}-db2 src/dbgen4.cpp )
find_package(DB2 REQUIRED)
if (DB2_FOUND)
    message(STATUS "DB2 library found.")
    target_link_libraries(${PROJECT_NAME}-db2 PRIVATE DB2::DB2)
else()
    message(FATAL_ERROR "DB2 knjižnica ni najdena. Preverite DB2_HOME!")
endif()

# Add main program


foreach(SUBDIR ${SRC_SUBDIRS})
  target_include_directories(${PROJECT_NAME}-db2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
endforeach()

set (SRC_FILES 
 src/dbgen4/parser/parser.cpp 
 src/dbgen4/parser/data_statement.cpp 
 src/dbgen4/parser/data_statements.cpp 
 src/dbgen4/parser/pars_result.cpp 
  src/dbgen4/parser/sql_col_def.cpp

 src/dbgen4/parameters/cmd_line_params.cpp 

 src/dbgen4/common/build_type.cpp
 src/dbgen4/common/parser_errors.cpp
 src/dbgen4/common/common.cpp
 src/dbgen4/common/cli_constants.cpp
 
 src/log/log.cpp
 
 src/programs/appl.cpp 
)
set (SRC_RTL_FILES
 src/rtl/rtl.cpp
 src/rtl/db2/db2_rtl.cpp
)

add_library(db2_rtl STATIC ${SRC_RTL_FILES})
target_include_directories(db2_rtl 
  PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl/db2
    $ENV{IBM_DB_INCLUDE}
)

target_sources(${PROJECT_NAME}-db2 PRIVATE  ${SRC_FILES} )
# Target dependencies
target_link_libraries(${PROJECT_NAME}-db2 
  PRIVATE  
    ${DB2_LIBRARY} 
    fmt::fmt 
    yaml-cpp::yaml-cpp 
    db2_rtl 
    spdlog::spdlog 
    ODBC::ODBC
)
#target_compile_definitions(${PROJECT_NAME} PRIVATE FMT_COMPILE=1)



# tidy integration
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
if(CLANG_TIDY_EXECUTABLE)
    # Dodaj custom target za clang-tidy
    add_custom_target(
        dbgen4-tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
                -p ${CMAKE_BINARY_DIR}
                -format-style=file
                -header-filter=.*dbgen4.*
                ${SRC_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on dbgen4 source files"
    )
    add_custom_target(
        rtl-tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
                -p ${CMAKE_BINARY_DIR}
                -format-style=file
                -header-filter=.*rtl.*
                ${SRC_RTL_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on rtl library files"
    )
else()
    message(WARNING "clang-tidy not found, skipping clang-tidy target")
endif()

## Eksplicitno določi include poti za izvorne verzije
#target_include_directories(${PROJECT_NAME} PRIVATE
#  ${fmt_SOURCE_DIR}/include
#  ${spdlog_SOURCE_DIR}/include
#  # Dodaj to pred sistemskimi imeniki
#  BEFORE
#)

## Prepreči uporabo sistemskih glav
#set(CMAKE_IGNORE_PATH /usr/include/fmt /usr/include/spdlog)

#
# packaging
#
# add_subdirectory(packaging)