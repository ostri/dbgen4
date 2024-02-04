# Works with 3.11 and tested through 3.22
cmake_minimum_required(VERSION 3.27)

project(
  dbgen4
  VERSION 0.1.0
  DESCRIPTION "database access code genrator"
  LANGUAGES CXX
)

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
      -pthread
      # -fsanitize=address
      # -fno-omit-frame-pointer
      # -fsanitize=undefined
      # -fno-sanitize-recover=all
      # -fsanitize=float-divide-by-zero
      # -fsanitize=float-cast-overflow
      # -fno-sanitize=null
      # -fno-sanitize=alignment
      )
      add_link_options(
        # -fno-omit-frame-pointer
        # -fsanitize=address
        # -fsanitize=undefined
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

  # find_program(
  #   CLANG_TIDY_EXE
  #   NAMES "clang-tidy"
  #   DOC "Path to clang-tidy executable"
  # )
  # set(CMAKE_CXX_CLANG_TIDY "clang-tidy")


  # Testing only available if this is the main app
  # include(CTest)

  # # Docs only available if this is the main app
  # find_package(Doxygen OPTIONAL_COMPONENTS dot mscgen dia)
  # if(Doxygen_FOUND)
  #   add_subdirectory(docs)
  # else()
  #   message(STATUS "Doxygen not found, not building docs")
  # endif()
endif()


# include(FetchContent)

# find_package (gflags REQUIRED)    # sudo dnf install gflags-devel
# find_package (GLOG REQUIRED)      # sudo dnf install glog-devel
# find_package (XercesC REQUIRED)   # sudo dnf install xerces-c-devel.x86_64
# find_package (DB2 REQUIRED)

# option(DOCTEST_NO_INSTALL  "Skip the installation process" ON)
# # unit testing
# FetchContent_Declare(DocTest GIT_REPOSITORY "https://github.com/doctest/doctest.git" GIT_TAG "2.4.7")
# FetchContent_MakeAvailable(DocTest)

# # Adds fmt::fmt
# FetchContent_Declare(fmt GIT_REPOSITORY "https://github.com/fmtlib/fmt.git" GIT_TAG "8.1.1")
# FetchContent_MakeAvailable(fmt)

# message (STATUS "fmt: [${fmt_SOURCE_DIR}]")


# # static enums
# FetchContent_Declare(magic_enum GIT_REPOSITORY "https://github.com/Neargye/magic_enum.git" GIT_TAG v0.7.3)
# FetchContent_MakeAvailable(magic_enum)

# # The compiled library code is here
add_subdirectory(src)
add_subdirectory(examples)

# Add main program
add_executable(dbgen4 src/programs/dbgen4.cpp )

# Testing only available if this is the main app
# if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
#    AND BUILD_TESTING)
#   add_subdirectory(tests)
# endif()

# find_program(CCACHE_PROGRAM ccache)
# if(CCACHE_PROGRAM)
#     set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
# endif()
#
# installation
#
# include(GNUInstallDirs)
# include(CMakePackageConfigHelpers)

# write_basic_package_version_file(
#   ${CMAKE_CURRENT_BINARY_DIR}/dbgen3ConfigVersion.cmake
#   VERSION ${PROJECT_VERSION}
#   COMPATIBILITY ExactVersion
# )

# configure_package_config_file(
#     cmake/dbgen3Config.cmake.in                      # the template file
#     ${CMAKE_CURRENT_BINARY_DIR}/dbgen3Config.cmake   # destination
#     PATH_VARS CMAKE_INSTALL_INCLUDEDIR
#     INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dbgen3
# )

# install(FILES
#     ${CMAKE_CURRENT_BINARY_DIR}/dbgen3Config.cmake
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dbgen3
#     # ${CMAKE_CURRENT_BINARY_DIR}/dbgen3ConfigVersion.cmake
#     # DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dbgen3
# )
#
# packaging
#
# add_subdirectory(packaging)