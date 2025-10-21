# Works with 3.11 and tested through 3.22
cmake_minimum_required(VERSION 4.1.2)

project(
  dbgen4
  VERSION 0.1.0
  DESCRIPTION "database access code generator"
  LANGUAGES CXX
)
include(FetchContent)

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
  src/programs
  src/parser
  src/parameters
  src/common
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

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master # Can be a tag (yaml-cpp-x.x.x), a commit hash, or a branch name (master)
)
FetchContent_MakeAvailable(yaml-cpp)


find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)


# Add main program
add_executable(${PROJECT_NAME} src/dbgen4.cpp )

foreach(SUBDIR ${SRC_SUBDIRS})
  target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
endforeach()

target_sources(${PROJECT_NAME} PRIVATE
 src/programs/appl.cpp 
 src/parser/parser.cpp 
 src/parser/data_statement.cpp 
 src/parser/data_statements.cpp 
 src/parameters/parameters.cpp 
 src/common/common.cpp
 src/common/build_type.cpp
 src/common/log.cpp
)
# Target dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE  fmt::fmt yaml-cpp::yaml-cpp  )
#target_compile_definitions(${PROJECT_NAME} PRIVATE FMT_COMPILE=1)

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