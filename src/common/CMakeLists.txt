add_library(common STATIC 
    common.cpp
    log_dbgen4.cpp
)
# Public headers
target_include_directories(common PUBLIC 
${CMAKE_CURRENT_SOURCE_DIR} 
${magic_enum_BINARY_DIR}/include
)
message(STATUS "magic_enum ${magic_enum_BINARY_DIR}")