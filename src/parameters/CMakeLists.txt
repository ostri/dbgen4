add_library(parameters STATIC 
    parameters.cpp
)
# Public headers
target_include_directories(parameters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})