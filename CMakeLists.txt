# Works with 3.11 and tested through 3.22
cmake_minimum_required(VERSION 3.27)

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

# Docs only available if this is the main app
find_package(Doxygen OPTIONAL_COMPONENTS dot mscgen dia)
if(Doxygen_FOUND)
  add_subdirectory(docs)
else()
  message(STATUS "Doxygen not found, not building docs")
endif()

# # The compiled library code is here
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(externals)

# Add main program
add_executable(dbgen4 src/dbgen4.cpp )
# Target dependencies
target_link_libraries(dbgen4 PRIVATE fmt common parameters appl)


#
# packaging
#
# add_subdirectory(packaging)