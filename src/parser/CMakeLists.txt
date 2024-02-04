add_library(parser STATIC 
    a.cpp
)
# Public headers
target_include_directories(parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})